#pragma once

#include "UI/Xaml/View/Pages/TaskFilesPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskFilesPage : TaskFilesPageT<TaskFilesPage>
	{
		TaskFilesPage();
		~TaskFilesPage();

		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

		// ComboBox selection changed for file priority
		void FilePriority_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender,
										   winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

	private:
		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_vmPropertyChangedToken{};

		winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
		winrt::event_token m_timerTickToken{};

		// Suppress priority change events during list refresh
		bool m_isRefreshing{ false };

		void Unsubscribe();
		void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender,
										winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
		void RefreshFileList();
		void OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const& sender,
								winrt::Windows::Foundation::IInspectable const& args);
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskFilesPage : TaskFilesPageT<TaskFilesPage, implementation::TaskFilesPage>
	{
	};
}
