#pragma once

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>

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
        winrt::Windows::Foundation::IAsyncOperation<int32_t> DetectNATTypeAsync();
        winrt::Windows::Foundation::IAsyncOperation<bool> CheckUPnPAvailabilityAsync();

        // 端口检测 / Port checking
        winrt::Windows::Foundation::IAsyncOperation<bool> TestPortAccessibilityAsync(uint16_t port, bool tcp = true);

        // 公网IP检测 / Public IP detection
        winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> GetPublicIPAddressAsync(bool ipv6 = false);

        // 防火墙检测（简化）/ Firewall detection (simplified)
        winrt::Windows::Foundation::IAsyncOperation<bool> CheckFirewallStatusAsync();

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
        winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> SendSTUNBindingRequestAsync(winrt::hstring const& stunServer, uint16_t port);
        NetworkType DetermineNetworkType();

        // 成员变量 / Member Variables
        bool m_isDetecting;
        winrt::Windows::Foundation::Collections::IVector<winrt::hstring> m_stunServers;

        // 事件 / Events
        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>> m_networkStateChanged;

        // 常量 / Constants
        static constexpr uint32_t DETECTION_TIMEOUT_MS = 30000;    // 检测超时 / Detection timeout
        static constexpr uint32_t PORT_SCAN_TIMEOUT_MS = 5000;     // 端口扫描超时 / Port scan timeout
        static constexpr uint32_t MAX_CONCURRENT_TESTS = 10;       // 最大并发测试数 / Max concurrent tests
        static constexpr uint16_t DEFAULT_TORRENT_PORT = 6881;     // 默认BitTorrent端口 / Default BitTorrent port
        static constexpr uint16_t DEFAULT_DHT_PORT = 6881;         // 默认DHT端口 / Default DHT port
    };
}