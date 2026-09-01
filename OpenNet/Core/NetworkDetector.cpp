#include "pch.h"
#include "NetworkDetector.h"
#include <winerror.h>

import OpenNet.Core.Setting.LocalSetting;
import OpenNet.Core.Setting.SettingKeys;
import OpenNet.Web.ServerDomain;
import OpenNet.Core.Utils.Message;
import winrt.Windows.Data.Json;

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Networking;
using namespace Windows::Networking::Connectivity;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Web::Http;
using namespace Windows::Data::Json;

namespace
{
	template<typename TAsync>
	IAsyncOperation<bool> WaitForCompletionAsync(TAsync const& operation, std::uint32_t timeoutMs)
	{
		auto cancellation = co_await winrt::get_cancellation_token();
		cancellation.enable_propagation();
		auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
		for (;;)
		{
			if (cancellation())
			{
				operation.Cancel();
				co_return false;
			}
			auto const status = operation.Status();
			if (status == AsyncStatus::Completed)
				co_return true;
			if (status != AsyncStatus::Started)
				co_return false;
			if (std::chrono::steady_clock::now() >= deadline)
			{
				operation.Cancel();
				co_return false;
			}
			co_await winrt::resume_after(std::chrono::milliseconds(50));
		}
	}

	std::uint32_t RemainingMilliseconds(std::chrono::steady_clock::time_point deadline, std::uint32_t maximum)
	{
		auto const now = std::chrono::steady_clock::now();
		if (now >= deadline)
			return 0;
		auto const remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
			deadline - now).count();
		return static_cast<std::uint32_t>(std::min<std::int64_t>(remaining, maximum));
	}
}

namespace OpenNet::Core
{
	NetworkDetector::NetworkDetector() : m_isDetecting(false)
	{
		std::wstring directoryUri{ ::OpenNet::Web::ServerDomain::GetApiRoot() };
		if (!directoryUri.ends_with(L'/'))
			directoryUri.push_back(L'/');
		directoryUri.append(L"api/v1/traversal/servers");
		m_traversalDirectoryUri = ::OpenNet::Core::Setting::LocalSetting::Get<winrt::hstring>(
			::OpenNet::Core::Setting::SettingKeys::TraversalDirectoryUri,
			winrt::hstring{ directoryUri });
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

	IAsyncOperation<std::int32_t> NetworkDetector::DetectNATTypeAsync()
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
						co_return static_cast<std::int32_t>(NATType::FullCone);
					}
				}
				catch (...)
				{
					// 尝试下一个STUN服务器 / Try next STUN server
					continue;
				}
			}

			co_return static_cast<std::int32_t>(NATType::Unknown);
		}
		catch (...)
		{
			co_return static_cast<std::int32_t>(NATType::Unknown);
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
				catch (...)
				{
				}
			});

			auto bind = socket.BindServiceNameAsync(L"");
			if (!co_await WaitForCompletionAsync(bind, PORT_SCAN_TIMEOUT_MS))
				co_return false;
			bind.GetResults();

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
			auto streamRequest = socket.GetOutputStreamAsync(remoteHost, remotePort);
			if (!co_await WaitForCompletionAsync(streamRequest, PORT_SCAN_TIMEOUT_MS))
				co_return false;
			auto outputStream = streamRequest.GetResults();
			auto write = outputStream.WriteAsync(buf);
			if (!co_await WaitForCompletionAsync(write, PORT_SCAN_TIMEOUT_MS))
				co_return false;
			write.GetResults();

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
			auto result = std::make_shared<PortProbeResult>();
			co_await TestPortAccessibilityDetailedAsync(port, result);
			co_return tcp ? result->tcpReachable : result->udpReachable;
		}
		catch (...)
		{
			co_return false;
		}
	}

	winrt::hstring NetworkDetector::TraversalDirectoryUri() const
	{
		return m_traversalDirectoryUri;
	}

	void NetworkDetector::TraversalDirectoryUri(winrt::hstring const& value)
	{
		m_traversalDirectoryUri = value;
		::OpenNet::Core::Setting::LocalSetting::Set(
			::OpenNet::Core::Setting::SettingKeys::TraversalDirectoryUri,
			value);
	}

	IAsyncAction NetworkDetector::GetTraversalServersAsync(
		std::shared_ptr<std::vector<TraversalServerDescriptor>> servers)
	{
		servers->clear();
		try
		{
			HttpClient client;
			auto request = client.GetStringAsync(Uri(m_traversalDirectoryUri));
			if (!co_await WaitForCompletionAsync(request, DIRECTORY_TIMEOUT_MS))
				co_return;
			auto const json = request.GetResults();
			auto root = JsonObject::Parse(json);
			auto items = root.GetNamedArray(L"servers");
			servers->reserve(items.Size());
			for (uint32_t index = 0; index < items.Size(); ++index)
			{
				auto item = items.GetObjectAt(index);
				TraversalServerDescriptor server;
				server.name = item.GetNamedString(L"name", L"Traversal server");
				server.ipv4Address = item.GetNamedString(L"ipv4Address", L"");
				if (item.HasKey(L"ipv6Address"))
				{
					auto ipv6 = item.Lookup(L"ipv6Address");
					if (ipv6.ValueType() == JsonValueType::String)
						server.ipv6Address = ipv6.GetString();
				}
				if (item.HasKey(L"alternateIPv4Address"))
				{
					auto alternate = item.Lookup(L"alternateIPv4Address");
					if (alternate.ValueType() == JsonValueType::String)
						server.alternateIPv4Address = alternate.GetString();
				}
				server.apiPort = static_cast<uint16_t>(item.GetNamedNumber(L"apiPort", 48100));
				server.stunPort = static_cast<uint16_t>(item.GetNamedNumber(L"stunPort", 3478));
				server.alternateStunPort = static_cast<uint16_t>(
					item.GetNamedNumber(L"alternateStunPort", 3479));
				server.priority = static_cast<int32_t>(item.GetNamedNumber(L"priority", 100));
				if (!server.ipv4Address.empty())
					servers->push_back(std::move(server));
			}
		}
		catch (...)
		{
		}
		co_return;
	}

	IAsyncOperation<bool> NetworkDetector::ProbeServerPortAsync(
		TraversalServerDescriptor const& server,
		uint16_t port,
		bool tcp,
		bool useIpv6,
		std::shared_ptr<PortProbeResult> result,
		std::uint32_t timeoutMs)
	{
		try
		{
			if (timeoutMs == 0)
				co_return false;
			auto const deadline = std::chrono::steady_clock::now()
				+ std::chrono::milliseconds(timeoutMs);
			auto host = useIpv6 ? L"[" + server.ipv6Address + L"]" : server.ipv4Address;
			auto uri = Uri(
				L"http://" + host + L":" + winrt::to_hstring(server.apiPort)
				+ (tcp ? L"/v1/probes/tcp" : L"/v1/probes/udp"));
			HttpClient client;
			HttpStringContent content(
				L"{\"port\":" + winrt::to_hstring(port) + L"}",
				UnicodeEncoding::Utf8,
				L"application/json");
			auto request = client.PostAsync(uri, content);
			if (!co_await WaitForCompletionAsync(request, RemainingMilliseconds(deadline, timeoutMs)))
			{
				co_return false;
			}
			auto const response = request.GetResults();
			if (!response.IsSuccessStatusCode())
				co_return false;

			auto contentRead = response.Content().ReadAsStringAsync();
			if (!co_await WaitForCompletionAsync(contentRead, RemainingMilliseconds(deadline, timeoutMs)))
			{
				co_return false;
			}
			auto json = contentRead.GetResults();
			auto body = JsonObject::Parse(json);
			bool reachable = body.GetNamedBoolean(L"reachable", false);
			auto evidence = body.GetNamedString(L"evidence", L"");
			auto const latency = static_cast<std::int32_t>(body.GetNamedNumber(L"latencyMs", 0));
			if (useIpv6 && tcp)
			{
				result->ipv6TcpCompleted = true;
				result->ipv6TcpLatencyMs = latency;
				if (reachable || result->ipv6TcpEvidence.empty())
					result->ipv6TcpEvidence = evidence;
			}
			else if (useIpv6)
			{
				result->ipv6UdpCompleted = true;
				result->ipv6UdpLatencyMs = latency;
				if (reachable || result->ipv6UdpEvidence.empty())
					result->ipv6UdpEvidence = evidence;
			}
			else if (tcp)
			{
				result->tcpCompleted = true;
				result->tcpLatencyMs = latency;
				if (reachable || result->tcpEvidence.empty())
					result->tcpEvidence = evidence;
			}
			else
			{
				result->udpCompleted = true;
				result->udpLatencyMs = latency;
				if (reachable || result->udpEvidence.empty())
					result->udpEvidence = evidence;
			}
			if (useIpv6 && tcp)
				result->ipv6TcpReachable = result->ipv6TcpReachable || reachable;
			else if (useIpv6)
				result->ipv6UdpReachable = result->ipv6UdpReachable || reachable;
			else if (tcp)
				result->tcpReachable = result->tcpReachable || reachable;
			else
				result->udpReachable = result->udpReachable || reachable;
			co_return true;
		}
		catch (...)
		{
			co_return false;
		}
	}

	IAsyncAction NetworkDetector::TestPortAccessibilityDetailedAsync(
		uint16_t port,
		std::shared_ptr<PortProbeResult> result,
		PortProbeAddressFamily family)
	{
		*result = {};
		auto cancellation = co_await winrt::get_cancellation_token();
		cancellation.enable_propagation();
		auto const deadline = std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(PORT_TEST_TIMEOUT_MS);
		auto servers = std::make_shared<std::vector<TraversalServerDescriptor>>();
		co_await GetTraversalServersAsync(servers);
		bool attemptedIPv4 = false;
		bool attemptedIPv6 = false;
		for (size_t index = 0; index < std::min<size_t>(servers->size(), 2); ++index)
		{
			if (cancellation() || RemainingMilliseconds(deadline, PORT_SCAN_TIMEOUT_MS) == 0)
				break;
			std::vector<IAsyncOperation<bool>> probes;
			if (family != PortProbeAddressFamily::IPv6)
			{
				attemptedIPv4 = true;
				probes.push_back(ProbeServerPortAsync(
					(*servers)[index], port, true, false, result,
					RemainingMilliseconds(deadline, PORT_SCAN_TIMEOUT_MS)));
				probes.push_back(ProbeServerPortAsync(
					(*servers)[index], port, false, false, result,
					RemainingMilliseconds(deadline, PORT_SCAN_TIMEOUT_MS)));
			}
			if (family != PortProbeAddressFamily::IPv4
				&& !(*servers)[index].ipv6Address.empty())
			{
				attemptedIPv6 = true;
				probes.push_back(ProbeServerPortAsync(
					(*servers)[index], port, true, true, result,
					RemainingMilliseconds(deadline, PORT_SCAN_TIMEOUT_MS)));
				probes.push_back(ProbeServerPortAsync(
					(*servers)[index], port, false, true, result,
					RemainingMilliseconds(deadline, PORT_SCAN_TIMEOUT_MS)));
			}
			// All requested transports are started before awaiting any one of
			// them. A blocked UDP port therefore does not delay the TCP result (or
			// vice versa), and all four checks finish within one server timeout.
			for (auto const& probe : probes)
			{
				try
				{
					co_await probe;
				}
				catch (...)
				{
				}
			}
			result->completed = result->tcpCompleted || result->udpCompleted;
			result->ipv6Completed =
				result->ipv6TcpCompleted || result->ipv6UdpCompleted;
			bool const ipv4Done = family == PortProbeAddressFamily::IPv6
				|| (result->tcpCompleted && result->udpCompleted);
			bool const ipv6Done = family == PortProbeAddressFamily::IPv4
				|| (result->ipv6TcpCompleted && result->ipv6UdpCompleted);
			if (ipv4Done && ipv6Done)
				break;
		}
		result->ipv4TimedOut = family != PortProbeAddressFamily::IPv6
			&& (!result->tcpCompleted || !result->udpCompleted)
			&& (attemptedIPv4 || servers->empty());
		result->ipv6TimedOut = family != PortProbeAddressFamily::IPv4
			&& (!result->ipv6TcpCompleted || !result->ipv6UdpCompleted)
			&& attemptedIPv6;
		result->timedOut = result->ipv4TimedOut || result->ipv6TimedOut;
		if (servers->empty())
			result->detail = L"Traversal directory unavailable or timed out.";
		co_return;
	}

	IAsyncOperation<winrt::hstring> NetworkDetector::GetPublicIPAddressAsync(bool ipv6)
	{
		try
		{
			HttpClient httpClient;

			// 使用不同的服务获取公网IP / Use different services to get public IP
			winrt::hstring url = ipv6 ? L"https://ipv6.icanhazip.com" : L"https://ipv4.icanhazip.com";

			auto request = httpClient.GetStringAsync(Uri(url));
			if (!co_await WaitForCompletionAsync(request, PORT_SCAN_TIMEOUT_MS))
				co_return L"";
			auto response = request.GetResults();

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
			auto bind = socket.BindServiceNameAsync(winrt::to_hstring(DEFAULT_TORRENT_PORT));
			if (!co_await WaitForCompletionAsync(bind, PORT_SCAN_TIMEOUT_MS))
				co_return true;
			bind.GetResults();
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
							if (attrType == 0x0020)
							{
								mp ^= 0x2112; ip ^= 0x2112A442;
							}
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
				catch (...)
				{
				}
			});

			auto connect = socket.ConnectAsync(HostName(serverHost), serverPort);
			if (!co_await WaitForCompletionAsync(connect, PORT_SCAN_TIMEOUT_MS))
				co_return L"";
			connect.GetResults();

			// Build STUN Binding Request (RFC 5389)
			uint8_t txId[12];
			for (int i = 0; i < 12; ++i) txId[i] = static_cast<uint8_t>(rand() & 0xFF);

			DataWriter writer(socket.OutputStream());
			writer.WriteByte(0x00); writer.WriteByte(0x01);
			writer.WriteByte(0x00); writer.WriteByte(0x00);
			writer.WriteByte(0x21); writer.WriteByte(0x12);
			writer.WriteByte(0xA4); writer.WriteByte(0x42);
			writer.WriteBytes(winrt::array_view<const uint8_t>(txId, 12));
			auto store = writer.StoreAsync();
			if (!co_await WaitForCompletionAsync(store, PORT_SCAN_TIMEOUT_MS))
				co_return L"";
			store.GetResults();

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

	IAsyncAction NetworkDetector::PerformStunExchangeAsync(
		DatagramSocket const& socket,
		winrt::hstring const& server,
		uint16_t port,
		bool changeIp,
		bool changePort,
		std::shared_ptr<StunObservation> result,
		std::uint32_t timeoutMs)
	{
		struct ExchangeState
		{
			std::atomic<bool> received{ false };
			std::array<uint8_t, 12> transaction{};
			StunObservation observation;
		};

		auto state = std::make_shared<ExchangeState>();
		state->observation.server = server;
		state->observation.serverPort = port;
		std::random_device random;
		for (auto& value : state->transaction)
			value = static_cast<uint8_t>(random());

		auto started = std::chrono::steady_clock::now();
		auto const deadline = started + std::chrono::milliseconds(timeoutMs);
		auto token = socket.MessageReceived(
			[state, started](DatagramSocket const&, DatagramSocketMessageReceivedEventArgs const& args)
		{
			try
			{
				auto reader = args.GetDataReader();
				auto length = reader.UnconsumedBufferLength();
				if (length < 20)
					return;

				std::vector<uint8_t> data(length);
				reader.ReadBytes(winrt::array_view<uint8_t>(data));
				auto read16 = [&data](size_t offset)
				{
					return static_cast<uint16_t>(
						(static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1]);
				};
				if (read16(0) != 0x0101
					|| data[4] != 0x21 || data[5] != 0x12
					|| data[6] != 0xA4 || data[7] != 0x42
					|| !std::equal(
						state->transaction.begin(),
						state->transaction.end(),
						data.begin() + 8))
				{
					return;
				}

				uint16_t messageLength = read16(2);
				if (data.size() < 20U + messageLength)
					return;
				for (size_t offset = 20; offset + 4 <= 20U + messageLength;)
				{
					uint16_t type = read16(offset);
					uint16_t attributeLength = read16(offset + 2);
					size_t value = offset + 4;
					if (value + attributeLength > 20U + messageLength)
						break;
					if ((type == 0x0020 || type == 0x0001)
						&& attributeLength >= 8
						&& data[value + 1] == 0x01)
					{
						uint16_t mappedPort = read16(value + 2);
						uint32_t mappedIp =
							(static_cast<uint32_t>(data[value + 4]) << 24)
							| (static_cast<uint32_t>(data[value + 5]) << 16)
							| (static_cast<uint32_t>(data[value + 6]) << 8)
							| data[value + 7];
						if (type == 0x0020)
						{
							mappedPort ^= 0x2112;
							mappedIp ^= 0x2112A442;
						}
						state->observation.mappedAddress = winrt::hstring{ std::format(
							L"{}.{}.{}.{}",
							(mappedIp >> 24) & 0xff,
							(mappedIp >> 16) & 0xff,
							(mappedIp >> 8) & 0xff,
							mappedIp & 0xff) };
						state->observation.mappedPort = mappedPort;
						state->observation.latencyMs = static_cast<int32_t>(
							std::chrono::duration_cast<std::chrono::milliseconds>(
								std::chrono::steady_clock::now() - started).count());
						state->observation.success = true;
						state->received.store(true);
						return;
					}
					offset = value + ((attributeLength + 3U) & ~3U);
				}
			}
			catch (...)
			{
			}
		});

		try
		{
			std::vector<uint8_t> packet;
			packet.reserve(28);
			packet.insert(packet.end(), { 0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xA4, 0x42 });
			packet.insert(packet.end(), state->transaction.begin(), state->transaction.end());
			if (changeIp || changePort)
			{
				packet[2] = 0x00;
				packet[3] = 0x08;
				packet.insert(packet.end(), { 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x00,
					static_cast<uint8_t>((changeIp ? 0x04 : 0) | (changePort ? 0x02 : 0)) });
			}

			auto streamRequest = socket.GetOutputStreamAsync(HostName(server), winrt::to_hstring(port));
			if (!co_await WaitForCompletionAsync(streamRequest, RemainingMilliseconds(deadline, timeoutMs)))
			{
				throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
			}
			auto stream = streamRequest.GetResults();
			DataWriter writer(stream);
			writer.WriteBytes(packet);
			auto store = writer.StoreAsync();
			if (!co_await WaitForCompletionAsync(
				store, RemainingMilliseconds(deadline, timeoutMs)))
			{
				throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
			}
			store.GetResults();
			writer.DetachStream();

			while (!state->received.load()
				   && std::chrono::steady_clock::now() < deadline)
			{
				co_await winrt::resume_after(std::chrono::milliseconds(100));
			}
		}
		catch (...)
		{
		}

		socket.MessageReceived(token);
		*result = std::move(state->observation);
		co_return;
	}

	winrt::hstring NetworkDetector::GetLocalIPv4Address() const
	{
		try
		{
			for (auto const& host : NetworkInformation::GetHostNames())
			{
				if (host.Type() != HostNameType::Ipv4)
					continue;
				auto address = host.CanonicalName();
				if (address != L"127.0.0.1" && !address.starts_with(L"169.254."))
					return address;
			}
		}
		catch (...)
		{
		}
		return {};
	}

	IAsyncAction NetworkDetector::TestListeningPortsAsync(
		std::uint16_t ipv4Port,
		std::uint16_t ipv6Port,
		std::shared_ptr<PortProbeResult> result)
	{
		*result = {};
		if (ipv4Port == 0 && ipv6Port == 0)
			co_return;
		if (ipv4Port > 0 && ipv4Port == ipv6Port)
		{
			co_await TestPortAccessibilityDetailedAsync(
				ipv4Port, result, PortProbeAddressFamily::Both);
			co_return;
		}

		auto ipv4Result = std::make_shared<PortProbeResult>();
		auto ipv6Result = std::make_shared<PortProbeResult>();
		IAsyncAction ipv4Operation{ nullptr };
		IAsyncAction ipv6Operation{ nullptr };
		if (ipv4Port > 0)
		{
			ipv4Operation = TestPortAccessibilityDetailedAsync(
				ipv4Port, ipv4Result, PortProbeAddressFamily::IPv4);
		}
		if (ipv6Port > 0)
		{
			ipv6Operation = TestPortAccessibilityDetailedAsync(
				ipv6Port, ipv6Result, PortProbeAddressFamily::IPv6);
		}
		try
		{
			if (ipv4Operation) co_await ipv4Operation;
		}
		catch (...)
		{
		}
		try
		{
			if (ipv6Operation) co_await ipv6Operation;
		}
		catch (...)
		{
		}

		result->completed = ipv4Result->completed;
		result->tcpCompleted = ipv4Result->tcpCompleted;
		result->udpCompleted = ipv4Result->udpCompleted;
		result->tcpReachable = ipv4Result->tcpReachable;
		result->udpReachable = ipv4Result->udpReachable;
		result->tcpLatencyMs = ipv4Result->tcpLatencyMs;
		result->udpLatencyMs = ipv4Result->udpLatencyMs;
		result->tcpEvidence = ipv4Result->tcpEvidence;
		result->udpEvidence = ipv4Result->udpEvidence;
		result->ipv4TimedOut = ipv4Result->ipv4TimedOut;

		result->ipv6Completed = ipv6Result->ipv6Completed;
		result->ipv6TcpCompleted = ipv6Result->ipv6TcpCompleted;
		result->ipv6UdpCompleted = ipv6Result->ipv6UdpCompleted;
		result->ipv6TcpReachable = ipv6Result->ipv6TcpReachable;
		result->ipv6UdpReachable = ipv6Result->ipv6UdpReachable;
		result->ipv6TcpLatencyMs = ipv6Result->ipv6TcpLatencyMs;
		result->ipv6UdpLatencyMs = ipv6Result->ipv6UdpLatencyMs;
		result->ipv6TcpEvidence = ipv6Result->ipv6TcpEvidence;
		result->ipv6UdpEvidence = ipv6Result->ipv6UdpEvidence;
		result->ipv6TimedOut = ipv6Result->ipv6TimedOut;
		result->timedOut = result->ipv4TimedOut || result->ipv6TimedOut;
	}

	IAsyncAction NetworkDetector::DetectNATBehaviorAsync(
		uint16_t ipv4ListenPort,
		uint16_t ipv6ListenPort,
		std::shared_ptr<NatDetectionResult> output)
	{
		*output = {};
		auto& result = *output;
		auto cancellation = co_await winrt::get_cancellation_token();
		cancellation.enable_propagation();
		auto const deadline = std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(DETECTION_TIMEOUT_MS);
		result.localIPv4 = GetLocalIPv4Address();
		auto servers = std::make_shared<std::vector<TraversalServerDescriptor>>();
		co_await GetTraversalServersAsync(servers);
		if (servers->empty())
		{
			result.diagnostic =
				L"Traversal server discovery timed out or returned no server: "
				+ m_traversalDirectoryUri;
			result.portProbe.timedOut = true;
			result.portProbe.ipv4TimedOut = true;
			result.portProbe.ipv6TimedOut = true;
			co_return;
		}

		DatagramSocket socket;
		try
		{
			auto bind = socket.BindServiceNameAsync(L"");
			if (!co_await WaitForCompletionAsync(
				bind, RemainingMilliseconds(deadline, PORT_SCAN_TIMEOUT_MS)))
			{
				result.diagnostic = L"Timed out while opening the STUN test socket.";
				co_return;
			}
			bind.GetResults();
			try
			{
				auto const information = socket.Information();
				if (auto const localAddress = information.LocalAddress();
					localAddress && localAddress.Type() == HostNameType::Ipv4)
				{
					result.localIPv4 = localAddress.CanonicalName();
				}
				auto const localPortText = winrt::to_string(information.LocalPort());
				unsigned int localPort{};
				auto const [end, error] = std::from_chars(
					localPortText.data(),
					localPortText.data() + localPortText.size(),
					localPort);
				if (error == std::errc{}
					&& end == localPortText.data() + localPortText.size()
					&& localPort <= 65535)
				{
					result.localPort = static_cast<std::uint16_t>(localPort);
				}
			}
			catch (...)
			{
			}
			auto const& primary = servers->front();
			auto first = std::make_shared<StunObservation>();
			co_await PerformStunExchangeAsync(
				socket, primary.ipv4Address, primary.stunPort, false, false, first,
				RemainingMilliseconds(deadline, 3000));
			result.observations.push_back(*first);
			if (!first->success)
			{
				socket.Close();
				if (ipv4ListenPort > 0 || ipv6ListenPort > 0)
				{
					auto portProbe = std::make_shared<PortProbeResult>();
					auto portOperation = TestListeningPortsAsync(
						ipv4ListenPort, ipv6ListenPort, portProbe);
					auto const remaining = RemainingMilliseconds(
						deadline, PORT_TEST_TIMEOUT_MS);
					if (remaining > 0
						&& co_await WaitForCompletionAsync(portOperation, remaining))
					{
						portOperation.GetResults();
						result.portProbe = std::move(*portProbe);
					}
					else
					{
						result.portProbe.timedOut = true;
						result.portProbe.ipv4TimedOut = true;
						result.portProbe.ipv6TimedOut = true;
					}
				}
				result.diagnostic =
					L"The primary STUN endpoint timed out. Port reachability was "
					L"allowed to finish independently.";
				co_return;
			}

			result.udpAvailable = true;
			result.publicIPv4 = first->mappedAddress;
			auto differentPort = std::make_shared<StunObservation>();
			co_await PerformStunExchangeAsync(
				socket,
				primary.ipv4Address,
				primary.alternateStunPort,
				false,
				false,
				differentPort,
				RemainingMilliseconds(deadline, 3000));
			result.observations.push_back(*differentPort);

			StunObservation differentAddress;
			if (!primary.alternateIPv4Address.empty())
			{
				auto observation = std::make_shared<StunObservation>();
				co_await PerformStunExchangeAsync(
					socket,
					primary.alternateIPv4Address,
					primary.stunPort,
					false,
					false,
					observation,
					RemainingMilliseconds(deadline, 3000));
				differentAddress = std::move(*observation);
			}
			else if (servers->size() > 1)
			{
				auto observation = std::make_shared<StunObservation>();
				co_await PerformStunExchangeAsync(
					socket,
					(*servers)[1].ipv4Address,
					(*servers)[1].stunPort,
					false,
					false,
					observation,
					RemainingMilliseconds(deadline, 3000));
				differentAddress = std::move(*observation);
			}
			if (!differentAddress.server.empty())
				result.observations.push_back(differentAddress);

			auto sameMapping = [](StunObservation const& left, StunObservation const& right)
			{
				return left.success && right.success
					&& left.mappedAddress == right.mappedAddress
					&& left.mappedPort == right.mappedPort;
			};

			if (!result.localIPv4.empty() && result.localIPv4 == result.publicIPv4)
			{
				result.mapping = NatMappingBehavior::Direct;
			}
			else if (differentPort->success && !sameMapping(*first, *differentPort))
			{
				result.mapping = NatMappingBehavior::AddressAndPortDependent;
			}
			else if (differentAddress.success && !sameMapping(*first, differentAddress))
			{
				result.mapping = NatMappingBehavior::AddressDependent;
			}
			else if (differentPort->success && differentAddress.success)
			{
				result.mapping = NatMappingBehavior::EndpointIndependent;
			}

			if (!primary.alternateIPv4Address.empty())
			{
				auto changeBoth = std::make_shared<StunObservation>();
				result.filteringDifferentAddressAndPortTested = true;
				co_await PerformStunExchangeAsync(
					socket, primary.ipv4Address, primary.stunPort, true, true, changeBoth,
					RemainingMilliseconds(deadline, 3000));
				result.filteringDifferentAddressAndPort = *changeBoth;
				if (changeBoth->success)
				{
					result.filtering = NatFilteringBehavior::EndpointIndependent;
				}
				else
				{
					auto changePort = std::make_shared<StunObservation>();
					result.filteringDifferentPortTested = true;
					co_await PerformStunExchangeAsync(
						socket, primary.ipv4Address, primary.stunPort, false, true, changePort,
						RemainingMilliseconds(deadline, 3000));
					result.filteringDifferentPort = *changePort;
					result.filtering = changePort->success
						? NatFilteringBehavior::AddressDependent
						: NatFilteringBehavior::AddressAndPortDependent;
				}
			}
			socket.Close();

			if (ipv4ListenPort > 0 || ipv6ListenPort > 0)
			{
				auto portProbe = std::make_shared<PortProbeResult>();
				auto portOperation = TestListeningPortsAsync(
					ipv4ListenPort, ipv6ListenPort, portProbe);
				auto const remaining = RemainingMilliseconds(
					deadline, PORT_TEST_TIMEOUT_MS);
				if (remaining > 0
					&& co_await WaitForCompletionAsync(portOperation, remaining))
				{
					portOperation.GetResults();
					result.portProbe = std::move(*portProbe);
				}
				else
				{
					result.portProbe.timedOut = true;
					result.portProbe.ipv4TimedOut = true;
					result.portProbe.ipv6TimedOut = true;
				}
			}

			if (result.mapping == NatMappingBehavior::Direct)
			{
				result.legacyType = result.portProbe.tcpReachable || result.portProbe.udpReachable
					? NATType::Open
					: NATType::Unknown;
			}
			else if (result.mapping == NatMappingBehavior::EndpointIndependent)
			{
				if (result.filtering == NatFilteringBehavior::EndpointIndependent)
					result.legacyType = NATType::FullCone;
				else if (result.filtering == NatFilteringBehavior::AddressDependent)
					result.legacyType = NATType::RestrictedCone;
				else if (result.filtering == NatFilteringBehavior::AddressAndPortDependent)
					result.legacyType = NATType::PortRestricted;
			}
			else if (result.mapping == NatMappingBehavior::AddressDependent
					 || result.mapping == NatMappingBehavior::AddressAndPortDependent)
			{
				result.legacyType = NATType::Symmetric;
			}

			result.completed = true;
			result.summary = MappingBehaviorToString(result.mapping)
				+ L" / " + FilteringBehaviorToString(result.filtering);
			if (primary.alternateIPv4Address.empty())
			{
				result.diagnostic =
					L"Mapping behavior was measured, but filtering behavior requires "
					L"a traversal node with two public IPv4 addresses.";
			}
		}
		catch (winrt::hresult_error const& error)
		{
			socket.Close();
			result.diagnostic = error.message();
		}
		catch (...)
		{
			socket.Close();
			result.diagnostic = L"Unexpected NAT detection failure.";
		}
		co_return;
	}

	winrt::hstring NetworkDetector::MappingBehaviorToString(NatMappingBehavior value)
	{
		switch (value)
		{
			case NatMappingBehavior::Direct: return ResourceGetString(L"CoreNetworkDetectorMappingDirect");
			case NatMappingBehavior::EndpointIndependent: return ResourceGetString(L"CoreNetworkDetectorMappingEndpointIndependent");
			case NatMappingBehavior::AddressDependent: return ResourceGetString(L"CoreNetworkDetectorMappingAddressDependent");
			case NatMappingBehavior::AddressAndPortDependent: return ResourceGetString(L"CoreNetworkDetectorMappingAddressAndPortDependent");
			default: return ResourceGetString(L"CommonUnknown");
		}
	}

	winrt::hstring NetworkDetector::FilteringBehaviorToString(NatFilteringBehavior value)
	{
		switch (value)
		{
			case NatFilteringBehavior::EndpointIndependent: return ResourceGetString(L"CoreNetworkDetectorFilteringEndpointIndependent");
			case NatFilteringBehavior::AddressDependent: return ResourceGetString(L"CoreNetworkDetectorFilteringAddressDependent");
			case NatFilteringBehavior::AddressAndPortDependent: return ResourceGetString(L"CoreNetworkDetectorFilteringAddressAndPortDependent");
			default: return ResourceGetString(L"CommonUnknown");
		}
	}

	winrt::hstring NetworkDetector::NATTypeToString(NATType value)
	{
		switch (value)
		{
			case NATType::Open: return ResourceGetString(L"CoreNetworkDetectorNATOpenInternet");
			case NATType::FullCone: return ResourceGetString(L"CoreNetworkDetectorNATFullCone");
			case NATType::RestrictedCone: return ResourceGetString(L"CoreNetworkDetectorNATRestrictedCone");
			case NATType::PortRestricted: return ResourceGetString(L"CoreNetworkDetectorNATPortRestrictedCone");
			case NATType::Symmetric: return ResourceGetString(L"CoreNetworkDetectorNATSymmetric");
			default: return ResourceGetString(L"CommonUnknown");
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
			// Register network status changed handler and store token so we can unregister later
			m_networkStatusChangedToken = NetworkInformation::NetworkStatusChanged([this](auto&&)
			{
				// 网络状态发生变化时触发事件 / Trigger event when network state changes
				this->m_networkStateChanged(nullptr, nullptr);
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
		try
		{
			if (m_networkStatusChangedToken.value != 0)
			{
				NetworkInformation::NetworkStatusChanged(m_networkStatusChangedToken);
				m_networkStatusChangedToken = {};
			}
		}
		catch (...)
		{
		}
	}
}
