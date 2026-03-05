#pragma once
#include "ViewModels/MainViewModel.g.h"
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <thread>
#include <atomic>
#include <chrono>

namespace winrt::OpenNet::ViewModels::implementation
{
    // 主视图模型 / Main ViewModel
    struct MainViewModel : MainViewModelT<MainViewModel>
    {
        MainViewModel();
        ~MainViewModel();

        // 基本方法 / Basic Methods
        // Summary: 初始化视图模型
        void Initialize();

        // 基本属性 / Basic Properties
        // Summary: 当前状态文本
        // Return: 状态字符串
        winrt::hstring StatusText() { return m_statusText; }
        void StatusText(winrt::hstring str)
        {
            if (m_statusText != str)
            {
                m_statusText = str;
                OnPropertyChanged(L"StatusText");
            }
        }
        // Summary: 是否已连接
        bool IsConnected() { return m_isConnected; }
        void IsConnected(bool value)
        {
            if (m_isConnected != value)
            {
                m_isConnected = value;
                OnPropertyChanged(L"IsConnected");
            }
        }

        // Summary: 应用版本号（展示用）
        winrt::hstring AppVersion() const { return m_appVersion; }
        // Summary: 用户名文本（状态栏）
        winrt::hstring UserName() const { return m_userName; }
        // Summary: 端口状态
        winrt::hstring PortState() const { return m_portState; }

        // 快速统计 / Quick stats
        int32_t ConnectedPeersCount() const { return m_connectedPeersCount; }
        int32_t DhtNodeCount() const { return m_dhtNodeCount; }
        int32_t ActiveTransfersCount() const { return m_activeTransfersCount; }
        winrt::hstring TotalBytesTransferredText() const { return m_totalBytesTransferredText; }
        winrt::hstring CurrentTransferSpeedText() const { return m_currentTransferSpeedText; }
        winrt::hstring SpeedLevel() const { return m_speedLevel; }
        int32_t ListenPort() const { return m_listenPort; }

        // 网络状态 / Network status
        winrt::hstring NetworkStatusText() const { return m_networkStatusText; }
        winrt::hstring NetworkQualityText() const { return m_networkQualityText; }

        // 活动列表 / Activities
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> RecentActivities() const { return m_recentActivities; }

        // 错误/通知 / Error & Notification
        winrt::hstring LastError() const { return m_lastError; }
        bool HasError() const { return !m_lastError.empty(); }
        winrt::hstring LastNotification() const { return m_lastNotification; }
        bool HasNotification() const { return !m_lastNotification.empty(); }

        // Summary: 更新状态文本
        // Param status: 新的状态字符串
        void UpdateStatus(winrt::hstring const &status);

        // Summary: 初始化核心组件（P2P引擎）
        Windows::Foundation::IAsyncAction InitializeTorrentCore();

        // INotifyPropertyChanged add/remove
        winrt::event_token PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const &handler) { return m_propertyChanged.add(handler); }
        void PropertyChanged(winrt::event_token const &token) noexcept { m_propertyChanged.remove(token); }

        // 命令（暂为占位，后续接入实际逻辑）/ Commands (placeholders)
        winrt::Microsoft::UI::Xaml::Input::ICommand ShowAboutCommand() const { return m_showAboutCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand NavigateToPageCommand() const { return m_navigateToPageCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand StartNetworkDetectionCommand() const { return m_startNetworkDetectionCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand ConnectToPeerCommand() const { return m_connectToPeerCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand RefreshCommand() const { return m_refreshCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand ClearErrorCommand() const { return m_clearErrorCommand; }

        // 工具方法 / Utility
        void OnPropertyChanged(winrt::hstring const &propertyName)
        {
            // Summary: 触发属性变更通知
            // Param propertyName: 变更属性名称
            m_propertyChanged(*this, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{propertyName});
        }

    private:
        // 基本状态 / Basic state
        winrt::hstring m_statusText;
        bool m_isConnected{};
        winrt::hstring m_appVersion;
        winrt::hstring m_userName;
        winrt::hstring m_portState;

        // 导航状态 / Navigation state
        bool m_isHomeSelected{true};
        bool m_isNetworkSelected{false};
        bool m_isPeerSelected{false};
        bool m_isTransferSelected{false};
        bool m_isSettingsSelected{false};

        // 快速统计 / Quick stats
        int32_t m_connectedPeersCount{0};
        int32_t m_dhtNodeCount{0};
        int32_t m_activeTransfersCount{0};
        winrt::hstring m_totalBytesTransferredText{L"0 B"};
        winrt::hstring m_currentTransferSpeedText{L"0 bps"};
        winrt::hstring m_speedLevel{L"Low"};
        int32_t m_listenPort{0};

        // 网络状态 / Network status
        winrt::hstring m_networkStatusText{L"未知 / Unknown"};
        winrt::hstring m_networkQualityText{L"N/A"};

        // 活动列表 / Activities
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_recentActivities;

        // 错误/通知 / Error & notification
        winrt::hstring m_lastError;
        winrt::hstring m_lastNotification;

        // 命令 / Commands
        winrt::Microsoft::UI::Xaml::Input::ICommand m_showAboutCommand{nullptr};
        winrt::Microsoft::UI::Xaml::Input::ICommand m_navigateToPageCommand{nullptr};
        winrt::Microsoft::UI::Xaml::Input::ICommand m_startNetworkDetectionCommand{nullptr};
        winrt::Microsoft::UI::Xaml::Input::ICommand m_connectToPeerCommand{nullptr};
        winrt::Microsoft::UI::Xaml::Input::ICommand m_refreshCommand{nullptr};
        winrt::Microsoft::UI::Xaml::Input::ICommand m_clearErrorCommand{nullptr};

        // 事件 / Events
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;

        // Speed refresh
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{nullptr};
        std::thread m_speedRefreshThread;
        std::atomic<bool> m_stopSpeedRefresh{false};
        void SpeedRefreshThreadEntry();

        // Port check state
        std::chrono::steady_clock::time_point m_lastPortCheckTime{};
        int m_lastCheckedPort{0};
        std::wstring m_cachedPortState{L"Unknown"};
    };
}
namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct MainViewModel : MainViewModelT<MainViewModel, implementation::MainViewModel>
    {
    };
}
