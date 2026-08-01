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

		// SelectorBar (Summary / PeersList) tab switch
		void Task_SelectBar_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::SelectorBar const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const& args);

		// Context menu item handlers
		void TasksColumnHeader_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);
		void TasksColumnHeader_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void TasksColumnMenuFlyout_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void TasksColumnMenuFlyout_Closed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void TasksContextMenuFlyout_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		void TasksColumnAutoSizeSelectedWidth_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void TasksColumnAutoSizeAllWidth_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void TasksColumnDisplayItemsReset_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		winrt::Windows::Foundation::IAsyncAction RenameTaskMenuItem_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void MoveTaskMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void OpenTaskLocationMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction PropertiesMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		// Make these handlers public so XAML generated code can bind to them
		winrt::Windows::Foundation::IAsyncAction MenuItemAddFromLink_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction MenuItemAddFromFile_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction MenuItemAddFromHttp_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		void ViewTasksPagePortTestAppBarButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void ViewTasksPageSettingsAppBarButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		void PortTestKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void SettingKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
		void SearchKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

	private:
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

		winrt::event_token m_addTaskToken{};

		// Handle when ViewModel requests adding a new task
		winrt::Windows::Foundation::IAsyncAction OnAddTaskRequested(winrt::Windows::Foundation::IInspectable const&, winrt::hstring const&);

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
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TasksPage : TasksPageT<TasksPage, implementation::TasksPage>
	{
	};
}
