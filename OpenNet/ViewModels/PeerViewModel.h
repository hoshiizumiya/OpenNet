#pragma once

#include "ObservableObject.h"
#include "../Models/PeerInfo.h"
#include "../Models/NetworkInfo.h"
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.Threading.h>
#include <memory>
#include <string>

// 使用项目自有模型命名空间
namespace Models = winrt::OpenNet::Models;

namespace OpenNet::ViewModels
{
    // 对等节点视图模型 / Peer View Model
    struct PeerViewModel : ObservableObject
    {
    public:
        PeerViewModel();
        ~PeerViewModel();

        // 对等节点列表 / Peer List
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> DiscoveredPeers() const
        {
            return m_discoveredPeers;
        }

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> ConnectedPeers() const
        {
            return m_connectedPeers;
        }

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> FavoritePeers() const
        {
            return m_favoritePeers;
        }

        // 发现状态 / Discovery Status
        enum class DiscoveryStatus
        {
            Stopped,                       // 已停止 / Stopped
            Discovering,                   // 发现中 / Discovering
            Broadcasting                   // 广播中 / Broadcasting
        };

        DiscoveryStatus Status() const { return m_status; }
        void Status(DiscoveryStatus value)
        {
            if (SetProperty(m_status, value, L"Status"))
            {
                RaisePropertyChanged(L"StatusText");
                RaisePropertyChanged(L"IsDiscovering");
                RaisePropertyChanged(L"IsBroadcasting");
                RaisePropertyChanged(L"CanStartDiscovery");
                RaisePropertyChanged(L"CanStopDiscovery");
                UpdateCommands();
            }
        }

        std::wstring StatusText() const
        {
            switch (m_status)
            {
            case DiscoveryStatus::Stopped: return L"已停止 / Stopped";
            case DiscoveryStatus::Discovering: return L"发现中... / Discovering...";
            case DiscoveryStatus::Broadcasting: return L"广播中... / Broadcasting...";
            default: return L"未知 / Unknown";
            }
        }

        bool IsDiscovering() const { return m_status == DiscoveryStatus::Discovering; }
        bool IsBroadcasting() const { return m_status == DiscoveryStatus::Broadcasting; }
        bool CanStartDiscovery() const { return m_status == DiscoveryStatus::Stopped; }
        bool CanStopDiscovery() const { return m_status != DiscoveryStatus::Stopped; }

        // 选中的对等节点 / Selected Peer
        std::shared_ptr<Models::PeerInfo> SelectedPeer() const { return m_selectedPeer; }
        void SelectedPeer(std::shared_ptr<Models::PeerInfo> value)
        {
            if (m_selectedPeer != value)
            {
                m_selectedPeer = std::move(value);
                RaisePropertyChanged(L"SelectedPeer");
                RaisePropertyChanged(L"HasSelectedPeer");
                RaisePropertyChanged(L"SelectedPeerDetails");
                UpdateCommands();
            }
        }

        bool HasSelectedPeer() const { return static_cast<bool>(m_selectedPeer); }

        std::wstring SelectedPeerDetails() const
        {
            if (!m_selectedPeer)
                return L"未选择节点 / No peer selected";

            std::wstring details = L"节点信息 / Peer Information:\n";
            details += L"名称 / Name: " + std::wstring(m_selectedPeer->displayName.c_str()) + L"\n";
            details += L"设备 / Device: " + std::wstring(m_selectedPeer->deviceName.c_str()) + L"\n";
            details += L"系统 / OS: " + std::wstring(m_selectedPeer->operatingSystem.c_str()) + L"\n";
            details += L"状态 / Status: " + std::wstring(m_selectedPeer->GetStatusString().c_str()) + L"\n";
            details += L"本地IP / Local IP: " + std::wstring(m_selectedPeer->localIP.c_str()) + L"\n";
            details += L"公网IP / Public IP: " + std::wstring(m_selectedPeer->publicIP.c_str()) + L"\n";
            details += L"NAT类型 / NAT Type: " + std::wstring(m_selectedPeer->GetNATTypeString().c_str()) + L"\n";
            details += L"延迟 / Latency: " + std::to_wstring(static_cast<int>(m_selectedPeer->latency)) + L" ms\n";
            details += L"信号强度 / Signal: " + std::wstring(m_selectedPeer->GetSignalStrengthString().c_str());

            return details;
        }

        // 本地节点信息 / Local Peer Information
        std::wstring LocalPeerName() const { return m_localPeerName; }
        void LocalPeerName(std::wstring_view value) { SetProperty(m_localPeerName, std::wstring(value), L"LocalPeerName"); }

        std::wstring LocalDeviceName() const { return m_localDeviceName; }
        void LocalDeviceName(std::wstring_view value) { SetProperty(m_localDeviceName, std::wstring(value), L"LocalDeviceName"); }

        uint16_t ListenPort() const { return m_listenPort; }
        void ListenPort(uint16_t value) { SetProperty(m_listenPort, value, L"ListenPort"); }

        bool EnableBroadcast() const { return m_enableBroadcast; }
        void EnableBroadcast(bool value)
        {
            if (SetProperty(m_enableBroadcast, value, L"EnableBroadcast"))
            {
                if (value && m_status == DiscoveryStatus::Stopped)
                {
                    StartBroadcastingAsync();
                }
                else if (!value && m_status == DiscoveryStatus::Broadcasting)
                {
                    StopDiscoveryAsync();
                }
            }
        }

        // 发现设置 / Discovery Settings
        uint32_t DiscoveryInterval() const { return m_discoveryInterval; }
        void DiscoveryInterval(uint32_t value) { SetProperty(m_discoveryInterval, value, L"DiscoveryInterval"); }

        uint32_t BroadcastInterval() const { return m_broadcastInterval; }
        void BroadcastInterval(uint32_t value) { SetProperty(m_broadcastInterval, value, L"BroadcastInterval"); }

        bool AutoConnect() const { return m_autoConnect; }
        void AutoConnect(bool value) { SetProperty(m_autoConnect, value, L"AutoConnect"); }

        // 统计信息 / Statistics
        uint32_t DiscoveredPeersCount() const { return m_discoveredPeersCount; }
        void DiscoveredPeersCount(uint32_t value) { SetProperty(m_discoveredPeersCount, value, L"DiscoveredPeersCount"); }

        uint32_t ConnectedPeersCount() const { return m_connectedPeersCount; }
        void ConnectedPeersCount(uint32_t value) { SetProperty(m_connectedPeersCount, value, L"ConnectedPeersCount"); }

        uint32_t TotalConnectionAttempts() const { return m_totalConnectionAttempts; }
        void TotalConnectionAttempts(uint32_t value) { SetProperty(m_totalConnectionAttempts, value, L"TotalConnectionAttempts"); }

        uint32_t SuccessfulConnections() const { return m_successfulConnections; }
        void SuccessfulConnections(uint32_t value) 
        { 
            if (SetProperty(m_successfulConnections, value, L"SuccessfulConnections"))
            {
                RaisePropertyChanged(L"ConnectionSuccessRate");
            }
        }

        std::wstring ConnectionSuccessRate() const
        {
            if (m_totalConnectionAttempts == 0)
                return L"N/A";
            double rate = static_cast<double>(m_successfulConnections) / m_totalConnectionAttempts * 100.0;
            return std::to_wstring(static_cast<int>(rate)) + L"%";
        }

        // 过滤设置 / Filter Settings
        std::wstring FilterText() const { return m_filterText; }
        void FilterText(std::wstring_view value)
        {
            if (SetProperty(m_filterText, std::wstring(value), L"FilterText"))
            {
                ApplyFilter();
            }
        }

        enum class SortOrder
        {
            Name,                          // 按名称 / By Name
            Distance,                      // 按距离 / By Distance
            Signal,                        // 按信号强度 / By Signal Strength
            Status                         // 按状态 / By Status
        };

        SortOrder CurrentSortOrder() const { return m_sortOrder; }
        void CurrentSortOrder(SortOrder value)
        {
            if (SetProperty(m_sortOrder, value, L"CurrentSortOrder"))
            {
                SortPeerList();
            }
        }

        // 错误信息 / Error Information
        std::wstring LastError() const { return m_lastError; }
        void LastError(std::wstring_view value) { SetProperty(m_lastError, std::wstring(value), L"LastError"); }

        bool HasError() const { return !m_lastError.empty(); }

        // 命令 / Commands
        winrt::Microsoft::UI::Xaml::Input::ICommand StartDiscoveryCommand() const { return m_startDiscoveryCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand StopDiscoveryCommand() const { return m_stopDiscoveryCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand ConnectToPeerCommand() const { return m_connectToPeerCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand DisconnectFromPeerCommand() const { return m_disconnectFromPeerCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand AddToFavoritesCommand() const { return m_addToFavoritesCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand RemoveFromFavoritesCommand() const { return m_removeFromFavoritesCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand RefreshPeerListCommand() const { return m_refreshPeerListCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand BlockPeerCommand() const { return m_blockPeerCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand ClearPeerListCommand() const { return m_clearPeerListCommand; }

        // 公共方法 / Public Methods
        winrt::Windows::Foundation::IAsyncAction InitializeAsync();
        void AddDiscoveredPeer(std::shared_ptr<Models::PeerInfo> peerInfo);
        void UpdatePeerStatus(std::shared_ptr<Models::PeerInfo> peerInfo);
        void RemovePeer(const std::wstring& peerId);

    private:
        // 命令实现 / Command Implementations
        winrt::Windows::Foundation::IAsyncAction StartDiscoveryAsync();
        winrt::Windows::Foundation::IAsyncAction StopDiscoveryAsync();
        winrt::Windows::Foundation::IAsyncAction StartBroadcastingAsync();
        winrt::Windows::Foundation::IAsyncAction ConnectToPeerAsync(winrt::Windows::Foundation::IInspectable const& parameter);
        winrt::Windows::Foundation::IAsyncAction DisconnectFromPeerAsync(winrt::Windows::Foundation::IInspectable const& parameter);
        winrt::Windows::Foundation::IAsyncAction AddToFavoritesAsync();
        winrt::Windows::Foundation::IAsyncAction RemoveFromFavoritesAsync();
        winrt::Windows::Foundation::IAsyncAction RefreshPeerListAsync();
        winrt::Windows::Foundation::IAsyncAction BlockPeerAsync();
        winrt::Windows::Foundation::IAsyncAction ClearPeerListAsync();

        // 私有方法 / Private Methods
        void InitializeCommands();
        void UpdateCommands();
        void UpdateStatistics();
        void ApplyFilter();
        void SortPeerList();

        // 发现和广播 / Discovery and Broadcasting
        winrt::Windows::Foundation::IAsyncAction PerformDiscoveryAsync();
        winrt::Windows::Foundation::IAsyncAction PerformBroadcastAsync();
        winrt::Windows::Foundation::IAsyncAction SendDiscoveryMessageAsync();
        winrt::Windows::Foundation::IAsyncAction ListenForDiscoveryMessagesAsync();

        // 连接管理 / Connection Management
        winrt::Windows::Foundation::IAsyncAction EstablishConnectionAsync(std::shared_ptr<Models::PeerInfo> peerInfo);
        winrt::Windows::Foundation::IAsyncAction PerformHandshakeAsync(std::shared_ptr<Models::PeerInfo> peerInfo);
        void OnPeerConnected(std::shared_ptr<Models::PeerInfo> peerInfo);
        void OnPeerDisconnected(std::shared_ptr<Models::PeerInfo> peerInfo);

        // 实用方法 / Utility Methods
        std::shared_ptr<Models::PeerInfo> FindPeerById(const std::wstring& peerId);
        bool IsPeerInList(const std::wstring& peerId, 
                          winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> const& list);

    private:
        // 发现状态 / Discovery Status
        DiscoveryStatus m_status;

        // 对等节点列表 / Peer Lists
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_discoveredPeers;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_connectedPeers;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_favoritePeers;

        std::shared_ptr<Models::PeerInfo> m_selectedPeer;

        // 本地节点信息 / Local Peer Information
        std::wstring m_localPeerName;
        std::wstring m_localDeviceName;
        uint16_t m_listenPort;
        bool m_enableBroadcast;

        // 发现设置 / Discovery Settings
        uint32_t m_discoveryInterval;
        uint32_t m_broadcastInterval;
        bool m_autoConnect;

        // 统计信息 / Statistics
        uint32_t m_discoveredPeersCount;
        uint32_t m_connectedPeersCount;
        uint32_t m_totalConnectionAttempts;
        uint32_t m_successfulConnections;

        // 过滤和排序 / Filtering and Sorting
        std::wstring m_filterText;
        SortOrder m_sortOrder;

        // 错误信息 / Error Information
        std::wstring m_lastError;

        // 命令 / Commands
        winrt::Microsoft::UI::Xaml::Input::ICommand m_startDiscoveryCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_stopDiscoveryCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_connectToPeerCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_disconnectFromPeerCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_addToFavoritesCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_removeFromFavoritesCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_refreshPeerListCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_blockPeerCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_clearPeerListCommand;

        // 定时器 / Timers
        winrt::Windows::System::Threading::ThreadPoolTimer m_discoveryTimer{ nullptr };
        winrt::Windows::System::Threading::ThreadPoolTimer m_broadcastTimer{ nullptr };

        // 常量 / Constants
        static constexpr uint32_t DEFAULT_DISCOVERY_INTERVAL_MS = 5000;   // 5秒 / 5 seconds
        static constexpr uint32_t DEFAULT_BROADCAST_INTERVAL_MS = 10000;  // 10秒 / 10 seconds
        static constexpr uint16_t DEFAULT_LISTEN_PORT = 8888;
        static constexpr uint32_t MAX_PEER_LIST_SIZE = 100;
    };
}