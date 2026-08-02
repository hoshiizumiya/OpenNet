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
import winrt.Microsoft.UI;
import winrt.Microsoft.UI.Windowing;
import winrt.Windows.Graphics;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	NATDetectorWindow::NATDetectorWindow()
	{
		InitializeComponent();
		InitializeWindow();
	}

	void NATDetectorWindow::InitializeWindow()
	{
		SetTitleBar(NATDetectorWindowTitleBar());
		ExtendsContentIntoTitleBar(true);
		::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Enable(*this);
		DirectoryUriBox().Text(m_detector.TraversalDirectoryUri());

		auto& manager = ::OpenNet::Core::P2PManager::Instance();
		if (auto* core = manager.TorrentCore(); core && core->IsRunning())
		{
			auto stats = core->GetSessionStats();
			if (stats.isListening && stats.listenPort > 0)
				ListenPortNumber().Value(stats.listenPort);
			else
			{
				ListenPortNumber().Value(std::numeric_limits<double>::quiet_NaN());
				DiagnosticInfoBar().Severity(Controls::InfoBarSeverity::Error);
				DiagnosticInfoBar().Message(
					stats.listenError.empty()
						? L"libtorrent did not open a listening socket."
						: winrt::to_hstring(stats.listenError));
				DiagnosticInfoBar().IsOpen(true);
			}
		}

		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(*this);
		::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(*this);
		if (auto presenter = winrt::Microsoft::UI::Windowing::OverlappedPresenter::CreateForDialog())
		{
			// presenter.IsModal(true);
			presenter.IsResizable(true);
			AppWindow().SetPresenter(presenter);
		}
		UpdateLibtorrentState();
		Closed([this](auto const&, auto const&)
		{
			::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Save(*this);
		});
	}

	void NATDetectorWindow::UpdateLibtorrentState()
	{
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (!core || !core->IsRunning())
		{
			UpnpText().Text(L"Engine not running");
			NatPmpText().Text(L"Engine not running");
			PortMappingText().Text(L"—");
			return;
		}

		auto status = core->GetPortMappingStatus();
		auto stats = core->GetSessionStats();
		ListenPortNumber().Value(
			stats.isListening && stats.listenPort > 0
				? static_cast<double>(stats.listenPort)
				: std::numeric_limits<double>::quiet_NaN());
		UpnpText().Text(status.upnpEnabled ? L"Enabled" : L"Disabled");
		NatPmpText().Text(status.natPmpEnabled ? L"Enabled" : L"Disabled");
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
			PortMappingText().Text(L"Waiting for router response");
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

		results[index].Text(observation.success ? L"Received" : L"Not received");
		addresses[index].Text(
			observation.success
			? observation.mappedAddress + L":" + winrt::to_hstring(observation.mappedPort)
			: L"—");
		latencies[index].Text(
			observation.success
			? winrt::to_hstring(observation.latencyMs) + L" ms"
			: L"—");
	}

	winrt::Windows::Foundation::IAsyncAction NATDetectorWindow::StartButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_running)
			co_return;

		double portValue = ListenPortNumber().Value();
		auto* torrentCore = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (!torrentCore || !torrentCore->IsRunning())
		{
			DiagnosticInfoBar().Severity(Controls::InfoBarSeverity::Error);
			DiagnosticInfoBar().Message(L"Start the BitTorrent engine before testing its port.");
			DiagnosticInfoBar().IsOpen(true);
			co_return;
		}
		auto sessionStats = torrentCore->GetSessionStats();
		if (std::isnan(portValue) || portValue < 1 || portValue > 65535)
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
		DetectingProgress().IsActive(true);
		StatusText().Text(L"Detecting NAT behavior and inbound reachability…");
		DiagnosticInfoBar().IsOpen(false);
		ResetObservationTable();
		MappingText().Text(L"Detecting…");
		FilteringText().Text(L"Detecting…");
		NatTypeText().Text(L"Detecting…");
		TcpPortText().Text(L"Testing…");
		UdpPortText().Text(L"Testing…");
		ProbeEvidenceText().Text(L"Waiting for traversal server…");

		if (torrentCore && torrentCore->IsRunning())
		{
			torrentCore->RefreshPortMappings();
		}
		m_detector.TraversalDirectoryUri(DirectoryUriBox().Text());
		auto result = std::make_shared<::OpenNet::Core::NatDetectionResult>();
		co_await m_detector.DetectNATBehaviorAsync(
			static_cast<uint16_t>(portValue),
			result);

		LocalIPv4Text().Text(result->localIPv4.empty() ? L"—" : result->localIPv4);
		PublicIPv4Text().Text(result->publicIPv4.empty() ? L"—" : result->publicIPv4);
		MappingText().Text(::OpenNet::Core::NetworkDetector::MappingBehaviorToString(result->mapping));
		FilteringText().Text(::OpenNet::Core::NetworkDetector::FilteringBehaviorToString(result->filtering));
		NatTypeText().Text(::OpenNet::Core::NetworkDetector::NATTypeToString(result->legacyType));
		TcpPortText().Text(
			result->portProbe.tcpCompleted
			? (result->portProbe.tcpReachable ? L"Open" : L"Blocked / unreachable")
			: L"Not tested");
		UdpPortText().Text(
			result->portProbe.udpCompleted
			? (result->portProbe.udpReachable ? L"Open" : L"Blocked / unreachable")
			: L"Not tested");
		IPv6PortText().Text(
			result->portProbe.ipv6Completed
			? std::format(
				L"TCP: {}; UDP: {}",
				result->portProbe.ipv6TcpCompleted
				? (result->portProbe.ipv6TcpReachable ? L"Open" : L"Blocked")
				: L"Not tested",
				result->portProbe.ipv6UdpCompleted
				? (result->portProbe.ipv6UdpReachable ? L"Open" : L"Blocked")
					: L"Not tested")
				: L"Not available");
		ProbeEvidenceText().Text(
			result->portProbe.completed || result->portProbe.ipv6Completed
				? std::format(
					L"TCP: {}; UDP: {}",
					result->portProbe.tcpEvidence.empty()
						? L"no response"
						: result->portProbe.tcpEvidence,
					result->portProbe.udpEvidence.empty()
						? L"no response"
						: result->portProbe.udpEvidence)
				: L"No traversal probe response");

		for (std::size_t index = 0; index < result->observations.size() && index < 3; ++index)
			ShowObservation(index, result->observations[index]);

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
		StatusText().Text(result->completed ? L"Detection complete" : L"Detection incomplete");
		DetectingProgress().IsActive(false);
		StartButton().IsEnabled(true);
		m_running = false;
	}

	void NATDetectorWindow::CloseButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		Close();
	}
}
