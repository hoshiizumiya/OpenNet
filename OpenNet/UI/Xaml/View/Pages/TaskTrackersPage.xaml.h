#pragma once

#include "UI/Xaml/View/Pages/TaskTrackersPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct TaskTrackersPage : TaskTrackersPageT<TaskTrackersPage>
    {
        TaskTrackersPage();
        ~TaskTrackersPage();

        void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

    private:
        winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
        winrt::event_token m_vmPropertyChangedToken{};

        winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
        winrt::event_token m_timerTickToken{};

        void Unsubscribe();
        void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender,
                                        winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
        void RefreshTrackerList();
        void OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const& sender,
                                winrt::Windows::Foundation::IInspectable const& args);
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct TaskTrackersPage : TaskTrackersPageT<TaskTrackersPage, implementation::TaskTrackersPage>
    {
    };
}
