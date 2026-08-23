#pragma once

import winrt.XamlToolkit.Labs.WinUI;
import OpenNet.ViewModels.ObservableMixin;

// Ensure custom control types are declared before including the generated XAML header.
// The generated header (`Pages/TasksPage.g.h`) uses `winrt::OpenNet::Controls::SpeedGraph::SpeedGraph`
// in its declarations. If that type is not visible at the point the generated header is included
// the compiler will fail with errors such as "symbol must be a type" or "variable cannot have type void".
#include "Controls/SpeedGraph/SpeedGraph.xaml.h"
#include "UI/Xaml/View/Pages/TaskSummaryPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskSpeedGraphPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskPieceMapPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskPeersListPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskTrackersPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskFilesPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskHttpLogPage.xaml.h"
#include "UI/Xaml/Control/TaskStatusIndicator.xaml.h"
#include "UI/Xaml/View/Pages/TasksPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TasksPage : TasksPageT<TasksPage>, ::OpenNet::ViewModels::ObservableMixin<TasksPage>
	{
	public:
		TasksPage();
		~TasksPage();
		using ::OpenNet::ViewModels::ObservableMixin<TasksPage>::SetProperty;
		using ::OpenNet::ViewModels::ObservableMixin<TasksPage>::RaisePropertyChanged;

		void InitializeComponent();
		winrt::fire_and_forget Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&);
		void DataTable_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void GridSplitter_PointerReleased(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

		// Expose strongly-typed ViewModel for x:Bind
		winrt::OpenNet::ViewModels::TasksViewModel ViewModel() const
		{
			return m_viewModel;
		}

		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingNameTextBlock();
		void TextWrappingNameTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingSizeTextBlock();
		void TextWrappingSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingProgressTextBlock();
		void TextWrappingProgressTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingDownloadSizeTextBlock();
		void TextWrappingDownloadSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingUploadSizeTextBlock();
		void TextWrappingUploadSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingTotalDownloadSizeTextBlock();
		void TextWrappingTotalDownloadSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingTotalUploadSizeTextBlock();
		void TextWrappingTotalUploadSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingDLRateTextBlock();
		void TextWrappingDLRateTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingULRateTextBlock();
		void TextWrappingULRateTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingRemainingTextBlock();
		void TextWrappingRemainingTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);
		winrt::Microsoft::UI::Xaml::TextWrapping TextWrappingAddDateTextBlock();
		void TextWrappingAddDateTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value);

		Microsoft::UI::Xaml::Controls::TabViewWidthMode TaskTabViewTabWidthMode();
		void TaskTabViewTabWidthMode(Microsoft::UI::Xaml::Controls::TabViewWidthMode const& value);

		// Filter nav selection (must be public for XAML wiring)
		void FilterNavView_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender, winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);
		// Task list container content changing
		void TasksList_ContainerContentChanging(winrt::Microsoft::UI::Xaml::Controls::ListViewBase const&, winrt::Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const& args);
		// Task list selection changed handler
		void TasksList_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
		// Task list right-click handlers
		void TasksList_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);

		// Search
		void SearchBox_TextChanged(winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& sender, winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& args);

		// TabView
		void Task_TabView_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
		void TaskTabViewContextFlyout_Opening(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::Foundation::IInspectable const&);
		void TabViewContextFlyoutRadioMenuFlyoutItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		// Context menu item handlers
		void TasksColumnHeader_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);
		void TasksColumnHeader_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void TasksColumnMenuFlyout_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void TasksColumnMenuFlyout_Closed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void TasksColumnVisibility_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void TaskDataRow_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void TasksContextMenuFlyout_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void TasksColumnAutoSizeSelectedWidth_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void TasksColumnAutoSizeAllWidth_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void TasksColumnDisplayItemsReset_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		void StartTaskMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void StopTaskMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void PreviewTaskMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void UpdateTrackerMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void SuperSeedModeMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void SequentialDownloadMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void OpenTaskFileMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void ManualHashCheckMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction SaveTorrentAsMenuItem_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction DeleteTaskMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction DeleteTaskButton_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction RenameTaskMenuItem_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void MoveTaskMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void OpenTaskLocationMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::fire_and_forget SearchOnlineMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void CopyMagnetUriMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void CopyTaskNameMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void CopyTaskHashMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void CopyTaskPathMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction DiskUsageInfoMenuItem_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction PropertiesMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		// Make these handlers public so XAML generated code can bind to them
		winrt::Windows::Foundation::IAsyncAction MenuItemAddFromLink_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction MenuItemAddFromFile_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction MenuItemAddFromHttp_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction CreateTorrentMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		void ViewTasksPagePortTestAppBarButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void ViewTasksPageSettingsAppBarButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		void TaskNewFromUrlKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void TaskNewFromFileKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void TaskNewFromHttpKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void TaskStartKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void TaskPauseKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void TaskDeleteKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void CreateTorrentKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void PortTestKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void SettingKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void SearchKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

	private:
		enum class KeyboardAction
		{
			NewFromUrl,
			NewFromFile,
			NewFromHttp,
			Delete,
			CreateTorrent,
		};

		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };

		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingNameTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingSizeTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingProgressTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingDownloadSizeTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingUploadSizeTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingTotalDownloadSizeTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingTotalUploadSizeTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingDLRateTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingULRateTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingRemainingTextBlock;
		winrt::Microsoft::UI::Xaml::TextWrapping m_textWrappingAddDateTextBlock;

		Microsoft::UI::Xaml::Controls::TabViewWidthMode m_taskTabViewTabWidthMode{ Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal };
		bool m_taskTabViewTabWidthModeInitialized{};

		winrt::event_token m_addTaskToken{};
		winrt::event_token m_taskDeletionFailedToken{};

		static constexpr std::string_view TaskTabStateKey =	"TasksPage.TaskTabs";
		bool m_restoringTabViewState{ true };
		int32_t m_previousSelectedIndex{ -1 };

		// Handle when ViewModel requests adding a new task
		winrt::Windows::Foundation::IAsyncAction OnAddTaskRequested(winrt::Windows::Foundation::IInspectable const&, winrt::hstring const&);
		winrt::fire_and_forget InvokeKeyboardActionAsync(KeyboardAction action);
		void OnTaskDeletionFailed(winrt::Windows::Foundation::IInspectable const&, winrt::hstring const& message);

		// Process the torrent link and open the metadata check window
		void ProcessAndShowTorrentMetadataWindow(winrt::hstring const& torrentLink);

		// File operation helpers
		winrt::Windows::Foundation::IAsyncAction PerformMoveTaskAsync();
		winrt::Windows::Foundation::IAsyncAction ShowTaskPropertiesAsync();

		// Column width persistence
		void RestoreColumnWidths();
		void SaveColumnWidths();
		void AutoSizeTaskColumn(winrt::XamlToolkit::Labs::WinUI::DataColumn const& column);
		void AutoSizeAllTaskColumns();
		bool HasFilteredTasks();
		winrt::XamlToolkit::Labs::WinUI::DataColumn ColumnForTag(winrt::hstring const& tag);
		void SetColumnSetting(winrt::hstring const& tag, bool value);
		void SynchronizeTaskRows();
		void UpdateTaskDetailTabs();

		struct PersistedScrollState
		{
			double itemContainerHeight{ -1.0 };
			winrt::hstring itemKey;
			winrt::hstring position;
		};

		// Each navigation filter owns an independent scroll position.
		static std::map<std::wstring, PersistedScrollState> s_persistedScrollStates;
		winrt::hstring m_currentFilterKey{ L"AllTasks" };
		winrt::hstring m_savingFilterKey;
		winrt::hstring m_restoringFilterKey;
		std::uint64_t m_scrollRestoreGeneration{ 0 };
		bool m_isRestoringScrollPosition{ false };

		winrt::XamlToolkit::Labs::WinUI::DataColumn m_contextColumn{ nullptr };
		winrt::hstring m_sortColumn;
		int m_sortDirection{};
		bool m_isApplyingSort{};
		bool m_sortPending{};
		winrt::event_token m_filteredTasksChangedToken{};

		void UpdateTaskSortHeaders();
		void SortFilteredTasks();
		PersistedScrollState& ScrollStateFor(winrt::hstring const& filterKey);
		void SaveScrollPosition(winrt::hstring const& filterKey);
		winrt::fire_and_forget RestoreScrollPositionAsync(winrt::hstring filterKey);
		void ClearRestoredItemContainerHeight();
		void CancelScrollRestore();
		winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::IInspectable> GetItem(hstring const& key);
		hstring GetKey(IInspectable const& object);
		void UpdateTaskTabViewContextFlyout();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TasksPage : TasksPageT<TasksPage, implementation::TasksPage>
	{
	};
}
