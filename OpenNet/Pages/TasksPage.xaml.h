#pragma once

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

		// SpeedGraph size changed handler
		void TaskSpeedGraph_SizeChanged(winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);

	private:
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_addTaskToken{};
		winrt::event_token m_selectedTaskPropertyChangedToken{};
		winrt::OpenNet::ViewModels::TaskViewModel m_currentSubscribedTask{ nullptr };

		void OnAddTaskRequested(winrt::Windows::Foundation::IInspectable const&, winrt::hstring const&);
		winrt::fire_and_forget ShowAddMagnetDialog();
		Windows::Foundation::IAsyncOperation<hstring> PickFolderAsync();

		// Update the SpeedGraph with selected task's data
		void UpdateSpeedGraphForSelectedTask();
		
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
