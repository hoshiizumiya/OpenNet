#pragma once

#include "ObservableObject.h"
#include "../Models/NetworkInfo.h"
#include "../Core/NetworkDetector.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <memory>
#include <string>

namespace OpenNet::ViewModels
{
    // 网络状态视图模型 / Network Status View Model
    struct NetworkViewModel : ObservableObject
    {
    public:
        NetworkViewModel() = default;

        // 当前网络状态 / Current Network Status
        std::shared_ptr<Models::NetworkInfo> CurrentNetwork() const { return m_currentNetwork; }
        void CurrentNetwork(std::shared_ptr<Models::NetworkInfo> value)
        {
            m_currentNetwork = value;
            RaisePropertyChanged(L"CurrentNetwork");
            UpdateNetworkProperties();
        }

        // 网络适配器列表 / Network Adapter List
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> NetworkAdapters() const
        {
            return m_networkAdapters;
        }

        // 网络检测状态 / Network Detection Status
        enum class DetectionStatus
        {
            Idle,                          // 空闲 / Idle
            Detecting,                     // 检测中 / Detecting
            Completed,                     // 已完成 / Completed
            Failed                         // 失败 / Failed
        };

        DetectionStatus Status() const { return m_status; }
        void Status(DetectionStatus value)
        {
            if (SetProperty(m_status, value, L"Status"))
            {
                RaisePropertyChanged(L"StatusText");
                RaisePropertyChanged(L"IsDetecting");
                RaisePropertyChanged(L"CanStartDetection");
                UpdateCommands();
            }
        }

        std::wstring StatusText() const
        {
            switch (m_status)
            {
            case DetectionStatus::Idle: return L"就绪 / Ready";
            case DetectionStatus::Detecting: return L"检测中... / Detecting...";
            case DetectionStatus::Completed: return L"检测完成 / Detection Completed";
            case DetectionStatus::Failed: return L"检测失败 / Detection Failed";
            default: return L"未知 / Unknown";
            }
        }

        bool IsDetecting() const { return m_status == DetectionStatus::Detecting; }
        bool CanStartDetection() const { return m_status != DetectionStatus::Detecting; }

        // 网络信息属性 / Network Information Properties
        std::wstring NetworkType() const { return m_networkType; }
        void NetworkType(std::wstring_view value) { SetProperty(m_networkType, std::wstring(value), L"NetworkType"); }

        std::wstring ConnectionStatus() const { return m_connectionStatus; }
        void ConnectionStatus(std::wstring_view value) { SetProperty(m_connectionStatus, std::wstring(value), L"ConnectionStatus"); }

        std::wstring LocalIPAddress() const { return m_localIPAddress; }
        void LocalIPAddress(std::wstring_view value) { SetProperty(m_localIPAddress, std::wstring(value), L"LocalIPAddress"); }

        std::wstring PublicIPAddress() const { return m_publicIPAddress; }
        void PublicIPAddress(std::wstring_view value) { SetProperty(m_publicIPAddress, std::wstring(value), L"PublicIPAddress"); }

        std::wstring NATType() const { return m_natType; }
        void NATType(std::wstring_view value) { SetProperty(m_natType, std::wstring(value), L"NATType"); }

        bool UPnPAvailable() const { return m_upnpAvailable; }
        void UPnPAvailable(bool value) 
        { 
            if (SetProperty(m_upnpAvailable, value, L"UPnPAvailable"))
            {
                RaisePropertyChanged(L"UPnPStatusText");
            }
        }

        std::wstring UPnPStatusText() const
        {
            return m_upnpAvailable ? L"可用 / Available" : L"不可用 / Not Available";
        }

        bool FirewallEnabled() const { return m_firewallEnabled; }
        void FirewallEnabled(bool value)
        {
            if (SetProperty(m_firewallEnabled, value, L"FirewallEnabled"))
            {
                RaisePropertyChanged(L"FirewallStatusText");
            }
        }

        std::wstring FirewallStatusText() const
        {
            return m_firewallEnabled ? L"已启用 / Enabled" : L"已禁用 / Disabled";
        }

        // 网络质量指标 / Network Quality Metrics
        double Latency() const { return m_latency; }
        void Latency(double value)
        {
            if (SetProperty(m_latency, value, L"Latency"))
            {
                RaisePropertyChanged(L"LatencyText");
                UpdateNetworkScore();
            }
        }

        std::wstring LatencyText() const
        {
            if (m_latency > 0)
                return std::to_wstring(static_cast<int>(m_latency)) + L" ms";
            return L"未测试 / Not Tested";
        }

        double Bandwidth() const { return m_bandwidth; }
        void Bandwidth(double value)
        {
            if (SetProperty(m_bandwidth, value, L"Bandwidth"))
            {
                RaisePropertyChanged(L"BandwidthText");
                UpdateNetworkScore();
            }
        }

        std::wstring BandwidthText() const
        {
            if (m_bandwidth > 0)
            {
                const wchar_t* units[] = { L"bps", L"Kbps", L"Mbps", L"Gbps" };
                int unitIndex = 0;
                double speed = m_bandwidth;
                
                while (speed >= 1000.0 && unitIndex < 3)
                {
                    speed /= 1000.0;
                    unitIndex++;
                }
                
                return std::to_wstring(static_cast<int>(speed)) + L" " + units[unitIndex];
            }
            return L"未测试 / Not Tested";
        }

        double PacketLoss() const { return m_packetLoss; }
        void PacketLoss(double value)
        {
            if (SetProperty(m_packetLoss, value, L"PacketLoss"))
            {
                RaisePropertyChanged(L"PacketLossText");
                UpdateNetworkScore();
            }
        }

        std::wstring PacketLossText() const
        {
            if (m_packetLoss >= 0)
                return std::to_wstring(static_cast<int>(m_packetLoss * 100)) + L"%";
            return L"未测试 / Not Tested";
        }

        // 网络评分 / Network Score
        double NetworkScore() const { return m_networkScore; }
        void NetworkScore(double value)
        {
            if (SetProperty(m_networkScore, value, L"NetworkScore"))
            {
                RaisePropertyChanged(L"NetworkScoreText");
                RaisePropertyChanged(L"NetworkQuality");
            }
        }

        std::wstring NetworkScoreText() const
        {
            return std::to_wstring(static_cast<int>(m_networkScore)) + L"/100";
        }

        std::wstring NetworkQuality() const
        {
            if (m_networkScore >= 80) return L"优秀 / Excellent";
            else if (m_networkScore >= 60) return L"良好 / Good";
            else if (m_networkScore >= 40) return L"一般 / Fair";
            else if (m_networkScore >= 20) return L"较差 / Poor";
            else return L"很差 / Very Poor";
        }

        // 端口状态 / Port Status
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> OpenPorts() const
        {
            return m_openPorts;
        }

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> BlockedPorts() const
        {
            return m_blockedPorts;
        }

        // STUN服务器列表 / STUN Server List
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> STUNServers() const
        {
            return m_stunServers;
        }

        std::wstring SelectedSTUNServer() const { return m_selectedSTUNServer; }
        void SelectedSTUNServer(std::wstring_view value) { SetProperty(m_selectedSTUNServer, std::wstring(value), L"SelectedSTUNServer"); }

        // 检测历史 / Detection History
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> DetectionHistory() const
        {
            return m_detectionHistory;
        }

        // 错误信息 / Error Information
        std::wstring LastError() const { return m_lastError; }
        void LastError(std::wstring_view value) { SetProperty(m_lastError, std::wstring(value), L"LastError"); }

        bool HasError() const { return !m_lastError.empty(); }

        // 命令 / Commands
        winrt::Microsoft::UI::Xaml::Input::ICommand StartDetectionCommand() const { return m_startDetectionCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand TestLatencyCommand() const { return m_testLatencyCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand TestBandwidthCommand() const { return m_testBandwidthCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand TestPortCommand() const { return m_testPortCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand RefreshCommand() const { return m_refreshCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand ExportReportCommand() const { return m_exportReportCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand ClearHistoryCommand() const { return m_clearHistoryCommand; }

        // 公共方法 / Public Methods
        winrt::Windows::Foundation::IAsyncAction InitializeAsync();
        winrt::Windows::Foundation::IAsyncAction StartNetworkDetectionAsync();
        void AddDetectionResult(std::wstring_view result);

    private:
        // 命令实现 / Command Implementations
        winrt::Windows::Foundation::IAsyncAction StartDetectionAsync();
        winrt::Windows::Foundation::IAsyncAction TestLatencyAsync();
        winrt::Windows::Foundation::IAsyncAction TestBandwidthAsync();
        winrt::Windows::Foundation::IAsyncAction TestPortAsync(winrt::Windows::Foundation::IInspectable const& parameter);
        winrt::Windows::Foundation::IAsyncAction RefreshAsync();
        winrt::Windows::Foundation::IAsyncAction ExportReportAsync();
        winrt::Windows::Foundation::IAsyncAction ClearHistoryAsync();

        // 私有方法 / Private Methods
        void InitializeCommands();
        void UpdateNetworkProperties();
        void UpdateNetworkScore();
        void UpdateCommands();
        void PopulateNetworkAdapters();
        void PopulateSTUNServers();

        // 事件处理 / Event Handlers
        void OnNetworkChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);

    private:
        // 网络信息 / Network Information
        std::shared_ptr<Models::NetworkInfo> m_currentNetwork;
        std::unique_ptr<Core::NetworkDetector> m_networkDetector;

        // 检测状态 / Detection Status
        DetectionStatus m_status;

        // 网络属性 / Network Properties
        std::wstring m_networkType;
        std::wstring m_connectionStatus;
        std::wstring m_localIPAddress;
        std::wstring m_publicIPAddress;
        std::wstring m_natType;
        bool m_upnpAvailable;
        bool m_firewallEnabled;

        // 网络质量 / Network Quality
        double m_latency;
        double m_bandwidth;
        double m_packetLoss;
        double m_networkScore;

        // 集合 / Collections
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_networkAdapters;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_openPorts;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_blockedPorts;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_stunServers;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_detectionHistory;

        // STUN服务器 / STUN Server
        std::wstring m_selectedSTUNServer;

        // 错误信息 / Error Information
        std::wstring m_lastError;

        // 命令 / Commands
        winrt::Microsoft::UI::Xaml::Input::ICommand m_startDetectionCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_testLatencyCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_testBandwidthCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_testPortCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_refreshCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_exportReportCommand;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_clearHistoryCommand;

        // 事件令牌 / Event Tokens
        winrt::event_token m_networkChangedToken;

        // 常量 / Constants
        static constexpr uint32_t MAX_HISTORY_ENTRIES = 100;
        static constexpr std::wstring_view DEFAULT_TEST_HOST = L"www.google.com";
    };
}