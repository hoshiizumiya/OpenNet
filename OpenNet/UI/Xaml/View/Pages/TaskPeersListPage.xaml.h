#pragma once

import winrt.XamlToolkit.Labs.WinUI;
import winrt.XamlToolkit.WinUI.Animations;
import winrt.XamlToolkit.WinUI.Behaviors;
import winrt.XamlToolkit.WinUI.Controls;
import winrt.XamlToolkit.WinUI.Interactivity;
import winrt.OpenNet.UI.Xaml.Markup;
import winrt.OpenNet.UI.Xaml.Media;
import winrt.OpenNet.UI.Xaml.Behavior;
import winrt.OpenNet.UI.Xaml.Control.Card;
import winrt.OpenNet.UI.Xaml.Control.Effect;
import winrt.OpenNet.UI.Xaml.Control.HomePage.Header;
import winrt.OpenNet.UI.Xaml.Media.Brushes;
import winrt.OpenNet.UI.Xaml.Media.Effects;
import winrt.OpenNet.UI.Xaml.Media.Pipelines;
import winrt.OpenNet.UI.Xaml.Media.Shadows;
import winrt.OpenNet.UI.Xaml.Media.Visuals;
import winrt.Microsoft.UI.Xaml.Controls.AnimatedVisuals;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Composition.SystemBackdrops;
#include "UI/Xaml/View/Pages/TaskPeersListPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct TaskPeersListPage : TaskPeersListPageT<TaskPeersListPage>
    {
        TaskPeersListPage();
        ~TaskPeersListPage();

        void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
        void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

        // Ban peer context menu handlers
        void BanPeer1h_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void BanPeer24h_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void BanPeerPermanent_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
        winrt::event_token m_vmPropertyChangedToken{};

        // Timer for periodic peer refresh
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
        winrt::event_token m_timerTickToken{};

        // Cached observable vector for incremental updates
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_peerItems{ nullptr };

        // Track last known task id to detect task change
        std::string m_lastTaskId;

        void Unsubscribe();
        void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender,
                                        winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
        void RefreshPeerList();
        void OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const& sender,
                                winrt::Windows::Foundation::IInspectable const& args);
        void BanSelectedPeer(winrt::hstring const& description);
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct TaskPeersListPage : TaskPeersListPageT<TaskPeersListPage, implementation::TaskPeersListPage>
    {
    };
}
