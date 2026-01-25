#pragma once

// Ensure custom control types are declared before including the generated XAML header.
// The generated header (`Pages/TasksPage.g.h`) uses `winrt::OpenNet::Controls::SpeedGraph::SpeedGraph`
// in its declarations. If that type is not visible at the point the generated header is included
// the compiler will fail with errors such as "symbol must be a type" or "variable cannot have type void".
#include "../Controls/SpeedGraph/SpeedGraph.xaml.h"
#include "UI/Xaml/View/Pages/TaskSummaryPage.xaml.h"
#include "Pages/TasksPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::Pages::implementation
{
	struct TasksPage : TasksPageT<TasksPage>
	{
		TasksPage();
		~TasksPage();

		// Expose strongly-typed ViewModel for x:Bind
		winrt::OpenNet::ViewModels::TasksViewModel ViewModel() const { return m_viewModel; }

		// Filter nav selection (must be public for XAML wiring)
		void FilterNavView_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender,
			winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);

		// Task list selection changed handler
		void TasksList_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

		// Show magnet link dialog
		winrt::Windows::Foundation::IAsyncAction ShowAddMagnetDialog();

	private:
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_addTaskToken{};
		winrt::event_token m_selectedTaskPropertyChangedToken{};
		winrt::OpenNet::ViewModels::TaskViewModel m_currentSubscribedTask{ nullptr };

		winrt::Windows::Foundation::IAsyncAction OnAddTaskRequested(winrt::Windows::Foundation::IInspectable const&, winrt::hstring const&);


		// Subscribe/unsubscribe to selected task's property changes
		void SubscribeToSelectedTaskChanges(winrt::OpenNet::ViewModels::TaskViewModel const& task);
		void UnsubscribeFromSelectedTaskChanges();
		void OnSelectedTaskPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
	};
}

namespace winrt::OpenNet::Pages::factory_implementation
{
	struct TasksPage : TasksPageT<TasksPage, implementation::TasksPage>
	{
	};
}
