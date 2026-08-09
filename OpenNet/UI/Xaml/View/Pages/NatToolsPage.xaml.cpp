#include "XamlWorkaround.h"
#include "NatToolsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/NatToolsPage.g.cpp")
#include "UI/Xaml/View/Pages/NatToolsPage.g.cpp"
#endif

import OpenNet.Core.P2PManager;
import OpenNet.Core.TorrentSettings;
import OpenNet.Core.Utils.Message;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Dispatching;
import winrtplus_coroutine;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	NatToolsPage::NatToolsPage()
	{
		InitializeComponent();

		// Populate STUN servers list
		auto servers = m_detector.GetRecommendedSTUNServers();
		auto items = winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();
		for (uint32_t i = 0; i < servers.Size(); ++i)
		{
			items.Append(winrt::box_value(servers.GetAt(i)));
		}
		StunServersList().ItemsSource(items);

		// Show libtorrent status on load
		UpdateLibtorrentStatus();

		// Set up auto-refresh timer (5 seconds)
		m_refreshTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
		m_refreshTimer.Interval(std::chrono::seconds(5));
		m_timerToken = m_refreshTimer.Tick([weak = get_weak()](auto&&, auto&&)
		{
			if (auto strong = weak.get())
			{
				strong->UpdateLibtorrentStatus();
			}
		});
		m_refreshTimer.Start();
	}

	NatToolsPage::~NatToolsPage()
	{
		if (m_refreshTimer)
		{
			m_refreshTimer.Stop();
			m_refreshTimer.Tick(m_timerToken);
		}
	}

	void NatToolsPage::UpdateLibtorrentStatus()
	{
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (core && core->IsRunning())
		{
			auto stats = core->GetSessionStats();
			ListenPortText().Text(winrt::to_hstring(stats.listenPort));
			DhtNodesText().Text(winrt::to_hstring(stats.dhtNodes));
			ActiveTorrentsText().Text(winrt::to_hstring(stats.numTorrents));
			ConnectedPeersText().Text(winrt::to_hstring(stats.numPeers));
		}
		else
		{
			ListenPortText().Text(ResourceGetString(L"ViewNatToolsPageNACoreNotRunning"));
			DhtNodesText().Text(ResourceGetString(L"ViewNatToolsPageNA"));
			ActiveTorrentsText().Text(ResourceGetString(L"ViewNatToolsPageNA"));
			ConnectedPeersText().Text(ResourceGetString(L"ViewNatToolsPageNA"));
		}

		// UPnP/NAT-PMP enabled status from settings
		auto& mgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
		mgr.Load();
		auto s = mgr.Get();
		UPnPText().Text(s.enableUpnp ? ResourceGetString(L"CommonEnabled") : ResourceGetString(L"CommonDisabled"));
		NatPmpText().Text(s.enableNatpmp ? ResourceGetString(L"CommonEnabled") : ResourceGetString(L"CommonDisabled"));

		// Network type
		auto netType = m_detector.GetNetworkType();
		NetworkTypeText().Text(::OpenNet::Core::NetworkDetector::NetworkTypeToString(netType));
	}

	// Perform a single STUN binding request and parse the response to extract mapped address
	IAsyncAction NatToolsPage::PerformStunTest(
		winrt::hstring const& server, uint16_t port, std::shared_ptr<StunTestResult> outResult)
	{
		auto& result = *outResult;
		try
		{
			// Parse server:port
			winrt::hstring host;
			winrt::hstring portStr;
			std::wstring_view sv{ server.c_str() };
			auto colonPos = sv.find(L':');
			if (colonPos != std::wstring_view::npos)
			{
				host = winrt::hstring{ sv.substr(0, colonPos) };
				portStr = winrt::hstring{ sv.substr(colonPos + 1) };
			}
			else
			{
				host = server;
				portStr = winrt::to_hstring(port);
			}

			DatagramSocket socket;
			auto start = std::chrono::steady_clock::now();

			co_await socket.ConnectAsync(HostName(host), portStr);

			// Build STUN Binding Request (RFC 5389)
			// Type=0x0001, Length=0x0000, Magic=0x2112A442, TransactionId=12 random bytes
			uint8_t txId[12];
			for (int i = 0; i < 12; ++i) txId[i] = static_cast<uint8_t>(rand() & 0xFF);

			DataWriter writer(socket.OutputStream());
			writer.WriteByte(0x00); writer.WriteByte(0x01); // Type: Binding Request
			writer.WriteByte(0x00); writer.WriteByte(0x00); // Length: 0
			writer.WriteByte(0x21); writer.WriteByte(0x12); // Magic cookie
			writer.WriteByte(0xA4); writer.WriteByte(0x42);
			writer.WriteBytes(winrt::array_view<const uint8_t>(txId, 12));
			co_await writer.StoreAsync();

			// Wait for response with timeout
			winrt::Windows::Foundation::IAsyncAction recvAction{ nullptr };
			bool gotResponse = false;
			std::vector<uint8_t> responseData;

			// Register message received handler
			socket.MessageReceived([&](DatagramSocket const&, DatagramSocketMessageReceivedEventArgs const& args)
			{
				try
				{
					auto reader = args.GetDataReader();
					auto len = reader.UnconsumedBufferLength();
					if (len >= 20) // Minimum STUN header
					{
						responseData.resize(len);
						reader.ReadBytes(winrt::array_view<uint8_t>(responseData));
						gotResponse = true;
					}
				}
				catch (...)
				{
				}
			});

			// Wait up to 3 seconds for response
			for (int i = 0; i < 30 && !gotResponse; ++i)
			{
				co_await winrt::resume_after(std::chrono::milliseconds(100));
			}

			auto end = std::chrono::steady_clock::now();
			result.latencyMs = static_cast<int>(
				std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

			if (gotResponse && responseData.size() >= 20)
			{
				// Verify STUN response: type should be 0x0101 (Binding Success)
				uint16_t msgType = (responseData[0] << 8) | responseData[1];
				uint16_t msgLen = (responseData[2] << 8) | responseData[3];

				if (msgType == 0x0101 && responseData.size() >= static_cast<size_t>(20 + msgLen))
				{
					// Parse attributes looking for MAPPED-ADDRESS (0x0001) or XOR-MAPPED-ADDRESS (0x0020)
					size_t offset = 20;
					while (offset + 4 <= 20 + msgLen)
					{
						uint16_t attrType = (responseData[offset] << 8) | responseData[offset + 1];
						uint16_t attrLen = (responseData[offset + 2] << 8) | responseData[offset + 3];
						size_t valueStart = offset + 4;

						if ((attrType == 0x0020 || attrType == 0x0001) && attrLen >= 8)
						{
							uint8_t family = responseData[valueStart + 1]; // IPv4=0x01
							if (family == 0x01)
							{
								uint16_t mappedPort = (responseData[valueStart + 2] << 8) | responseData[valueStart + 3];
								uint32_t mappedIP = (responseData[valueStart + 4] << 24) | (responseData[valueStart + 5] << 16) |
									(responseData[valueStart + 6] << 8) | responseData[valueStart + 7];

								// XOR-MAPPED-ADDRESS: XOR with magic cookie
								if (attrType == 0x0020)
								{
									mappedPort ^= 0x2112;
									mappedIP ^= 0x2112A442;
								}

								wchar_t buf[64];
								swprintf(buf, 64, L"%u.%u.%u.%u:%u",
										 (mappedIP >> 24) & 0xFF, (mappedIP >> 16) & 0xFF,
										 (mappedIP >> 8) & 0xFF, mappedIP & 0xFF,
										 mappedPort);
								result.mappedAddress = buf;
								result.success = true;
								break;
							}
						}

						// Advance to next attribute (padded to 4 bytes)
						offset = valueStart + ((attrLen + 3) & ~3);
					}
				}
			}

			socket.Close();
		}
		catch (...)
		{
			result.success = false;
		}
		co_return;
	}

	winrt::Windows::Foundation::IAsyncAction NatToolsPage::DetectNat_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto dispatcher = this->DispatcherQueue();

		DetectNatButton().IsEnabled(false);
		DetectingProgress().IsActive(true);
		DetectingProgress().Visibility(Visibility::Visible);
		NatTypeText().Text(ResourceGetString(L"ViewNatToolsPageDetecting"));
		PublicIPText().Text(ResourceGetString(L"ViewNatToolsPage"));
		PublicIPv6Text().Text(ResourceGetString(L"ViewNatToolsPage"));

		// Reset table
		StunTest1_1().Text(ResourceGetString(L"ViewNatToolsPageTesting"));
		StunTest1_2().Text(ResourceGetString(L"ViewNatToolsPageTesting"));
		StunTest1_3().Text(ResourceGetString(L"ViewNatToolsPageTesting"));
		StunIP1_1().Text(ResourceGetString(L"ViewNatToolsPage"));
		StunIP1_2().Text(ResourceGetString(L"ViewNatToolsPage"));
		StunIP1_3().Text(ResourceGetString(L"ViewNatToolsPage"));
		StunLatency1_1().Text(ResourceGetString(L"ViewNatToolsPage"));
		StunLatency1_2().Text(ResourceGetString(L"ViewNatToolsPage"));
		StunLatency1_3().Text(ResourceGetString(L"ViewNatToolsPage"));
		StunMapping().Text(ResourceGetString(L"ViewNatToolsPage"));

		bool detectError = false;
		try
		{
			// We'll use 2 different STUN servers for comparison
			// Server 1 = stun.l.google.com:19302 (same port test)
			// Server 1 alternate = stun.l.google.com:3478 (different port test, if available)
			// Server 2 = stun.cloudflare.com:3478 (different server, same port)

			auto server1 = winrt::hstring{ L"stun.l.google.com:19302" };
			auto server1alt = winrt::hstring{ L"stun1.l.google.com:19302" };  // Different IP, same port
			auto server2 = winrt::hstring{ L"stun.cloudflare.com:3478" };

			// Test 1: Server 1, same port
			auto r1 = std::make_shared<StunTestResult>();
			co_await PerformStunTest(server1, 19302, r1);
			co_await winrtplus::resume_foreground(dispatcher);
			StunTest1_1().Text(r1->success ? ResourceGetString(L"CommonOk") : ResourceGetString(L"TaskStatusFailed"));
			StunIP1_1().Text(r1->success ? r1->mappedAddress : L"N/A");
			StunLatency1_1().Text(winrt::to_hstring(r1->latencyMs) + L" ms");

			// Test 2: Server 1, alt port (simulating different port)
			auto r2 = std::make_shared<StunTestResult>();
			co_await PerformStunTest(server1alt, 19302, r2);
			co_await winrtplus::resume_foreground(dispatcher);
			StunTest1_2().Text(r2->success ? ResourceGetString(L"CommonOk") : ResourceGetString(L"TaskStatusFailed"));
			StunIP1_2().Text(r2->success ? r2->mappedAddress : L"N/A");
			StunLatency1_2().Text(winrt::to_hstring(r2->latencyMs) + L" ms");

			// Test 3: Server 2, same port
			auto r3 = std::make_shared<StunTestResult>();
			co_await PerformStunTest(server2, 3478, r3);
			co_await winrtplus::resume_foreground(dispatcher);
			StunTest1_3().Text(r3->success ? ResourceGetString(L"CommonOk") : ResourceGetString(L"TaskStatusFailed"));
			StunIP1_3().Text(r3->success ? r3->mappedAddress : L"N/A");
			StunLatency1_3().Text(winrt::to_hstring(r3->latencyMs) + L" ms");

			// Determine NAT type from results
			co_await winrtplus::resume_foreground(dispatcher);

			if (r1->success && r2->success && r3->success)
			{
				// Extract just the IP:port for comparison
				bool sameMapping = (r1->mappedAddress == r2->mappedAddress) && (r1->mappedAddress == r3->mappedAddress);
				bool sameIP12 = false;
				bool sameIP13 = false;

				// Compare just IP parts (before the colon)
				auto getIP = [](winrt::hstring const& addr) -> std::wstring
				{
					std::wstring s{ addr.c_str() };
					auto pos = s.rfind(L':');
					return pos != std::wstring::npos ? s.substr(0, pos) : s;
				};

				sameIP12 = (getIP(r1->mappedAddress) == getIP(r2->mappedAddress));
				sameIP13 = (getIP(r1->mappedAddress) == getIP(r3->mappedAddress));

				if (sameMapping)
				{
					// Same IP:port across all tests → Full Cone or Open
					StunMapping().Text(ResourceGetString(L"ViewNatToolsPageYesConsistentMappingAcrossAllServers"));
					NatTypeText().Text(ResourceGetString(L"ViewNatToolsPageFullConeEIM"));
				}
				else if (sameIP12 && sameIP13)
				{
					// Same IP but different ports → Address-dependent or Port-restricted
					StunMapping().Text(ResourceGetString(L"ViewNatToolsPageNoSameIPButDifferentPorts"));
					NatTypeText().Text(ResourceGetString(L"ViewNatToolsPagePortRestrictedSymmetric"));
				}
				else
				{
					// Different IPs → Symmetric NAT
					StunMapping().Text(ResourceGetString(L"ViewNatToolsPageNoDifferentIPPortPerDestination"));
					NatTypeText().Text(ResourceGetString(L"ViewNatToolsPageSymmetric"));
				}
			}
			else if (r1->success)
			{
				StunMapping().Text(ResourceGetString(L"ViewNatToolsPagePartialOnlyOneServerResponded"));
				NatTypeText().Text(ResourceGetString(L"ViewNatToolsPageUnknownPartialResults"));
			}
			else
			{
				StunMapping().Text(ResourceGetString(L"ViewNatToolsPageFailedNoSTUNResponses"));
				NatTypeText().Text(ResourceGetString(L"ViewNatToolsPageUnknown"));
			}

			// Public IP via HTTP
			auto publicIP = co_await m_detector.GetPublicIPAddressAsync();
			co_await winrtplus::resume_foreground(dispatcher);
			PublicIPText().Text(publicIP.empty() ? L"N/A" : publicIP);

			// IPv6 public IP
			try
			{
				co_await winrt::resume_background();
				auto publicIPv6 = co_await m_detector.GetPublicIPAddressAsync(true);
				co_await winrtplus::resume_foreground(dispatcher);
				PublicIPv6Text().Text(publicIPv6.empty() ? L"N/A" : publicIPv6);
			}
			catch (...)
			{
				[dispatcher, this]() -> winrt::fire_and_forget
				{
					co_await winrtplus::resume_foreground(dispatcher);
					PublicIPv6Text().Text(ResourceGetString(L"ViewNatToolsPageNA"));
				}();
			}

			// Update libtorrent status
			UpdateLibtorrentStatus();
		}
		catch (...)
		{
			detectError = true;
		}

		if (detectError)
		{
			co_await winrtplus::resume_foreground(dispatcher);
			NatTypeText().Text(ResourceGetString(L"ViewNatToolsPageError"));
			PublicIPText().Text(ResourceGetString(L"ViewNatToolsPageError"));
			PublicIPv6Text().Text(ResourceGetString(L"ViewNatToolsPageError"));
		}

		DetectNatButton().IsEnabled(true);
		DetectingProgress().IsActive(false);
		DetectingProgress().Visibility(Visibility::Collapsed);
	}

	winrt::Windows::Foundation::IAsyncAction NatToolsPage::TestPort_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto dispatcher = this->DispatcherQueue();

		auto portValue = PortNumberBox().Value();
		if (std::isnan(portValue) || portValue < 1 || portValue > 65535)
		{
			PortResultText().Text(ResourceGetString(L"ViewNatToolsPageInvalidPortNumber"));
			co_return;
		}

		uint16_t port = static_cast<uint16_t>(portValue);

		TestPortButton().IsEnabled(false);
		PortTestProgress().IsActive(true);
		PortTestProgress().Visibility(Visibility::Visible);
		PortResultText().Text(ResourceGetString(L"ViewNatToolsPageTesting"));
		PortMappedText().Text(L"");

		bool portError = false;
		try
		{
			auto result = co_await m_detector.TestPortAccessibilityAsync(port, true);
			co_await winrtplus::resume_foreground(dispatcher);

			if (result)
			{
				PortResultText().Text(L"Port " + winrt::to_hstring(port) + L" is OPEN (mapped port matches)");
				PortMappedText().Text(ResourceGetString(L"ViewNatToolsPageSTUNReportsYourExternalPortMatchesTheLocalPortPeersShouldBeAbleToConnectToYouDirectly"));
			}
			else
			{
				PortResultText().Text(L"Port " + winrt::to_hstring(port) + L" appears CLOSED or remapped");
				PortMappedText().Text(ResourceGetString(L"ViewNatToolsPageSTUNMappedPortDiffersFromLocalPortOrNoResponseReceivedYourNATMayBeRewritingThePortConsiderEnablingUpnpOrSettingUpPortForwarding"));
			}
		}
		catch (...)
		{
			portError = true;
		}

		if (portError)
		{
			co_await winrtplus::resume_foreground(dispatcher);
			PortResultText().Text(ResourceGetString(L"ViewNatToolsPageErrorTestingPort"));
			PortMappedText().Text(ResourceGetString(L"ViewNatToolsPageCouldNotCompleteTheSTUNBasedPortTestCheckYourNetworkConnection"));
		}

		TestPortButton().IsEnabled(true);
		PortTestProgress().IsActive(false);
		PortTestProgress().Visibility(Visibility::Collapsed);
	}
}
