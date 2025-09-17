#pragma once

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Storage.Streams.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace OpenNet::Core
{
    // 前向声明 / Forward Declarations
    struct NetworkDetectionResult;
    enum class NATType;

    // P2P连接状态 / P2P Connection Status
    enum class ConnectionStatus
    {
        Disconnected,                  // 断开连接 / Disconnected
        Connecting,                    // 连接中 / Connecting
        Connected,                     // 已连接 / Connected
        Failed,                        // 连接失败 / Failed
        Timeout                        // 超时 / Timeout
    };

    // P2P连接信息 / P2P Connection Information
    struct P2PConnection
    {
        std::wstring peerId;           // 对等节点ID / Peer ID
        std::wstring peerName;         // 对等节点名称 / Peer Name
        winrt::Windows::Networking::HostName localEndpoint;    // 本地端点 / Local Endpoint
        winrt::Windows::Networking::HostName remoteEndpoint;   // 远程端点 / Remote Endpoint
        uint16_t localPort;            // 本地端口 / Local Port
        uint16_t remotePort;           // 远程端口 / Remote Port
        ConnectionStatus status;       // 连接状态 / Connection Status
        std::chrono::system_clock::time_point connectedTime;   // 连接时间 / Connected Time
        uint64_t bytesReceived;        // 接收字节数 / Bytes Received
        uint64_t bytesSent;            // 发送字节数 / Bytes Sent
        double latency;                // 延迟 / Latency
        winrt::Windows::Networking::Sockets::StreamSocket socket; // 套接字 / Socket

        P2PConnection()
            : localPort(0)
            , remotePort(0)
            , status(ConnectionStatus::Disconnected)
            , connectedTime{}
            , bytesReceived(0)
            , bytesSent(0)
            , latency(0.0)
            , socket(nullptr)
        {
        }
    };

    // 连接事件参数 / Connection Event Arguments
    struct ConnectionEventArgs
    {
        std::wstring peerId;
        ConnectionStatus status;
        std::wstring errorMessage;
    };

    // P2P管理器类 / P2P Manager Class
    class P2PManager
    {
    public:
        P2PManager();
        ~P2PManager();

        // 初始化和启动 / Initialize and Start
        winrt::Windows::Foundation::IAsyncOperation<bool> InitializeAsync();
        winrt::Windows::Foundation::IAsyncAction StartListeningAsync(uint16_t port = 8888);
        winrt::Windows::Foundation::IAsyncAction StopListeningAsync();

        // 节点发现 / Peer Discovery
        winrt::Windows::Foundation::IAsyncAction StartDiscoveryAsync();
        winrt::Windows::Foundation::IAsyncAction StopDiscoveryAsync();
        winrt::Windows::Foundation::IAsyncAction BroadcastPresenceAsync();

        // 连接管理 / Connection Management
        winrt::Windows::Foundation::IAsyncOperation<bool> ConnectToPeerAsync(
            const std::wstring& peerId,
            winrt::Windows::Networking::HostName const& hostName,
            uint16_t port);

        winrt::Windows::Foundation::IAsyncAction DisconnectFromPeerAsync(const std::wstring& peerId);
        winrt::Windows::Foundation::IAsyncAction DisconnectAllAsync();

        // NAT穿透 / NAT Traversal
        winrt::Windows::Foundation::IAsyncOperation<bool> PerformNATTraversalAsync(
            const std::wstring& peerId,
            winrt::Windows::Networking::HostName const& publicIP,
            uint16_t publicPort);

        winrt::Windows::Foundation::IAsyncOperation<bool> PerformHolePunchingAsync(
            winrt::Windows::Networking::HostName const& targetIP,
            uint16_t targetPort);

        // 数据传输 / Data Transfer
        winrt::Windows::Foundation::IAsyncOperation<bool> SendDataAsync(
            const std::wstring& peerId,
            winrt::Windows::Storage::Streams::IBuffer const& data);

        winrt::Windows::Foundation::IAsyncOperation<bool> SendFileAsync(
            const std::wstring& peerId,
            winrt::Windows::Storage::StorageFile const& file);

        // 连接信息查询 / Connection Information Query
        std::vector<std::shared_ptr<P2PConnection>> GetActiveConnections() const;
        std::shared_ptr<P2PConnection> GetConnection(const std::wstring& peerId) const;
        bool IsConnectedToPeer(const std::wstring& peerId) const;

        // 网络统计 / Network Statistics
        struct NetworkStatistics
        {
            uint32_t activeConnections;
            uint32_t totalConnectionAttempts;
            uint32_t successfulConnections;
            uint64_t totalBytesReceived;
            uint64_t totalBytesSent;
            double averageLatency;
        };

        NetworkStatistics GetNetworkStatistics() const;

        // 事件处理 / Event Handling
        winrt::event_token PeerDiscovered(winrt::Windows::Foundation::EventHandler<winrt::hstring> const& handler);
        void PeerDiscovered(winrt::event_token const& token) noexcept;

        winrt::event_token PeerConnected(winrt::Windows::Foundation::EventHandler<winrt::hstring> const& handler);
        void PeerConnected(winrt::event_token const& token) noexcept;

        winrt::event_token PeerDisconnected(winrt::Windows::Foundation::EventHandler<winrt::hstring> const& handler);
        void PeerDisconnected(winrt::event_token const& token) noexcept;

        winrt::event_token DataReceived(winrt::Windows::Foundation::EventHandler<winrt::Windows::Storage::Streams::IBuffer> const& handler);
        void DataReceived(winrt::event_token const& token) noexcept;

        // 配置管理 / Configuration Management
        void SetLocalPeerInfo(const std::wstring& peerId, const std::wstring& peerName);
        void SetNetworkInfo(const NetworkDetectionResult& networkInfo);
        void SetSTUNServers(const std::vector<std::wstring>& stunServers);

        // 安全设置 / Security Settings
        void EnableEncryption(bool enable);
        void SetEncryptionKey(const std::vector<uint8_t>& key);

    private:
        // 内部连接管理 / Internal Connection Management
        winrt::Windows::Foundation::IAsyncOperation<std::shared_ptr<P2PConnection>> 
            EstablishConnectionAsync(
                const std::wstring& peerId,
                winrt::Windows::Networking::HostName const& hostName,
                uint16_t port);

        winrt::Windows::Foundation::IAsyncAction HandleIncomingConnectionAsync(
            winrt::Windows::Networking::Sockets::StreamSocket socket);

        winrt::Windows::Foundation::IAsyncAction PerformHandshakeAsync(
            std::shared_ptr<P2PConnection> connection);

        // 数据处理 / Data Processing
        winrt::Windows::Foundation::IAsyncAction ProcessIncomingDataAsync(
            std::shared_ptr<P2PConnection> connection);

        winrt::Windows::Foundation::IAsyncAction SendHeartbeatAsync(
            std::shared_ptr<P2PConnection> connection);

        // 发现协议 / Discovery Protocol
        winrt::Windows::Foundation::IAsyncAction SendDiscoveryMessageAsync();
        winrt::Windows::Foundation::IAsyncAction ListenForDiscoveryAsync();
        winrt::Windows::Foundation::IAsyncAction ProcessDiscoveryMessageAsync(
            winrt::Windows::Storage::Streams::IBuffer const& message,
            winrt::Windows::Networking::HostName const& senderIP);

        // NAT穿透辅助 / NAT Traversal Helpers
        winrt::Windows::Foundation::IAsyncOperation<bool> TestDirectConnectionAsync(
            winrt::Windows::Networking::HostName const& hostName,
            uint16_t port);

        winrt::Windows::Foundation::IAsyncOperation<bool> PerformUDPHolePunchingAsync(
            winrt::Windows::Networking::HostName const& targetIP,
            uint16_t targetPort);

        winrt::Windows::Foundation::IAsyncOperation<bool> PerformTCPHolePunchingAsync(
            winrt::Windows::Networking::HostName const& targetIP,
            uint16_t targetPort);

        // 连接维护 / Connection Maintenance
        winrt::Windows::Foundation::IAsyncAction MonitorConnectionsAsync();
        void CleanupConnection(const std::wstring& peerId);
        void UpdateConnectionStatistics(std::shared_ptr<P2PConnection> connection);

        // 协议处理 / Protocol Handling
        enum class MessageType : uint8_t
        {
            Handshake = 0x01,
            HandshakeResponse = 0x02,
            Data = 0x03,
            FileTransfer = 0x04,
            Heartbeat = 0x05,
            Disconnect = 0x06,
            Discovery = 0x07,
            DiscoveryResponse = 0x08
        };

        struct MessageHeader
        {
            MessageType type;
            uint32_t dataLength;
            std::wstring sourceId;
            std::wstring targetId;
        };

        winrt::Windows::Foundation::IAsyncOperation<MessageHeader> ReadMessageHeaderAsync(
            winrt::Windows::Storage::Streams::DataReader const& reader);

        winrt::Windows::Foundation::IAsyncAction SendMessageAsync(
            std::shared_ptr<P2PConnection> connection,
            MessageType type,
            winrt::Windows::Storage::Streams::IBuffer const& data);

        // 加密和安全 / Encryption and Security
        winrt::Windows::Storage::Streams::IBuffer EncryptData(
            winrt::Windows::Storage::Streams::IBuffer const& data);

        winrt::Windows::Storage::Streams::IBuffer DecryptData(
            winrt::Windows::Storage::Streams::IBuffer const& encryptedData);

        bool VerifyPeerIdentity(const std::wstring& peerId, const std::vector<uint8_t>& signature);

    private:
        // 本地节点信息 / Local Peer Information
        std::wstring m_localPeerId;
        std::wstring m_localPeerName;
        uint16_t m_listenPort;

        // 网络信息 / Network Information
        OpenNet::Core::P2PManager::NetworkDetectionResult m_networkInfo;
        std::vector<std::wstring> m_stunServers;
        NATType m_natType;

        // 连接管理 / Connection Management
        std::unordered_map<std::wstring, std::shared_ptr<P2PConnection>> m_connections;
        mutable std::mutex m_connectionsMutex;

        // 网络套接字 / Network Sockets
        winrt::Windows::Networking::Sockets::StreamSocketListener m_tcpListener;
        winrt::Windows::Networking::Sockets::DatagramSocket m_udpSocket;
        winrt::Windows::Networking::Sockets::DatagramSocket m_discoverySocket;

        // 状态标志 / State Flags
        bool m_isListening;
        bool m_isDiscovering;
        bool m_encryptionEnabled;

        // 统计信息 / Statistics
        NetworkStatistics m_statistics;

        // 加密密钥 / Encryption Key
        std::vector<uint8_t> m_encryptionKey;

        // 事件 / Events
        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::hstring>> m_peerDiscoveredEvent;
        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::hstring>> m_peerConnectedEvent;
        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::hstring>> m_peerDisconnectedEvent;
        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Storage::Streams::IBuffer>> m_dataReceivedEvent;

        // 定时器 / Timers
        winrt::Windows::System::Threading::ThreadPoolTimer m_heartbeatTimer;
        winrt::Windows::System::Threading::ThreadPoolTimer m_discoveryTimer;
        winrt::Windows::System::Threading::ThreadPoolTimer m_cleanupTimer;

        // 常量 / Constants
        static constexpr uint32_t MAX_MESSAGE_SIZE = 64 * 1024 * 1024; // 64MB
        static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;        // 30秒 / 30 seconds
        static constexpr uint32_t DISCOVERY_INTERVAL_MS = 10000;        // 10秒 / 10 seconds
        static constexpr uint32_t CONNECTION_TIMEOUT_MS = 30000;        // 30秒 / 30 seconds
        static constexpr uint32_t CLEANUP_INTERVAL_MS = 60000;          // 60秒 / 60 seconds
        static constexpr uint16_t DISCOVERY_PORT = 8889;
    };
}