#include "pch.h"
#include "NetworkDetector.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Networking;
using namespace Windows::Networking::Connectivity;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Web::Http;

namespace OpenNet::Core
{
	NetworkDetector::NetworkDetector() : m_isDetecting(false)
	{
		// 初始化推荐的STUN服务器列表 / Initialize recommended STUN server list
		m_stunServers = winrt::single_threaded_vector<winrt::hstring>();
		// Google STUN servers
		m_stunServers.Append(L"stun.l.google.com:19302");
		m_stunServers.Append(L"stun1.l.google.com:19302");
		m_stunServers.Append(L"stun2.l.google.com:19302");
		m_stunServers.Append(L"stun3.l.google.com:19302");
		m_stunServers.Append(L"stun4.l.google.com:19302");
		// https://turn.cloudflare.com/
		m_stunServers.Append(L"stun.cloudflare.com:3478");
		// https://senzyo.net/2024-7/
		m_stunServers.Append(L"stun.miwifi.com:3478");
		m_stunServers.Append(L"stun.chat.bilibili.com:3478");

		StartNetworkMonitoring();
	}

	NetworkDetector::~NetworkDetector()
	{
		StopNetworkMonitoring();
	}

	IAsyncOperation<bool> NetworkDetector::DetectNetworkEnvironmentAsync()
	{
		if (m_isDetecting)
		{
			co_return false;
		}

		m_isDetecting = true;

		try
		{
			// 简化的网络检测 / Simplified network detection
			auto connectionProfiles = NetworkInformation::GetConnectionProfiles();
			bool hasConnection = false;

			for (auto const& profile : connectionProfiles)
			{
				if (profile.GetNetworkConnectivityLevel() != NetworkConnectivityLevel::None)
				{
					hasConnection = true;
					break;
				}
			}

			m_isDetecting = false;
			co_return hasConnection;
		}
		catch (...)
		{
			m_isDetecting = false;
			co_return false;
		}
	}

	IAsyncOperation<winrt::hstring> NetworkDetector::GetNetworkInfoAsync()
	{
		try
		{
			auto connectionProfiles = NetworkInformation::GetConnectionProfiles();
			winrt::hstring info = L"网络接口信息 / Network Interface Information:\n";

			for (auto const& profile : connectionProfiles)
			{
				if (profile.GetNetworkConnectivityLevel() != NetworkConnectivityLevel::None)
				{
					auto adapter = profile.NetworkAdapter();
					if (adapter)
					{
						info = info + L"适配器: " + winrt::to_hstring(adapter.NetworkAdapterId()) + L"\n";

						switch (profile.GetNetworkConnectivityLevel())
						{
							case NetworkConnectivityLevel::LocalAccess:
								info = info + L"状态: 本地访问 / Local Access\n";
								break;
							case NetworkConnectivityLevel::ConstrainedInternetAccess:
								info = info + L"状态: 受限网络访问 / Constrained Internet Access\n";
								break;
							case NetworkConnectivityLevel::InternetAccess:
								info = info + L"状态: 完全网络访问 / Full Internet Access\n";
								break;
							default:
								break;
						}
					}
				}
			}

			co_return info;
		}
		catch (...)
		{
			co_return L"获取网络信息失败 / Failed to get network information";
		}
	}

	IAsyncOperation<int32_t> NetworkDetector::DetectNATTypeAsync()
	{
		try
		{
			// 简化的NAT类型检测 / Simplified NAT type detection
			for (uint32_t i = 0; i < m_stunServers.Size(); ++i)
			{
				auto const stunServer = m_stunServers.GetAt(i);
				try
				{
					auto result = co_await SendSTUNBindingRequestAsync(stunServer, 3478);
					if (!result.empty())
					{
						// 简化的NAT类型判断 / Simplified NAT type determination
						co_return static_cast<int32_t>(NATType::FullCone);
					}
				}
				catch (...)
				{
					// 尝试下一个STUN服务器 / Try next STUN server
					continue;
				}
			}

			co_return static_cast<int32_t>(NATType::Unknown);
		}
		catch (...)
		{
			co_return static_cast<int32_t>(NATType::Unknown);
		}
	}

	IAsyncOperation<bool> NetworkDetector::CheckUPnPAvailabilityAsync()
	{
		try
		{
			// Send SSDP M-SEARCH to discover UPnP Internet Gateway Devices
			DatagramSocket socket;

			bool found = false;
			socket.MessageReceived([&](DatagramSocket const&, DatagramSocketMessageReceivedEventArgs const& args)
			{
				try
				{
					auto reader = args.GetDataReader();
					auto len = reader.UnconsumedBufferLength();
					if (len > 0)
					{
						std::vector<uint8_t> data(len);
						reader.ReadBytes(winrt::array_view<uint8_t>(data));
						std::string response(data.begin(), data.end());
						// Check for WANIPConnection or WANPPPConnection in response
						if (response.find("WANIPConnection") != std::string::npos ||
							response.find("WANPPPConnection") != std::string::npos ||
							response.find("InternetGatewayDevice") != std::string::npos)
						{
							found = true;
						}
					}
				}
				catch (...) {}
			});

			co_await socket.BindServiceNameAsync(L"");

			// SSDP M-SEARCH message for Internet Gateway Device
			std::string msearch =
				"M-SEARCH * HTTP/1.1\r\n"
				"HOST: 239.255.255.250:1900\r\n"
				"MAN: \"ssdp:discover\"\r\n"
				"MX: 3\r\n"
				"ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
				"\r\n";

			auto remoteHost = HostName(L"239.255.255.250");
			auto remotePort = L"1900";

			DataWriter writer;
			writer.WriteBytes(winrt::array_view<const uint8_t>(
				reinterpret_cast<const uint8_t*>(msearch.data()), static_cast<uint32_t>(msearch.size())));
			auto buf = writer.DetachBuffer();
			auto outputStream = co_await socket.GetOutputStreamAsync(remoteHost, remotePort);
			co_await outputStream.WriteAsync(buf);

			// Wait up to 3 seconds for SSDP responses
			for (int i = 0; i < 30 && !found; ++i)
				co_await winrt::resume_after(std::chrono::milliseconds(100));

			socket.Close();
			co_return found;
		}
		catch (...)
		{
			co_return false;
		}
	}

	IAsyncOperation<bool> NetworkDetector::TestPortAccessibilityAsync(uint16_t port, bool tcp)
	{
		try
		{
			// Use STUN to determine the external mapped port for a local socket
			// bound to the requested port. If the mapped port matches, the port
			// is likely directly accessible (Full Cone / EIM).
			DatagramSocket socket;

			// Bind to the specific local port
			co_await socket.BindServiceNameAsync(winrt::to_hstring(port));

			// Send STUN Binding Request to discover mapped address
			auto stunServer = L"stun.l.google.com";
			auto stunPort = L"19302";
			co_await socket.ConnectAsync(HostName(stunServer), stunPort);

			// Build STUN Binding Request (RFC 5389)
			uint8_t txId[12];
			for (int i = 0; i < 12; ++i) txId[i] = static_cast<uint8_t>(rand() & 0xFF);

			DataWriter writer(socket.OutputStream());
			writer.WriteByte(0x00); writer.WriteByte(0x01); // Binding Request
			writer.WriteByte(0x00); writer.WriteByte(0x00); // Length: 0
			writer.WriteByte(0x21); writer.WriteByte(0x12); // Magic cookie
			writer.WriteByte(0xA4); writer.WriteByte(0x42);
			writer.WriteBytes(winrt::array_view<const uint8_t>(txId, 12));
			co_await writer.StoreAsync();

			// Wait for response
			bool gotResponse = false;
			uint16_t mappedPort = 0;
			socket.MessageReceived([&](DatagramSocket const&, DatagramSocketMessageReceivedEventArgs const& args)
			{
				try
				{
					auto reader = args.GetDataReader();
					auto len = reader.UnconsumedBufferLength();
					if (len < 20) return;

					std::vector<uint8_t> data(len);
					reader.ReadBytes(winrt::array_view<uint8_t>(data));

					uint16_t msgType = (data[0] << 8) | data[1];
					uint16_t msgLen = (data[2] << 8) | data[3];
					if (msgType != 0x0101 || data.size() < static_cast<size_t>(20 + msgLen)) return;

					// Parse XOR-MAPPED-ADDRESS
					size_t offset = 20;
					while (offset + 4 <= 20u + msgLen)
					{
						uint16_t attrType = (data[offset] << 8) | data[offset + 1];
						uint16_t attrLen = (data[offset + 2] << 8) | data[offset + 3];
						if ((attrType == 0x0020 || attrType == 0x0001) && attrLen >= 8)
						{
							mappedPort = (data[offset + 6] << 8) | data[offset + 7];
							if (attrType == 0x0020) mappedPort ^= 0x2112;
							gotResponse = true;
							break;
						}
						offset += 4 + ((attrLen + 3) & ~3u);
					}
				}
				catch (...) {}
			});

			for (int i = 0; i < 30 && !gotResponse; ++i)
				co_await winrt::resume_after(std::chrono::milliseconds(100));

			socket.Close();

			// If the external mapped port equals our local port, the port is accessible
			co_return gotResponse && (mappedPort == port);
		}
		catch (...)
		{
			co_return false;
		}
	}

	IAsyncOperation<winrt::hstring> NetworkDetector::GetPublicIPAddressAsync(bool ipv6)
	{
		try
		{
			HttpClient httpClient;

			// 使用不同的服务获取公网IP / Use different services to get public IP
			winrt::hstring url = ipv6 ? L"https://ipv6.icanhazip.com" : L"https://ipv4.icanhazip.com";

			auto response = co_await httpClient.GetStringAsync(Uri(url));

			// 清理响应字符串 / Clean response string
			winrt::hstring cleanedResponse;
			for (auto c : response)
			{
				if (c != L'\n' && c != L'\r' && c != L' ')
				{
					cleanedResponse = cleanedResponse + c;
				}
			}

			co_return cleanedResponse;
		}
		catch (...)
		{
			co_return L"";
		}
	}

	IAsyncOperation<bool> NetworkDetector::CheckFirewallStatusAsync()
	{
		try
		{
			// Test by attempting to bind on the default torrent port.
			// If the bind fails the OS firewall is likely blocking it.
			DatagramSocket socket;
			co_await socket.BindServiceNameAsync(winrt::to_hstring(DEFAULT_TORRENT_PORT));
			socket.Close();
			co_return false; // No firewall block detected
		}
		catch (...)
		{
			co_return true; // Bind failed – firewall likely active
		}
	}

	Windows::Foundation::Collections::IVector<winrt::hstring> NetworkDetector::GetRecommendedSTUNServers() const
	{
		return m_stunServers;
	}

	NetworkType NetworkDetector::GetNetworkType() const
	{
		return const_cast<NetworkDetector*>(this)->DetermineNetworkType();
	}

	winrt::hstring NetworkDetector::NetworkTypeToString(NetworkType type)
	{
		switch (type)
		{
		case NetworkType::Ethernet:  return L"Ethernet";
		case NetworkType::WiFi:      return L"WiFi";
		case NetworkType::Mobile:    return L"Mobile";
		case NetworkType::Bluetooth: return L"Bluetooth";
		case NetworkType::VPN:       return L"VPN";
		default:                     return L"Unknown";
		}
	}

	winrt::event_token NetworkDetector::NetworkStateChanged(
		Windows::Foundation::EventHandler<Windows::Foundation::IInspectable> const& handler)
	{
		return m_networkStateChanged.add(handler);
	}

	void NetworkDetector::NetworkStateChanged(winrt::event_token const& token) noexcept
	{
		m_networkStateChanged.remove(token);
	}

	// 私有方法实现 / Private Method Implementations

	IAsyncOperation<winrt::hstring> NetworkDetector::SendSTUNBindingRequestAsync(
		winrt::hstring const& stunServer,
		uint16_t port)
	{
		try
		{
			DatagramSocket socket;

			// Parse STUN server address
			winrt::hstring serverHost;
			winrt::hstring serverPort;

			std::wstring_view s{ stunServer.c_str() };
			auto colonPos = s.find(L':');
			if (colonPos != std::wstring_view::npos)
			{
				serverHost = winrt::hstring{ s.substr(0, colonPos) };
				serverPort = winrt::hstring{ s.substr(colonPos + 1) };
			}
			else
			{
				serverHost = stunServer;
				serverPort = winrt::to_hstring(port);
			}

			// Wait for response
			bool gotResponse = false;
			winrt::hstring mappedAddress;
			socket.MessageReceived([&](DatagramSocket const&, DatagramSocketMessageReceivedEventArgs const& args)
			{
				try
				{
					auto reader = args.GetDataReader();
					auto len = reader.UnconsumedBufferLength();
					if (len < 20) return;

					std::vector<uint8_t> data(len);
					reader.ReadBytes(winrt::array_view<uint8_t>(data));

					uint16_t msgType = (data[0] << 8) | data[1];
					uint16_t msgLen = (data[2] << 8) | data[3];
					if (msgType != 0x0101 || data.size() < static_cast<size_t>(20 + msgLen)) return;

					size_t offset = 20;
					while (offset + 4 <= 20u + msgLen)
					{
						uint16_t attrType = (data[offset] << 8) | data[offset + 1];
						uint16_t attrLen = (data[offset + 2] << 8) | data[offset + 3];
						if ((attrType == 0x0020 || attrType == 0x0001) && attrLen >= 8)
						{
							uint16_t mp = (data[offset + 6] << 8) | data[offset + 7];
							uint32_t ip = (data[offset + 8] << 24) | (data[offset + 9] << 16) |
								(data[offset + 10] << 8) | data[offset + 11];
							if (attrType == 0x0020) { mp ^= 0x2112; ip ^= 0x2112A442; }
							wchar_t buf[64];
							swprintf(buf, 64, L"%u.%u.%u.%u:%u",
								(ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
								(ip >> 8) & 0xFF, ip & 0xFF, mp);
							mappedAddress = buf;
							gotResponse = true;
							break;
						}
						offset += 4 + ((attrLen + 3) & ~3u);
					}
				}
				catch (...) {}
			});

			co_await socket.ConnectAsync(HostName(serverHost), serverPort);

			// Build STUN Binding Request (RFC 5389)
			uint8_t txId[12];
			for (int i = 0; i < 12; ++i) txId[i] = static_cast<uint8_t>(rand() & 0xFF);

			DataWriter writer(socket.OutputStream());
			writer.WriteByte(0x00); writer.WriteByte(0x01);
			writer.WriteByte(0x00); writer.WriteByte(0x00);
			writer.WriteByte(0x21); writer.WriteByte(0x12);
			writer.WriteByte(0xA4); writer.WriteByte(0x42);
			writer.WriteBytes(winrt::array_view<const uint8_t>(txId, 12));
			co_await writer.StoreAsync();

			// Wait up to 3 seconds for response
			for (int i = 0; i < 30 && !gotResponse; ++i)
				co_await winrt::resume_after(std::chrono::milliseconds(100));

			socket.Close();
			co_return mappedAddress;
		}
		catch (...)
		{
			co_return L"";
		}
	}

	NetworkType NetworkDetector::DetermineNetworkType()
	{
		try
		{
			auto profile = NetworkInformation::GetInternetConnectionProfile();
			if (!profile) return NetworkType::Unknown;

			if (profile.IsWwanConnectionProfile())
				return NetworkType::Mobile;

			if (profile.IsWlanConnectionProfile())
				return NetworkType::WiFi;

			auto adapter = profile.NetworkAdapter();
			if (adapter)
			{
				// IANA interface type: 6 = Ethernet, 71 = WiFi, 243 = WWANPP2
				auto ianaType = adapter.IanaInterfaceType();
				switch (ianaType)
				{
				case 6:   return NetworkType::Ethernet;
				case 71:  return NetworkType::WiFi;
				case 243: return NetworkType::Mobile;
				case 15:  return NetworkType::Bluetooth;
				default:  break;
				}
			}

			return NetworkType::Ethernet;
		}
		catch (...)
		{
			return NetworkType::Unknown;
		}
	}

	void NetworkDetector::StartNetworkMonitoring()
	{
		// 启动网络状态监控 / Start network state monitoring
		try
		{
			NetworkInformation::NetworkStatusChanged([this](auto&&)
			{
				// 网络状态发生变化时触发事件 / Trigger event when network state changes
				m_networkStateChanged(nullptr, nullptr);
			});
		}
		catch (...)
		{
			// 监控启动失败 / Monitoring start failed
		}
	}

	void NetworkDetector::StopNetworkMonitoring()
	{
		// 停止网络状态监控 / Stop network state monitoring
		// WinRT NetworkInformation 不提供直接的取消注册方法
		// WinRT NetworkInformation doesn't provide direct unregister method
	}
}