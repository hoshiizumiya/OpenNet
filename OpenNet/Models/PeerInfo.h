#pragma once

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>

namespace winrt::OpenNet::Models
{
    // 简化的对等节点信息模型 / Simplified Peer Information Model
    struct PeerInfo
    {
        // 基本身份信息 / Basic Identity Information
        winrt::hstring peerId;               // 节点ID / Peer ID  
        winrt::hstring displayName;          // 显示名称 / Display Name
        winrt::hstring deviceName;           // 设备名称 / Device Name
        winrt::hstring operatingSystem;      // 操作系统 / Operating System
        winrt::hstring version;              // 应用版本 / Application Version

        // IP地址信息 / IP Address Information
        winrt::hstring localIP;            // 本地IP地址 / Local IP Address
        winrt::hstring publicIP;           // 公网IP地址 / Public IP Address
        uint16_t localPort;                // 本地端口 / Local Port
        uint16_t publicPort;               // 公网端口 / Public Port
        bool isIPv6;                       // 是否IPv6 / Is IPv6
        
        // 连接状态 / Connection Status
        enum class ConnectionStatus
        {
            Unknown,                       // 未知 / Unknown
            Discovering,                   // 发现中 / Discovering
            Connecting,                    // 连接中 / Connecting
            Connected,                     // 已连接 / Connected
            Disconnected,                  // 已断开 / Disconnected
            Failed,                        // 连接失败 / Failed
            Timeout                        // 超时 / Timeout
        } status;

        // 连接质量信息 / Connection Quality Information
        double latency;                    // 延迟(毫秒) / Latency (ms)
        double bandwidth;                  // 带宽(字节/秒) / Bandwidth (bytes/sec)
        double packetLoss;                 // 丢包率 / Packet Loss Rate
        int signalStrength;                // 信号强度(0-100) / Signal Strength (0-100)

        // NAT穿透信息 / NAT Traversal Information
        enum class NATType
        {
            Unknown,                       // 未知 / Unknown
            Open,                          // 开放 / Open
            FullCone,                      // 完全锥形 / Full Cone
            RestrictedCone,                // 受限锥形 / Restricted Cone
            PortRestricted,                // 端口受限 / Port Restricted
            Symmetric                      // 对称 / Symmetric
        } natType;

        bool upnpSupported;                // UPnP支持 / UPnP Support

        // 安全信息 / Security Information
        winrt::hstring publicKey;            // 公钥 / Public Key
        winrt::hstring fingerprint;          // 指纹 / Fingerprint
        bool isVerified;                   // 是否已验证 / Is Verified
        bool isEncrypted;                  // 是否加密 / Is Encrypted
        winrt::hstring encryptionMethod;     // 加密方法 / Encryption Method

        // 能力信息 / Capability Information
        bool supportsFileTransfer;        // 支持文件传输 / Supports File Transfer
        bool supportsDirectConnect;       // 支持直连 / Supports Direct Connection
        bool supportsRelay;              // 支持中继 / Supports Relay
        bool supportsEncryption;         // 支持加密 / Supports Encryption
        bool supportsCompression;        // 支持压缩 / Supports Compression
        bool supportsResume;             // 支持断点续传 / Supports Resume
        uint64_t maxFileSize;            // 最大文件大小 / Max File Size
        uint32_t maxConnections;         // 最大连接数 / Max Connections

        // 统计信息 / Statistics
        uint64_t bytesReceived;        // 接收字节数 / Bytes Received
        uint64_t bytesSent;            // 发送字节数 / Bytes Sent
        uint32_t filesReceived;        // 接收文件数 / Files Received
        uint32_t filesSent;            // 发送文件数 / Files Sent
        uint32_t connectAttempts;      // 连接尝试次数 / Connection Attempts
        uint32_t successfulConnects;   // 成功连接次数 / Successful Connections
        winrt::Windows::Foundation::DateTime firstSeen;    // 首次发现时间 / First Seen
        winrt::Windows::Foundation::DateTime lastSeen;     // 最后发现时间 / Last Seen
        winrt::Windows::Foundation::DateTime lastConnected; // 最后连接时间 / Last Connected

        // 构造函数 / Constructor
        PeerInfo()
            : localPort(0)
            , publicPort(0)
            , isIPv6(false)
            , status(ConnectionStatus::Unknown)
            , latency(0.0)
            , bandwidth(0.0)
            , packetLoss(0.0)
            , signalStrength(0)
            , natType(NATType::Unknown)
            , upnpSupported(false)
            , isVerified(false)
            , isEncrypted(false)
            , supportsFileTransfer(true)
            , supportsDirectConnect(true)
            , supportsRelay(false)
            , supportsEncryption(true)
            , supportsCompression(true)
            , supportsResume(true)
            , maxFileSize(UINT64_MAX)
            , maxConnections(10)
            , bytesReceived(0)
            , bytesSent(0)
            , filesReceived(0)
            , filesSent(0)
            , connectAttempts(0)
            , successfulConnects(0)
        {
            auto now = winrt::clock::now();
            firstSeen = now;
            lastSeen = now;
            lastConnected = {};
        }

        // 获取连接状态字符串 / Get Connection Status String
        winrt::hstring GetStatusString() const
        {
            switch (status)
            {
            case ConnectionStatus::Discovering: return L"发现中 / Discovering";
            case ConnectionStatus::Connecting: return L"连接中 / Connecting";
            case ConnectionStatus::Connected: return L"已连接 / Connected";
            case ConnectionStatus::Disconnected: return L"已断开 / Disconnected";
            case ConnectionStatus::Failed: return L"连接失败 / Failed";
            case ConnectionStatus::Timeout: return L"超时 / Timeout";
            default: return L"未知 / Unknown";
            }
        }

        // 获取NAT类型字符串 / Get NAT Type String
        winrt::hstring GetNATTypeString() const
        {
            switch (natType)
            {
            case NATType::Open: return L"开放 / Open";
            case NATType::FullCone: return L"完全锥形 / Full Cone";
            case NATType::RestrictedCone: return L"受限锥形 / Restricted Cone";
            case NATType::PortRestricted: return L"端口受限 / Port Restricted";
            case NATType::Symmetric: return L"对称 / Symmetric";
            default: return L"未知 / Unknown";
            }
        }

        // 获取信号强度描述 / Get Signal Strength Description
        winrt::hstring GetSignalStrengthString() const
        {
            if (signalStrength >= 80)
                return L"优秀 / Excellent";
            else if (signalStrength >= 60)
                return L"良好 / Good";
            else if (signalStrength >= 40)
                return L"一般 / Fair";
            else if (signalStrength >= 20)
                return L"较差 / Poor";
            else
                return L"很差 / Very Poor";
        }

        // 检查是否可以直连 / Check if Direct Connection Possible
        bool CanDirectConnect() const
        {
            return (status == ConnectionStatus::Connected) &&
                   (natType == NATType::Open || natType == NATType::FullCone) &&
                   supportsDirectConnect;
        }

        // 计算连接得分 / Calculate Connection Score
        double CalculateConnectionScore() const
        {
            double score = 0.0;
            
            // 连接状态评分 / Connection Status Score
            if (status == ConnectionStatus::Connected) score += 40.0;
            else if (status == ConnectionStatus::Connecting) score += 20.0;
            
            // NAT类型评分 / NAT Type Score
            switch (natType)
            {
            case NATType::Open: score += 25.0; break;
            case NATType::FullCone: score += 20.0; break;
            case NATType::RestrictedCone: score += 15.0; break;
            case NATType::PortRestricted: score += 10.0; break;
            case NATType::Symmetric: score += 5.0; break;
            default: break;
            }
            
            // 延迟评分 / Latency Score (越低越好 / Lower is better)
            if (latency > 0 && latency < 50) score += 20.0;
            else if (latency < 100) score += 15.0;
            else if (latency < 200) score += 10.0;
            else if (latency < 500) score += 5.0;
            
            // 信号强度评分 / Signal Strength Score
            score += signalStrength * 0.15;
            
            return score;
        }

        // 更新统计信息 / Update Statistics
        void UpdateStatistics()
        {
            lastSeen = winrt::clock::now();
            if (status == ConnectionStatus::Connected)
            {
                lastConnected = lastSeen;
            }
        }
    };
}

// 为原生使用方便，导出别名 / Alias for native use convenience
namespace OpenNet::Models
{
    using PeerInfo = winrt::OpenNet::Models::PeerInfo;
}