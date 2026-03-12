#pragma once

// Ensure custom control types are declared before including the generated XAML header.
// The generated header (`Pages/TasksPage.g.h`) uses `winrt::OpenNet::Controls::SpeedGraph::SpeedGraph`
// in its declarations. If that type is not visible at the point the generated header is included
// the compiler will fail with errors such as "symbol must be a type" or "variable cannot have type void".
#include "../Controls/SpeedGraph/SpeedGraph.xaml.h"
#include "UI/Xaml/View/Pages/TaskSummaryPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskPeersListPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskTrackersPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskFilesPage.xaml.h"
#include "UI/Xaml/View/Pages/TasksPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TasksPage : TasksPageT<TasksPage>
	{
		TasksPage();
		~TasksPage();

		// Expose strongly-typed ViewModel for x:Bind
		winrt::OpenNet::ViewModels::TasksViewModel ViewModel() const
		{
			return m_viewModel;
		}

		// Filter nav selection (must be public for XAML wiring)
		void FilterNavView_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender, winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);

		// Task list selection changed handler
		void TasksList_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

		// Task list right-click handlers
		void TasksList_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);

		// Search
		void SearchBox_TextChanged(winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& sender, winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& args);

		// SelectorBar (Summary / PeersList) tab switch
		void Task_SelectBar_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::SelectorBar const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const& args);

		// Context menu item handlers
		winrt::Windows::Foundation::IAsyncAction RenameTaskMenuItem_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		void MoveTaskMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		void OpenTaskLocationMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		winrt::Windows::Foundation::IAsyncAction PropertiesMenuItem_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		// Make these handlers public so XAML generated code can bind to them
		winrt::Windows::Foundation::IAsyncAction MenuItemAddFromLink_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction MenuItemAddFromFile_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction MenuItemAddFromHttp_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

	private:
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
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
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TasksPage : TasksPageT<TasksPage, implementation::TasksPage>
	{
	};
}
