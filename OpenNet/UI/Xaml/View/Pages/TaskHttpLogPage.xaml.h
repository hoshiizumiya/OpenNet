#pragma once

#include "UI/Xaml/View/Pages/TaskHttpLogPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskHttpLogPage : TaskHttpLogPageT<TaskHttpLogPage>
	{
		~TaskHttpLogPage();
		void InitializeComponent();
		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);

	private:
		void Refresh();
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::Microsoft::UI::Xaml::DispatcherTimer m_timer{ nullptr };
		winrt::event_token m_tickToken{};
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_entries{ nullptr };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskHttpLogPage : TaskHttpLogPageT<TaskHttpLogPage, implementation::TaskHttpLogPage>
	{
	};
}
