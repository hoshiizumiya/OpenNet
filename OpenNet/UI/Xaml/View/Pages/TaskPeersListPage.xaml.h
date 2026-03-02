#pragma once

#include "UI/Xaml/View/Pages/TaskPeersListPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct TaskPeersListPage : TaskPeersListPageT<TaskPeersListPage>
    {
        TaskPeersListPage();
        ~TaskPeersListPage();

        void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

    private:
        winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
        winrt::event_token m_vmPropertyChangedToken{};

        // Timer for periodic peer refresh
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
        winrt::event_token m_timerTickToken{};

        void Unsubscribe();
        void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender,
                                        winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
        void RefreshPeerList();
        void OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const& sender,
                                winrt::Windows::Foundation::IInspectable const& args);

        // Simple data holder for peer rows (displayed via Binding)
        struct PeerDisplayItem
        {
            winrt::hstring IP;
            winrt::hstring Client;
            winrt::hstring Progress;
            winrt::hstring DLSpeed;
            winrt::hstring ULSpeed;
            winrt::hstring Downloaded;
        };
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct TaskPeersListPage : TaskPeersListPageT<TaskPeersListPage, implementation::TaskPeersListPage>
    {
    };
}
