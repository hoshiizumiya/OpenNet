#pragma once

import winrt.Windows.Networking;
import winrt.Windows.Networking.Connectivity;
import winrt.Windows.Networking.Sockets;
import winrt.Windows.Foundation;
import winrt.Windows.Foundation.Collections;
import winrt.Windows.Storage.Streams;
import winrt.Windows.Web.Http;
import std;

namespace OpenNet::Core
{
	// 网络类型枚举 / Network Type Enumeration
	enum class NetworkType
	{
		Unknown,                         // 未知 / Unknown
		Ethernet,                        // 以太网 / Ethernet
		WiFi,                            // 无线网络 / WiFi
		Mobile,                          // 移动网络 / Mobile
		Bluetooth,                       // 蓝牙 / Bluetooth
		VPN                              // 虚拟专用网 / VPN
	};

	// NAT类型枚举 / NAT Type Enumeration
	enum class NATType
	{
		Unknown,                         // 未知 / Unknown
		Open,                            // 开放 / Open
		FullCone,                        // 完全锥形 / Full Cone
		RestrictedCone,                  // 受限锥形 / Restricted Cone
		PortRestricted,                  // 端口受限 / Port Restricted
		Symmetric                        // 对称 / Symmetric
	};

	enum class NatMappingBehavior
	{
		Unknown,
		Direct,
		EndpointIndependent,
		AddressDependent,
		AddressAndPortDependent
	};

	enum class NatFilteringBehavior
	{
		Unknown,
		EndpointIndependent,
		AddressDependent,
		AddressAndPortDependent
	};

	enum class PortProbeAddressFamily
	{
		Both,
		IPv4,
		IPv6
	};

	struct TraversalServerDescriptor
	{
		winrt::hstring name;
		winrt::hstring ipv4Address;
		winrt::hstring ipv6Address;
		winrt::hstring alternateIPv4Address;
		std::uint16_t apiPort{};
		std::uint16_t stunPort{};
		std::uint16_t alternateStunPort{};
		std::int32_t priority{};
	};

	struct StunObservation
	{
		bool success{};
		winrt::hstring server;
		std::uint16_t serverPort{};
		winrt::hstring mappedAddress;
		std::uint16_t mappedPort{};
		std::int32_t latencyMs{};
	};

	struct PortProbeResult
	{
		bool completed{};
		bool timedOut{};
		bool ipv4TimedOut{};
		bool ipv6TimedOut{};
		bool tcpCompleted{};
		bool udpCompleted{};
		bool tcpReachable{};
		bool udpReachable{};
		bool ipv6Completed{};
		bool ipv6TcpCompleted{};
		bool ipv6UdpCompleted{};
		bool ipv6TcpReachable{};
		bool ipv6UdpReachable{};
		std::int32_t tcpLatencyMs{};
		std::int32_t udpLatencyMs{};
		std::int32_t ipv6TcpLatencyMs{};
		std::int32_t ipv6UdpLatencyMs{};
		winrt::hstring observedAddress;
		winrt::hstring serverName;
		winrt::hstring detail;
		winrt::hstring tcpEvidence;
		winrt::hstring udpEvidence;
		winrt::hstring ipv6TcpEvidence;
		winrt::hstring ipv6UdpEvidence;
	};

	struct NatDetectionResult
	{
		bool completed{};
		bool udpAvailable{};
		winrt::hstring localIPv4;
		std::uint16_t localPort{};
		winrt::hstring publicIPv4;
		NatMappingBehavior mapping{ NatMappingBehavior::Unknown };
		NatFilteringBehavior filtering{ NatFilteringBehavior::Unknown };
		NATType legacyType{ NATType::Unknown };
		winrt::hstring summary;
		winrt::hstring diagnostic;
		std::vector<StunObservation> observations;
		StunObservation filteringDifferentAddressAndPort;
		StunObservation filteringDifferentPort;
		bool filteringDifferentAddressAndPortTested{};
		bool filteringDifferentPortTested{};
		PortProbeResult portProbe;
	};

	// 增强的网络检测器类 / Enhanced Network Detector Class
	class NetworkDetector
	{
	public:
		NetworkDetector();
		~NetworkDetector();

		// 基础网络检测 / Basic Network Detection
		winrt::Windows::Foundation::IAsyncOperation<bool> DetectNetworkEnvironmentAsync();
		winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> GetNetworkInfoAsync();

		// NAT穿透检测 / NAT Traversal Detection (简化实现)
		winrt::Windows::Foundation::IAsyncOperation<std::int32_t> DetectNATTypeAsync();
		winrt::Windows::Foundation::IAsyncOperation<bool> CheckUPnPAvailabilityAsync();

		// 端口检测 / Port checking
		winrt::Windows::Foundation::IAsyncOperation<bool> TestPortAccessibilityAsync(std::uint16_t port, bool tcp = true);
		winrt::Windows::Foundation::IAsyncAction TestPortAccessibilityDetailedAsync(
			std::uint16_t port,
			std::shared_ptr<PortProbeResult> result,
			PortProbeAddressFamily family = PortProbeAddressFamily::Both);
		winrt::Windows::Foundation::IAsyncAction TestListeningPortsAsync(
			std::uint16_t ipv4Port,
			std::uint16_t ipv6Port,
			std::shared_ptr<PortProbeResult> result);
		winrt::Windows::Foundation::IAsyncAction DetectNATBehaviorAsync(
			std::uint16_t ipv4ListenPort,
			std::uint16_t ipv6ListenPort,
			std::shared_ptr<NatDetectionResult> result);
		winrt::Windows::Foundation::IAsyncAction GetTraversalServersAsync(
			std::shared_ptr<std::vector<TraversalServerDescriptor>> servers);

		winrt::hstring TraversalDirectoryUri() const;
		void TraversalDirectoryUri(winrt::hstring const& value);

		static winrt::hstring MappingBehaviorToString(NatMappingBehavior value);
		static winrt::hstring FilteringBehaviorToString(NatFilteringBehavior value);
		static winrt::hstring NATTypeToString(NATType value);

		// 公网IP检测 / Public IP detection
		winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> GetPublicIPAddressAsync(bool ipv6 = false);

		// 防火墙检测（简化）/ Firewall detection (simplified)
		winrt::Windows::Foundation::IAsyncOperation<bool> CheckFirewallStatusAsync();

		// 网络类型检测 / Network type detection
		NetworkType GetNetworkType() const;
		static winrt::hstring NetworkTypeToString(NetworkType type);

		// 推荐服务器 / Recommended servers
		winrt::Windows::Foundation::Collections::IVector<winrt::hstring> GetRecommendedSTUNServers() const;

		// 事件处理 / Event Handling
		winrt::event_token NetworkStateChanged(winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& handler);
		void NetworkStateChanged(winrt::event_token const& token) noexcept;

		// 网络监控 / Network Monitoring
		void StartNetworkMonitoring();
		void StopNetworkMonitoring();

	private:
		// 内部方法 / Internal Methods
		winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> SendSTUNBindingRequestAsync(winrt::hstring const& stunServer, std::uint16_t port);
		winrt::Windows::Foundation::IAsyncAction PerformStunExchangeAsync(
			winrt::Windows::Networking::Sockets::DatagramSocket const& socket,
			winrt::hstring const& server,
			std::uint16_t port,
			bool changeIp,
			bool changePort,
			std::shared_ptr<StunObservation> result,
			std::uint32_t timeoutMs);
		winrt::hstring GetLocalIPv4Address() const;
		winrt::Windows::Foundation::IAsyncOperation<bool> ProbeServerPortAsync(
			TraversalServerDescriptor const& server,
			std::uint16_t port,
			bool tcp,
			bool useIpv6,
			std::shared_ptr<PortProbeResult> result,
			std::uint32_t timeoutMs);
		NetworkType DetermineNetworkType();

		// 成员变量 / Member Variables
		bool m_isDetecting;
		winrt::Windows::Foundation::Collections::IVector<winrt::hstring> m_stunServers;
		winrt::hstring m_traversalDirectoryUri;

		// 事件 / Events
		winrt::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>> m_networkStateChanged;
		// Token for the NetworkInformation::NetworkStatusChanged registration so we can unregister
		winrt::event_token m_networkStatusChangedToken{};

		// 常量 / Constants
		static constexpr std::uint32_t DETECTION_TIMEOUT_MS = 30000;    // 检测超时 / Detection timeout
		static constexpr std::uint32_t DIRECTORY_TIMEOUT_MS = 5000;
		static constexpr std::uint32_t PORT_TEST_TIMEOUT_MS = 12000;
		static constexpr std::uint32_t PORT_SCAN_TIMEOUT_MS = 5000;     // 端口扫描超时 / Port scan timeout
		static constexpr std::uint32_t MAX_CONCURRENT_TESTS = 10;       // 最大并发测试数 / Max concurrent tests
		static constexpr std::uint16_t DEFAULT_TORRENT_PORT = 6881;     // 默认BitTorrent端口 / Default BitTorrent port
		static constexpr std::uint16_t DEFAULT_DHT_PORT = 6881;         // 默认DHT端口 / Default DHT port
	};
}
