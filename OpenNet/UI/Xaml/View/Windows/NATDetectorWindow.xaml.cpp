#include <Windows.h>

#include "XamlWorkaround.h"
#include "NATDetectorWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/NATDetectorWindow.g.cpp")
#include "UI/Xaml/View/Windows/NATDetectorWindow.g.cpp"
#endif

import OpenNet.App;
import OpenNet.Core.P2PManager;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import OpenNet.Core.Utils.Message;
import winrt.Microsoft.UI;
import winrt.Microsoft.UI.Windowing;
import winrt.Windows.Graphics;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace
{
	winrt::hstring HumanProbeEvidence(winrt::hstring const& evidence)
	{
		if (evidence == L"tcp-connected-and-bittorrent-handshake-sent")
			return L"TCP connected; BitTorrent handshake sent";
		if (evidence == L"utp-response-received")
			return L"valid uTP response received";
		if (evidence == L"connection-refused")
			return L"connection refused";
		if (evidence == L"timeout")
			return L"no response before the traversal timeout";
		if (evidence == L"unexpected-udp-response")
			return L"UDP replied, but not with a valid uTP packet";
		return evidence.empty() ? L"no probe evidence" : evidence;
	}

	winrt::hstring FormatPortProbe(
		bool completed,
		bool reachable,
		bool timedOut,
		std::int32_t latencyMs,
		winrt::hstring const& evidence)
	{
		if (completed)
		{
			return winrt::hstring{ std::format(
				L"{} · {} ms · {}",
				reachable ? L"Open" : L"Blocked / unreachable",
				latencyMs,
				HumanProbeEvidence(evidence)) };
		}
		return timedOut
			? L"Timed out before the traversal node returned a result"
			: L"Not available / not tested";
	}

	winrt::hstring FormatFilteringObservation(
		bool tested,
		::OpenNet::Core::StunObservation const& observation)
	{
		if (!tested)
			return L"Not available (the traversal node needs a second public IPv4 address)";
		return observation.success
			? L"Received from the requested alternate endpoint"
			: L"Not received before the 3 s STUN timeout";
	}

	winrt::hstring EndpointText(::OpenNet::Core::StunObservation const& observation)
	{
		return observation.success
			? observation.mappedAddress + L":" + winrt::to_hstring(observation.mappedPort)
			: L"not observed";
	}

	winrt::hstring BuildConclusion(::OpenNet::Core::NatDetectionResult const& result)
	{
		std::wstring text = std::format(
			L"All mapping requests used the same internal UDP endpoint {}:{}. ",
			result.localIPv4.empty() ? L"—" : result.localIPv4.c_str(),
			result.localPort);
		if (!result.observations.empty())
		{
			text += L"The primary traversal endpoint observed ";
			text += EndpointText(result.observations[0]).c_str();
			text += L". ";
		}
		if (result.observations.size() > 1)
		{
			text += L"Changing only the remote port produced ";
			text += EndpointText(result.observations[1]).c_str();
			text += L". ";
		}
		if (result.observations.size() > 2)
		{
			text += L"Changing the remote address produced ";
			text += EndpointText(result.observations[2]).c_str();
			text += L". ";
		}

		switch (result.mapping)
		{
			case ::OpenNet::Core::NatMappingBehavior::Direct:
				text += L"The local and observed public IPv4 addresses match, so no IPv4 address translation was observed.";
				break;
			case ::OpenNet::Core::NatMappingBehavior::EndpointIndependent:
				text += L"The public endpoint stayed constant across remote endpoints (endpoint-independent mapping).";
				break;
			case ::OpenNet::Core::NatMappingBehavior::AddressDependent:
				text += L"The public endpoint changed when the remote IP changed, but not when only its port changed (address-dependent mapping).";
				break;
			case ::OpenNet::Core::NatMappingBehavior::AddressAndPortDependent:
				text += L"The same local socket received a different public mapping when the remote endpoint changed. This destination-dependent mapping is the key evidence behind the traditional Symmetric NAT label.";
				break;
			default:
				text += L"There was not enough STUN evidence to classify mapping behavior.";
				break;
		}
		text += L" Mapping describes how outbound traffic is translated; filtering separately describes which remote endpoints may send packets back. Neither result is a conventional port scan.";
		return winrt::hstring{ text };
	}
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	NATDetectorWindow::NATDetectorWindow()
	{
		InitializeComponent();
		InitializeWindowExBase();
		InitializeWindow();
	}

	void NATDetectorWindow::InitializeWindow()
	{
		SetTitleBar(NATDetectorWindowTitleBar());
		ExtendsContentIntoTitleBar(true);
		DirectoryUriBox().Text(m_detector.TraversalDirectoryUri());

		auto& manager = ::OpenNet::Core::P2PManager::Instance();
		if (auto* core = manager.TorrentCore(); core && core->IsRunning())
		{
			auto stats = core->GetSessionStats();
			ListenPortNumber().Value(
				stats.ipv4ListenPort > 0
				? static_cast<double>(stats.ipv4ListenPort)
				: std::numeric_limits<double>::quiet_NaN());
			IPv6ListenPortNumber().Value(
				stats.ipv6ListenPort > 0
				? static_cast<double>(stats.ipv6ListenPort)
				: std::numeric_limits<double>::quiet_NaN());
			if (!stats.isListening ||
				(stats.ipv4ListenPort <= 0 && stats.ipv6ListenPort <= 0))
			{
				DiagnosticInfoBar().Severity(Controls::InfoBarSeverity::Error);
				DiagnosticInfoBar().Message(
					stats.listenError.empty()
					? L"libtorrent did not open a listening socket."
					: winrt::to_hstring(stats.listenError));
				DiagnosticInfoBar().IsOpen(true);
			}
		}

		if (auto presenter = winrt::Microsoft::UI::Windowing::OverlappedPresenter::CreateForDialog())
		{
			// presenter.IsModal(true);
			presenter.IsResizable(true);
			AppWindow().SetPresenter(presenter);
		}
		UpdateLibtorrentState();
		Closed([this](auto const&, auto const&)
		{
			::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Save(AppWindow());
		});
	}

	void NATDetectorWindow::UpdateLibtorrentState()
	{
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (!core || !core->IsRunning())
		{
			UpnpText().Text(ResourceGetString(L"ViewNATDetectorWindowEngineNotRunning"));
			NatPmpText().Text(ResourceGetString(L"ViewNATDetectorWindowEngineNotRunning"));
			PortMappingText().Text(L"—");
			return;
		}

		auto status = core->GetPortMappingStatus();
		auto stats = core->GetSessionStats();
		ListenPortNumber().Value(
			stats.isListeningIPv4 && stats.ipv4ListenPort > 0
			? static_cast<double>(stats.ipv4ListenPort)
			: std::numeric_limits<double>::quiet_NaN());
		IPv6ListenPortNumber().Value(
			stats.isListeningIPv6 && stats.ipv6ListenPort > 0
			? static_cast<double>(stats.ipv6ListenPort)
			: std::numeric_limits<double>::quiet_NaN());
		UpnpText().Text(status.upnpEnabled ? ResourceGetString(L"CommonEnabled") : ResourceGetString(L"CommonDisabled"));
		NatPmpText().Text(status.natPmpEnabled ? ResourceGetString(L"CommonEnabled") : ResourceGetString(L"CommonDisabled"));
		if (status.tcpExternalPort > 0 || status.udpExternalPort > 0)
		{
			auto tcp = status.tcpExternalPort > 0
				? std::format(
					L"TCP {} ({})",
					status.tcpExternalPort,
					winrt::to_hstring(status.tcpMechanism))
				: L"TCP pending";
			auto udp = status.udpExternalPort > 0
				? std::format(
					L"UDP {} ({})",
					status.udpExternalPort,
					winrt::to_hstring(status.udpMechanism))
				: L"UDP pending";
			PortMappingText().Text(tcp + L"; " + udp);
		}
		else if (!status.lastError.empty())
		{
			PortMappingText().Text(winrt::to_hstring(status.lastError));
		}
		else
		{
			PortMappingText().Text(ResourceGetString(L"ViewNATDetectorWindowWaitingForRouterResponse"));
		}
	}

	void NATDetectorWindow::ResetObservationTable()
	{
		for (auto const& text : {
			Observation1Result(), Observation2Result(), Observation3Result(),
			Observation1Address(), Observation2Address(), Observation3Address(),
			Observation1Latency(), Observation2Latency(), Observation3Latency() })
		{
			text.Text(L"—");
		}
		LocalEndpointText().Text(L"—");
		PublicEndpointText().Text(L"—");
		FilteringDifferentAddressPortText().Text(L"Not tested");
		FilteringDifferentPortText().Text(L"Not tested");
		FilteringExplanationText().Text(
			L"Waiting for the full STUN behavior test.");
		ConclusionExplanationText().Text(
			L"Waiting for mapping and filtering evidence.");
	}

	void NATDetectorWindow::ShowObservation(
		std::size_t index,
		::OpenNet::Core::StunObservation const& observation)
	{
		if (index > 2)
			return;
		auto results = std::array{
			Observation1Result(), Observation2Result(), Observation3Result() };
		auto addresses = std::array{
			Observation1Address(), Observation2Address(), Observation3Address() };
		auto latencies = std::array{
			Observation1Latency(), Observation2Latency(), Observation3Latency() };

		results[index].Text(observation.success ? ResourceGetString(L"ViewNATDetectorWindowReceived") : ResourceGetString(L"ViewNATDetectorWindowNotReceived"));
		addresses[index].Text(
			observation.success
			? observation.mappedAddress + L":" + winrt::to_hstring(observation.mappedPort)
			: L"—");
		latencies[index].Text(
			observation.success
			? winrt::to_hstring(observation.latencyMs) + L" ms"
			: L"—");
	}

	void NATDetectorWindow::ShowPortProbeResult(
		::OpenNet::Core::PortProbeResult const& result)
	{
		TcpPortText().Text(FormatPortProbe(
			result.tcpCompleted,
			result.tcpReachable,
			result.ipv4TimedOut,
			result.tcpLatencyMs,
			result.tcpEvidence));
		UdpPortText().Text(FormatPortProbe(
			result.udpCompleted,
			result.udpReachable,
			result.ipv4TimedOut,
			result.udpLatencyMs,
			result.udpEvidence));
		IPv6TcpPortText().Text(FormatPortProbe(
			result.ipv6TcpCompleted,
			result.ipv6TcpReachable,
			result.ipv6TimedOut,
			result.ipv6TcpLatencyMs,
			result.ipv6TcpEvidence));
		IPv6UdpPortText().Text(FormatPortProbe(
			result.ipv6UdpCompleted,
			result.ipv6UdpReachable,
			result.ipv6TimedOut,
			result.ipv6UdpLatencyMs,
			result.ipv6UdpEvidence));
		ProbeEvidenceText().Text(
			result.completed || result.ipv6Completed
			? std::format(
				L"IPv4 TCP: {}\nIPv4 UDP: {}\nIPv6 TCP: {}\nIPv6 UDP: {}",
				HumanProbeEvidence(result.tcpEvidence),
				HumanProbeEvidence(result.udpEvidence),
				HumanProbeEvidence(result.ipv6TcpEvidence),
				HumanProbeEvidence(result.ipv6UdpEvidence))
			: (result.timedOut
			   ? L"Traversal probe timed out"
			   : L"No traversal probe response"));
	}

	winrt::Windows::Foundation::IAsyncAction NATDetectorWindow::QuickPortButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_running)
			co_return;
		auto const validPort = [](double value)
		{
			return std::isfinite(value) && value >= 1 && value <= 65535;
		};
		auto const ipv4Value = ListenPortNumber().Value();
		auto const ipv6Value = IPv6ListenPortNumber().Value();
		if (!validPort(ipv4Value) && !validPort(ipv6Value))
		{
			DiagnosticInfoBar().Severity(Controls::InfoBarSeverity::Error);
			DiagnosticInfoBar().Message(L"No active libtorrent IPv4 or IPv6 listener is available.");
			DiagnosticInfoBar().IsOpen(true);
			co_return;
		}

		auto lifetime = get_strong();
		m_running = true;
		QuickPortButton().IsEnabled(false);
		StartButton().IsEnabled(false);
		DetectingProgress().IsActive(true);
		StatusText().Text(L"Testing IPv4/IPv6 TCP and UDP reachability");
		TcpPortText().Text(L"Testing");
		UdpPortText().Text(L"Testing");
		IPv6TcpPortText().Text(L"Testing");
		IPv6UdpPortText().Text(L"Testing");
		m_detector.TraversalDirectoryUri(DirectoryUriBox().Text());
		auto result = std::make_shared<::OpenNet::Core::PortProbeResult>();
		try
		{
			co_await m_detector.TestListeningPortsAsync(
				validPort(ipv4Value) ? static_cast<std::uint16_t>(ipv4Value) : 0,
				validPort(ipv6Value) ? static_cast<std::uint16_t>(ipv6Value) : 0,
				result);
			ShowPortProbeResult(*result);
			StatusText().Text(L"Quick port test complete");
		}
		catch (...)
		{
			StatusText().Text(L"Quick port test incomplete");
			DiagnosticInfoBar().Severity(Controls::InfoBarSeverity::Warning);
			DiagnosticInfoBar().Message(L"The quick port test stopped unexpectedly.");
			DiagnosticInfoBar().IsOpen(true);
		}
		DetectingProgress().IsActive(false);
		QuickPortButton().IsEnabled(true);
		StartButton().IsEnabled(true);
		m_running = false;
	}

	winrt::Windows::Foundation::IAsyncAction NATDetectorWindow::StartButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_running)
			co_return;

		double ipv4PortValue = ListenPortNumber().Value();
		double ipv6PortValue = IPv6ListenPortNumber().Value();
		auto* torrentCore = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (!torrentCore || !torrentCore->IsRunning())
		{
			DiagnosticInfoBar().Severity(Controls::InfoBarSeverity::Error);
			DiagnosticInfoBar().Message(ResourceGetString(L"ViewNATDetectorWindowStartEngineBeforeTesting"));
			DiagnosticInfoBar().IsOpen(true);
			co_return;
		}
		auto sessionStats = torrentCore->GetSessionStats();
		auto const validPort = [](double value)
		{
			return std::isfinite(value) && value >= 1 && value <= 65535;
		};
		if (!validPort(ipv4PortValue) && !validPort(ipv6PortValue))
		{
			DiagnosticInfoBar().Severity(Controls::InfoBarSeverity::Error);
			DiagnosticInfoBar().Message(
				sessionStats.listenError.empty()
				? L"libtorrent is not listening. Set an available port in BitTorrent settings."
				: winrt::to_hstring(sessionStats.listenError));
			DiagnosticInfoBar().IsOpen(true);
			co_return;
		}

		auto lifetime = get_strong();
		m_running = true;
		StartButton().IsEnabled(false);
		QuickPortButton().IsEnabled(false);
		DetectingProgress().IsActive(true);
		StatusText().Text(ResourceGetString(L"ViewNATDetectorWindowDetectingNAT"));
		DiagnosticInfoBar().IsOpen(false);
		ResetObservationTable();
		MappingText().Text(ResourceGetString(L"CommonDetecting"));
		FilteringText().Text(ResourceGetString(L"CommonDetecting"));
		NatTypeText().Text(ResourceGetString(L"CommonDetecting"));
		TcpPortText().Text(ResourceGetString(L"CommonTesting"));
		UdpPortText().Text(ResourceGetString(L"CommonTesting"));
		IPv6TcpPortText().Text(ResourceGetString(L"CommonTesting"));
		IPv6UdpPortText().Text(ResourceGetString(L"CommonTesting"));
		ProbeEvidenceText().Text(ResourceGetString(L"ViewNATDetectorWindowWaitingForTraversalServer"));

		// Port/NAT diagnostics must be observational. Reopening libtorrent's
		// sockets here used to reset the listening port and the DHT UDP channel,
		// disrupting active torrents and magnet metadata discovery.
		m_detector.TraversalDirectoryUri(DirectoryUriBox().Text());
		auto result = std::make_shared<::OpenNet::Core::NatDetectionResult>();
		try
		{
			co_await m_detector.DetectNATBehaviorAsync(
				validPort(ipv4PortValue) ? static_cast<uint16_t>(ipv4PortValue) : 0,
				validPort(ipv6PortValue) ? static_cast<uint16_t>(ipv6PortValue) : 0,
				result);
		}
		catch (winrt::hresult_error const& error)
		{
			DiagnosticInfoBar().Severity(Controls::InfoBarSeverity::Warning);
			DiagnosticInfoBar().Message(
				error.code() == HRESULT_FROM_WIN32(ERROR_CANCELLED)
				? L"Detection was cancelled."
				: L"Detection stopped: " + error.message());
			DiagnosticInfoBar().IsOpen(true);
			StatusText().Text(L"Detection incomplete");
			DetectingProgress().IsActive(false);
			StartButton().IsEnabled(true);
			QuickPortButton().IsEnabled(true);
			m_running = false;
			co_return;
		}
		catch (...)
		{
			DiagnosticInfoBar().Severity(Controls::InfoBarSeverity::Warning);
			DiagnosticInfoBar().Message(L"Detection stopped unexpectedly.");
			DiagnosticInfoBar().IsOpen(true);
			StatusText().Text(L"Detection incomplete");
			DetectingProgress().IsActive(false);
			StartButton().IsEnabled(true);
			QuickPortButton().IsEnabled(true);
			m_running = false;
			co_return;
		}

		LocalIPv4Text().Text(result->localIPv4.empty() ? L"—" : result->localIPv4);
		LocalEndpointText().Text(
			result->localIPv4.empty() || result->localPort == 0
			? L"—"
			: result->localIPv4 + L":" + winrt::to_hstring(result->localPort));
		PublicIPv4Text().Text(result->publicIPv4.empty() ? L"—" : result->publicIPv4);
		PublicEndpointText().Text(
			result->observations.empty()
			? L"—"
			: EndpointText(result->observations.front()));
		MappingText().Text(::OpenNet::Core::NetworkDetector::MappingBehaviorToString(result->mapping));
		FilteringText().Text(::OpenNet::Core::NetworkDetector::FilteringBehaviorToString(result->filtering));
		NatTypeText().Text(::OpenNet::Core::NetworkDetector::NATTypeToString(result->legacyType));
		ShowPortProbeResult(result->portProbe);

		for (std::size_t index = 0; index < result->observations.size() && index < 3; ++index)
			ShowObservation(index, result->observations[index]);

		FilteringDifferentAddressPortText().Text(FormatFilteringObservation(
			result->filteringDifferentAddressAndPortTested,
			result->filteringDifferentAddressAndPort));
		FilteringDifferentPortText().Text(FormatFilteringObservation(
			result->filteringDifferentPortTested,
			result->filteringDifferentPort));
		switch (result->filtering)
		{
			case ::OpenNet::Core::NatFilteringBehavior::EndpointIndependent:
				FilteringExplanationText().Text(
					L"A reply from a different public IP and port was received. Once the mapping exists, inbound filtering is endpoint-independent.");
				break;
			case ::OpenNet::Core::NatFilteringBehavior::AddressDependent:
				FilteringExplanationText().Text(
					L"A different IP was blocked, while the same IP using a different source port was accepted. Filtering depends on the remote address.");
				break;
			case ::OpenNet::Core::NatFilteringBehavior::AddressAndPortDependent:
				FilteringExplanationText().Text(
					L"Replies from both a different IP+port and the same IP with a different port were not received. Filtering depends on the full remote endpoint.");
				break;
			default:
				FilteringExplanationText().Text(
					L"Filtering behavior could not be classified. A traversal node with two public IPv4 addresses is required for the complete test.");
				break;
		}
		ConclusionExplanationText().Text(BuildConclusion(*result));

		if (!result->diagnostic.empty())
		{
			DiagnosticInfoBar().Severity(
				result->completed
				? Controls::InfoBarSeverity::Informational
				: Controls::InfoBarSeverity::Warning);
			DiagnosticInfoBar().Message(result->diagnostic);
			DiagnosticInfoBar().IsOpen(true);
		}

		UpdateLibtorrentState();
		StatusText().Text(result->completed ? ResourceGetString(L"ViewNATDetectorWindowDetectionComplete") : ResourceGetString(L"ViewNATDetectorWindowDetectionIncomplete"));
		DetectingProgress().IsActive(false);
		StartButton().IsEnabled(true);
		QuickPortButton().IsEnabled(true);
		m_running = false;
	}

	void NATDetectorWindow::CloseButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		Close();
	}
}
