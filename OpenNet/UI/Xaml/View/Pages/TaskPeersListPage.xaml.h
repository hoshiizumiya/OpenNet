#pragma once

import OpenNet.ViewModels.ObservableMixin;
import OpenNet.Core.torrentCore.LibtorrentHandle;
import winrt.OpenNet.UI.Xaml.Control.Progress.HttpSegment;
import winrt.XamlToolkit.Labs.WinUI;
import winrt.XamlToolkit.WinUI.Animations;
import winrt.XamlToolkit.WinUI.Behaviors;
import winrt.XamlToolkit.WinUI.Controls;
import winrt.XamlToolkit.WinUI.Interactivity;
import winrt.XamlToolkit.WinUI.Media;
import winrt.OpenNet.UI.Xaml.Markup;
import winrt.OpenNet.UI.Xaml.Behavior;
import winrt.OpenNet.UI.Xaml.Control.Card;
import winrt.OpenNet.UI.Xaml.Control.Effect;
import winrt.OpenNet.UI.Xaml.Control.HomePage.Header;
import winrt.Microsoft.UI.Xaml.Controls.AnimatedVisuals;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Composition.SystemBackdrops;
import winrt.Windows.UI.Xaml.Controls;
import winrt.WinUI3Package.Svg;
#include "UI/Xaml/View/Pages/TaskPeersListPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskPeersListPage : TaskPeersListPageT<TaskPeersListPage>, ::OpenNet::ViewModels::ObservableMixin<TaskPeersListPage>
	{
		TaskPeersListPage();
		~TaskPeersListPage();

		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

		// Ban peer context menu handlers
		void BanPeer1h_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void BanPeer24h_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void BanPeerPermanent_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void UnbanPeer_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ColumnHeader_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ColumnHeader_PointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
		void ColumnHeader_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);
		void ColumnMenu_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void AutoSizeSelectedColumn_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void AutoSizeAllColumns_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ColumnVisibility_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ResetColumns_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void PeerDataRow_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_vmPropertyChangedToken{};

		// Timer for periodic peer refresh
		winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
		winrt::event_token m_timerTickToken{};

		// Cached observable vector for incremental updates
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_peerItems{ nullptr };
		winrt::OpenNet::ViewModels::PeerDisplayItem m_connectedGroup{ nullptr };
		winrt::OpenNet::ViewModels::PeerDisplayItem m_connectingGroup{ nullptr };
		winrt::OpenNet::ViewModels::PeerDisplayItem m_disconnectingGroup{ nullptr };
		winrt::OpenNet::ViewModels::PeerDisplayItem m_banIpGroup{ nullptr };
		std::unordered_map<std::string, winrt::OpenNet::ViewModels::PeerDisplayItem> m_lastActivePeers;
		std::unordered_map<std::string, winrt::OpenNet::ViewModels::PeerDisplayItem> m_disconnectingPeers;
		std::unordered_map<std::string, winrt::OpenNet::ViewModels::PeerDisplayItem> m_banIpPeers;
		std::unordered_set<std::string> m_cachedBannedPeerAddresses;
		std::string m_flagSprite;
		std::unordered_map<std::string, winrt::hstring> m_flagSvgCache;
		winrt::hstring m_sortColumn;
		int m_sortDirection{};
		winrt::XamlToolkit::Labs::WinUI::DataColumn m_contextColumn{ nullptr };
		bool m_rowLayoutSynchronizationQueued{};
		std::atomic_bool m_refreshInFlight{};
		std::atomic_bool m_forcePeerRefresh{ true };
		std::atomic_bool m_isActive{};
		std::atomic_uint64_t m_refreshGeneration{};
		std::size_t m_lastPeerSnapshotHash{};
		bool m_hasPeerSnapshot{};
		std::chrono::steady_clock::time_point m_lastAuxiliaryRefresh{};
		std::chrono::milliseconds m_configuredRefreshInterval{ 1000 };

		// Track last known task id to detect task change
		std::string m_lastTaskId;

		void Unsubscribe();
		void StopRefreshTimer() noexcept;
		void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender,
										winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
		winrt::fire_and_forget RefreshPeerList();
		void ApplyPeerSnapshot(
			std::string const& taskId,
			std::vector<::OpenNet::Core::Torrent::LibtorrentHandle::TorrentPeerInfo> peers,
			bool forceRefresh);
		void OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const& sender,
								winrt::Windows::Foundation::IInspectable const& args);
		void BanSelectedPeer(std::int64_t durationSeconds);
		void ResetPeerGroups();
		void EnsurePeerGroups();
		winrt::hstring BuildFlagSvg(std::string countryCode);
		void UpdateSortHeaders();
		void SortPeerItems(std::vector<winrt::OpenNet::ViewModels::PeerDisplayItem>& items);
		void AutoSizeColumn(winrt::XamlToolkit::Labs::WinUI::DataColumn const& column);
		winrt::XamlToolkit::Labs::WinUI::DataColumn ColumnForTag(winrt::hstring const& tag);
		void SynchronizePeerRows();
		void ScheduleRowLayoutSynchronization();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskPeersListPage : TaskPeersListPageT<TaskPeersListPage, implementation::TaskPeersListPage>
	{
	};
}
