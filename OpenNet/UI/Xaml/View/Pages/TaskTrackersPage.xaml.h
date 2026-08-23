#pragma once

import winrt.XamlToolkit.Labs.WinUI;
import winrt.OpenNet.UI.Xaml.Control.Effect;
#include "UI/Xaml/View/Pages/TaskTrackersPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskTrackersPage : TaskTrackersPageT<TaskTrackersPage>
	{
		TaskTrackersPage();
		~TaskTrackersPage();
		void InitializeComponent();

		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
		void ColumnHeader_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ColumnHeader_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);
		void ColumnMenu_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void AutoSizeSelectedColumn_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void AutoSizeAllColumns_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ColumnVisibility_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ResetColumns_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void TrackerDataRow_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void TrackerRow_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);
		void TrackerMenu_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void TrackerLogButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction EditTrackers_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void UpdateTracker_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction RemoveTracker_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ViewLog_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ClearTrackerLog_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void ClearTaskTrackerLogs_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction ClearAllTrackerLogs_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction RemoveUnreachableTrackers_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_vmPropertyChangedToken{};

		winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
		winrt::event_token m_timerTickToken{};
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_trackerItems{ nullptr };
		winrt::hstring m_sortColumn;
		int m_sortDirection{};
		winrt::XamlToolkit::Labs::WinUI::DataColumn m_contextColumn{ nullptr };
		std::string m_lastTaskId;
		std::size_t m_lastTrackerSnapshotHash{};
		bool m_hasTrackerSnapshot{};
		bool m_httpRefreshInFlight{};
		std::chrono::steady_clock::time_point m_lastTrackerRefresh{};

		void Unsubscribe();
		void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
		void RefreshTrackerList();
		winrt::fire_and_forget RefreshHttpServersAsync(std::string gid);
		void OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void UpdateSortHeaders();
		void AutoSizeColumn(winrt::XamlToolkit::Labs::WinUI::DataColumn const& column);
		winrt::XamlToolkit::Labs::WinUI::DataColumn ColumnForTag(winrt::hstring const& tag);
		void SynchronizeTrackerRows();
		bool TryGetTaskContext(std::string& taskId, winrt::hstring& taskName) const;
		winrt::hstring SelectedTrackerUrl();
		void OpenTrackerLog(winrt::hstring const& trackerUrl);
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskTrackersPage : TaskTrackersPageT<TaskTrackersPage, implementation::TaskTrackersPage>
	{
	};
}
