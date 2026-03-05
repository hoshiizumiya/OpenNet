#pragma once

#include "../Controls/SpeedGraph/SpeedGraph.xaml.h"
#include "UI/Xaml/View/Pages/TaskSummaryPage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskSummaryPage : TaskSummaryPageT<TaskSummaryPage>
	{
		TaskSummaryPage();
		~TaskSummaryPage();
	public:

		// SpeedGraph xaml size changed handler
		void TaskSpeedGraph_SizeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);
		// Update the SpeedGraph with selected task's data
		void UpdateSpeedGraphForSelectedTask();
		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
	private:
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_vmPropertyChangedToken{};

		// Subscription to SelectedTask's PropertyChanged (ProgressPercent / DownloadSpeedKB)
		winrt::OpenNet::ViewModels::TaskViewModel m_currentSubscribedTask{ nullptr };
		winrt::event_token m_taskPropertyChangedToken{};

		void Unsubscribe();
		void UnsubscribeTask();
		void SubscribeToTask(winrt::OpenNet::ViewModels::TaskViewModel const& task);
		void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender,
										winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
		void OnTaskPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender,
								   winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);

	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskSummaryPage : TaskSummaryPageT<TaskSummaryPage, implementation::TaskSummaryPage>
	{
	};
}
