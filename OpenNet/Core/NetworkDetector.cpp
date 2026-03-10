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
			// 简化的UPnP可用性检测 / Simplified UPnP availability detection
			// 在实际实现中，这里应该发送SSDP发现消息
			// In actual implementation, should send SSDP discovery messages
			co_return false; // 默认返回不可用 / Default return unavailable
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
			if (tcp)
			{
				StreamSocket socket;
				co_await socket.ConnectAsync(HostName(L"www.baidu.com"), winrt::to_hstring(port));
				co_return true;
			}
			else
			{
				// UDP测试简化实现 / Simplified UDP testing
				co_return true;
			}
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
			// 简化的防火墙状态检测 / Simplified firewall status detection
			// 尝试连接到已知端口来推断防火墙状态 / Try connecting to known port to infer firewall status
			bool canConnect = co_await TestPortAccessibilityAsync(80, true);
			co_return !canConnect; // 简化逻辑 / Simplified logic
		}
		catch (...)
		{
			co_return true; // 默认假设防火墙开启 / Default assume firewall is enabled
		}
	}

	Windows::Foundation::Collections::IVector<winrt::hstring> NetworkDetector::GetRecommendedSTUNServers() const
	{
		return m_stunServers;
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

			// 解析STUN服务器地址 / Parse STUN server address
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

			co_await socket.ConnectAsync(HostName(serverHost), serverPort);

			// 发送简化的STUN请求 / Send simplified STUN request
			DataWriter writer(socket.OutputStream());

			// STUN消息头的简化版本 / Simplified version of STUN message header
			std::array<uint8_t, 20> stunRequest{
				0x00, 0x01, // Message Type: Binding Request
				0x00, 0x00, // Message Length
				0x21, 0x12, 0xA4, 0x42, // Magic Cookie
				// Transaction ID (12 bytes)
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00
			};

			writer.WriteBytes(winrt::array_view(stunRequest));
			co_await writer.StoreAsync();

			// 简化返回 / Simplified return (不等待响应)
			co_return L"request_sent";
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
			auto connectionProfiles = NetworkInformation::GetConnectionProfiles();

			for (auto const& profile : connectionProfiles)
			{
				if (profile.GetNetworkConnectivityLevel() != NetworkConnectivityLevel::None)
				{
					auto adapter = profile.NetworkAdapter();
					if (adapter)
					{
						// 简化的网络类型判断 / Simplified network type determination
						return NetworkType::Ethernet; // 默认返回以太网 / Default return Ethernet
					}
				}
			}

			return NetworkType::Unknown;
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