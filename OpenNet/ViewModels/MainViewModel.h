#pragma once
#include "ViewModels/MainViewModel.g.h"
#include "Core/NetworkDetector.h"

import winrt.Windows.Foundation.Collections;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Input;
import winrt.Microsoft.UI.Dispatching;
import OpenNet.ViewModels.ObservableMixin;

namespace winrt::OpenNet::ViewModels::implementation
{
	struct MainViewModel : MainViewModelT<MainViewModel>, ::OpenNet::ViewModels::ObservableMixin<MainViewModel>
	{
		using ::OpenNet::ViewModels::ObservableMixin<MainViewModel>::SetProperty;
		using ::OpenNet::ViewModels::ObservableMixin<MainViewModel>::RaisePropertyChanged;
		MainViewModel();
		~MainViewModel();
		// 基本方法 / Basic Methods
		// Summary: 初始化视图模型
		void Initialize();
		// Summary: 停止后台线程，幂等
		void Shutdown();


		// Summary: 是否已连接
		bool IsConnected()
		{
			return m_isConnected;
		}
		void IsConnected(bool value)
		{
			SetProperty(m_isConnected, value, L"IsConnected");
		}

		// Summary: 用户名文本（状态栏）
		winrt::hstring UserName() const
		{
			return m_userName;
		}
		// Summary: 端口状态
		winrt::hstring PortState() const
		{
			return m_portState;
		}
		winrt::hstring IPv4PortState() const
		{
			return m_ipv4PortState;
		}
		winrt::hstring IPv6PortState() const
		{
			return m_ipv6PortState;
		}
		winrt::hstring IPv4TcpPortState() const
		{
			return m_ipv4TcpPortState;
		}
		winrt::hstring IPv4UdpPortState() const
		{
			return m_ipv4UdpPortState;
		}
		winrt::hstring IPv6TcpPortState() const
		{
			return m_ipv6TcpPortState;
		}
		winrt::hstring IPv6UdpPortState() const
		{
			return m_ipv6UdpPortState;
		}

		// 快速统计 / Quick stats
		std::int32_t ConnectedPeersCount() const
		{
			return m_connectedPeersCount;
		}
		std::int32_t DhtNodeCount() const
		{
			return m_dhtNodeCount;
		}
		std::int32_t ActiveTransfersCount() const
		{
			return m_activeTransfersCount;
		}
		winrt::hstring TotalBytesTransferredText() const
		{
			return m_totalBytesTransferredText;
		}
		winrt::hstring CurrentTransferSpeedText() const
		{
			return m_currentTransferSpeedText;
		}
		winrt::hstring SpeedLevel() const
		{
			return m_speedLevel;
		}
		std::int32_t ListenPort() const
		{
			return m_listenPort;
		}
		std::int32_t IPv4ListenPort() const
		{
			return m_ipv4ListenPort;
		}
		std::int32_t IPv6ListenPort() const
		{
			return m_ipv6ListenPort;
		}
		winrt::hstring ListenPortText() const
		{
			return m_listenPortText;
		}
		winrt::hstring IPv4ListenPortText() const
		{
			return m_ipv4ListenPortText;
		}
		winrt::hstring IPv6ListenPortText() const
		{
			return m_ipv6ListenPortText;
		}

		// 网络状态 / Network status
		winrt::hstring NetworkStatusText() const
		{
			return m_networkStatusText;
		}
		winrt::hstring NetworkQualityText() const
		{
			return m_networkQualityText;
		}

		// 活动列表 / Activities
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> RecentActivities() const
		{
			return m_recentActivities;
		}

		bool IsTasksPageSelected() const
		{
			return m_isTaskPageSelected;
		}
		void IsTasksPageSelected(bool value)
		{
			SetProperty(m_isTaskPageSelected, value, L"IsTasksPageSelected");
			if (TasksGlyph() == L"\uEB91" && value)
			{
				TasksGlyph(L"\uE7C4");
			}
			else if (TasksGlyph() == L"\uE7C4" && !value)
			{
				TasksGlyph(L"\uEB91");
			}
		}
		hstring TasksGlyph() const
		{
			return m_taskGlyph;
		}
		void TasksGlyph(hstring const& value)
		{
			SetProperty(m_taskGlyph, value, L"TasksGlyph");
		}

		// Summary: 初始化核心组件（P2P引擎）
		Windows::Foundation::IAsyncAction InitializeTorrentCore();

	private:
		bool m_isConnected{};
		winrt::hstring m_userName;
		winrt::hstring m_portState;
		winrt::hstring m_ipv4PortState{ L"Unknown" };
		winrt::hstring m_ipv6PortState{ L"Unknown" };
		winrt::hstring m_ipv4TcpPortState{ L"Unknown" };
		winrt::hstring m_ipv4UdpPortState{ L"Unknown" };
		winrt::hstring m_ipv6TcpPortState{ L"Unknown" };
		winrt::hstring m_ipv6UdpPortState{ L"Unknown" };

		// 导航状态 / Navigation state
		bool m_isHomeSelected{ true };
		bool m_isNetworkSelected{ false };
		bool m_isPeerSelected{ false };
		bool m_isTransferSelected{ false };
		bool m_isSettingsSelected{ false };

		// 快速统计 / Quick stats
		std::int32_t m_connectedPeersCount{ 0 };
		std::int32_t m_dhtNodeCount{ 0 };
		std::int32_t m_activeTransfersCount{ 0 };
		winrt::hstring m_totalBytesTransferredText{ L"0 B" };
		winrt::hstring m_currentTransferSpeedText{ L"0 bps" };
		winrt::hstring m_speedLevel{ L"Low" };
		std::int32_t m_listenPort{ 0 };
		std::int32_t m_ipv4ListenPort{ 0 };
		std::int32_t m_ipv6ListenPort{ 0 };
		winrt::hstring m_listenPortText{ L"—" };
		winrt::hstring m_ipv4ListenPortText{ L"—" };
		winrt::hstring m_ipv6ListenPortText{ L"—" };

		// 网络状态 / Network status
		winrt::hstring m_networkStatusText{ L"未知 / Unknown" };
		winrt::hstring m_networkQualityText{ L"N/A" };

		// 活动列表 / Activities
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_recentActivities;

		bool m_isTaskPageSelected{ true };
		winrt::hstring m_taskGlyph{ L"\uEB91" };

		// Speed refresh
		winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
		std::thread m_speedRefreshThread;
		std::thread m_portRefreshThread;
		std::atomic<bool> m_stopSpeedRefresh{ false };
		std::condition_variable m_speedCv;
		std::mutex m_speedMutex;
		void SpeedRefreshThreadEntry();
		void PortRefreshThreadEntry();
		::OpenNet::Core::NetworkDetector m_networkDetector;

		// Port check state
		std::chrono::steady_clock::time_point m_lastPortCheckTime{};
		std::int32_t m_lastCheckedIPv4Port{ 0 };
		std::int32_t m_lastCheckedIPv6Port{ 0 };
		std::wstring m_cachedIPv4PortState{ L"Unknown" };
		std::wstring m_cachedIPv6PortState{ L"Unknown" };
	};
}
namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct MainViewModel : MainViewModelT<MainViewModel, implementation::MainViewModel>
	{
	};
}
