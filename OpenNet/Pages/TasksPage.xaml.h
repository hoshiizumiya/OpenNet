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

	private:
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_addTaskToken{};

		void OnAddTaskRequested(winrt::Windows::Foundation::IInspectable const&, winrt::hstring const&);
		winrt::fire_and_forget ShowAddMagnetDialog();
		Windows::Foundation::IAsyncOperation<hstring> PickFolderAsync();
	};
}

namespace winrt::OpenNet::Pages::factory_implementation
{
	struct TasksPage : TasksPageT<TasksPage, implementation::TasksPage>
	{
	};
}
