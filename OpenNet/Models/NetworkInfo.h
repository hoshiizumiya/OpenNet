#pragma once
#include "pch.h"
#include "Core/Utils/Misc.h"
#include "Models/NetModel.g.h" // Provides projected enums ConnectionProtocol/IPProtocolPriority
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Networking.h>
#include <vector>
#include <algorithm>

namespace winrt::OpenNet::Models::implementation
{
    // Map implementation namespace enums used by existing code to projected enums from IDL
    using ConnectionProtocol = winrt::OpenNet::Models::ConnectionProtocol;
    using IPProtocolPriority = winrt::OpenNet::Models::IPProtocolPriority;

    // 以下为原生 C++ 模型（非 runtimeclass），供 ViewModel/核心逻辑使用
    // Native C++ models (not runtimeclass) used by ViewModels and core logic

    // 加密设置 / Encryption Settings
    struct EncryptionSettings
    {
        bool enableEncryption{ true };     // 启用加密 / Enable Encryption
        bool forceEncryption{ false };     // 强制加密 / Force Encryption
        winrt::hstring encryptionMethod{ L"AES-256" }; // 加密方法 / Encryption Method
        bool allowPlaintext{ false };      // 允许明文 / Allow Plaintext
    };

    // 端口配置 / Port Configuration
    struct PortConfiguration
    {
        uint16_t listenPort{ 6881 };       // 监听端口 / Listen Port
        uint16_t minPort{ 49152 };         // 最小端口范围 / Min Port Range
        uint16_t maxPort{ 65535 };         // 最大端口范围 / Max Port Range
        bool randomPort{ true };           // 随机端口 / Random Port
        bool useUPnP{ true };              // 使用UPnP / Use UPnP
        std::vector<uint16_t> customPorts{}; // 自定义端口
        std::vector<uint16_t> blockedPorts{}; // 阻塞端口
    };

    // 防火墙检测结果 / Firewall Detection Result
    struct FirewallStatus
    {
        bool windowsFirewallEnabled{ false };     // Windows防火墙启用
        bool antivirusFirewallEnabled{ false };   // 杀毒软件防火墙
        bool routerFirewallEnabled{ false };      // 路由器防火墙
        std::vector<uint16_t> allowedPorts{};     // 允许的端口
        std::vector<uint16_t> deniedPorts{};      // 拒绝的端口
        winrt::hstring firewallSoftware{};        // 防火墙软件
    };

    // 扩展的网络信息模型 / Extended Network Information Model
    struct NetworkInfo
    {
        // 构造函数 / Ctor
        NetworkInfo();

        // 基本信息 / Basics
        winrt::hstring interfaceName{};   // 接口名称
        winrt::hstring description{};     // 接口描述
        winrt::hstring adapterType{};     // 适配器类型
        bool isWired{ false };            // 有线
        bool isWireless{ false };         // 无线
        bool isMobile{ false };           // 移动
        bool isActive{ false };           // 活跃

        // IPv4 状态 / IPv4 Status
        struct IPv4Status
        {
            bool enabled{ true };
            bool available{ false };
            bool hasPublicIP{ false };
            winrt::hstring localIP{};
            winrt::hstring publicIP{};
            winrt::hstring subnetMask{};
            winrt::hstring gateway{};
            winrt::hstring dnsServer{};
            double latency{ 0.0 };
            bool natDetected{ false };
        } ipv4Status{};

        // IPv6 状态 / IPv6 Status
        struct IPv6Status
        {
            bool enabled{ true };
            bool available{ false };
            bool hasGlobalAddress{ false };
            winrt::hstring linkLocalIP{};
            winrt::hstring globalIP{};
            winrt::hstring gateway{};
            winrt::hstring dnsServer{};
            double latency{ 0.0 };
            bool natDetected{ false };
        } ipv6Status{};

        // 协议与安全 / Protocols & Security
        IPProtocolPriority protocolPriority{ IPProtocolPriority::Auto };
        ConnectionProtocol preferredProtocol{ ConnectionProtocol::Auto };
        std::vector<winrt::OpenNet::Models::ConnectionProtocol> supportedProtocols{};
        EncryptionSettings encryptionSettings{};
        PortConfiguration portConfiguration{};
        FirewallStatus firewallStatus{};

        // 网络质量 / Quality Metrics
        double latency{ 0.0 };
        double bandwidth{ 0.0 };
        double packetLoss{ 0.0 };
        double signalStrength{ 0.0 };

        // 连接状态 / Connection Status
        enum class ConnectionStatus
        {
            Disconnected,
            Connecting,
            Connected,
            Limited,
            NoInternet
        };
        ConnectionStatus status{ ConnectionStatus::Disconnected };
        uint32_t mtu{ 1500 };

        // NAT 信息 / NAT info
        enum class NATType
        {
            Unknown,
            Open,
            FullCone,
            RestrictedCone,
            PortRestricted,
            Symmetric,
            Blocked
        };
        NATType natType{ NATType::Unknown };
        uint16_t externalPort{ 0 };
        bool upnpAvailable{ false };
        bool pmpAvailable{ false };
        bool iceSupported{ false };
        bool stunSupported{ false };
        bool turnSupported{ false };

        // 统计数据 / Stats
        uint64_t bytesReceived{ 0 };
        uint64_t bytesSent{ 0 };
        uint64_t packetsReceived{ 0 };
        uint64_t packetsSent{ 0 };

        // 时间戳 / Timestamps
        winrt::Windows::Foundation::DateTime lastUpdated{ winrt::clock::now() };
        winrt::Windows::Foundation::DateTime connectedTime{};

        // 工具方法 / Helpers
        winrt::hstring GetStatusString() const;
        winrt::hstring GetNATTypeString() const;
        winrt::hstring GetProtocolPriorityString() const;
        winrt::hstring GetPreferredProtocolString() const;
        double GetNetworkScore() const;
        winrt::hstring GetBestIPAddress() const;
        bool SupportsProtocol(winrt::OpenNet::Models::ConnectionProtocol protocol) const;
        void AddSupportedProtocol(winrt::OpenNet::Models::ConnectionProtocol protocol);
    };

    // 网络统计信息 / Network Statistics
    struct NetworkStatistics
    {
        uint64_t bytesReceived{ 0 };
        uint64_t bytesSent{ 0 };
        uint64_t packetsReceived{ 0 };
        uint64_t packetsSent{ 0 };
        uint64_t errorsReceived{ 0 };
        uint64_t errorsSent{ 0 };
        uint64_t droppedPackets{ 0 };
        winrt::Windows::Foundation::DateTime timestamp{ winrt::clock::now() };

        // 计算传输速率 / Calculate Transfer Rate
        double GetReceiveRate(NetworkStatistics const& previous) const
        {
            auto currentTicks = timestamp.time_since_epoch().count();
            auto previousTicks = previous.timestamp.time_since_epoch().count();
            auto timeDiffSeconds = static_cast<double>(currentTicks - previousTicks) / 10000000.0; // 100ns tick
            if (timeDiffSeconds == 0.0) return 0.0;
            return static_cast<double>(bytesReceived - previous.bytesReceived) / timeDiffSeconds;
        }
        double GetSendRate(NetworkStatistics const& previous) const
        {
            auto currentTicks = timestamp.time_since_epoch().count();
            auto previousTicks = previous.timestamp.time_since_epoch().count();
            auto timeDiffSeconds = static_cast<double>(currentTicks - previousTicks) / 10000000.0;
            if (timeDiffSeconds == 0.0) return 0.0;
            return static_cast<double>(bytesSent - previous.bytesSent) / timeDiffSeconds;
        }
        static winrt::hstring FormatBytes(uint64_t bytes)
        {
            return ::Core::Utils::Misc::friendlyUnitCompact(bytes);
        }
    };
}

// 为原生使用者提供简单别名：OpenNet::Models::{NetworkInfo}
// Aliases for native code convenience
namespace OpenNet::Models
{
    using NetworkInfo = winrt::OpenNet::Models::implementation::NetworkInfo;
}
