#pragma once
#include "pch.h"
import winrt.OpenNet.UI.Xaml.Control;
#include "UI/Xaml/View/Pages/NetworkSettingsPage.g.h"
#include "ViewModels/NetworkSettingsViewModel.h"

import winrt.Windows.Foundation.Collections;
import winrt.Microsoft.UI.Dispatching;
import OpenNet.Core.Torrent.TrackerManager;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct NetworkSettingsPage : NetworkSettingsPageT<NetworkSettingsPage>
    {
        NetworkSettingsPage();
        ~NetworkSettingsPage();

        winrt::OpenNet::ViewModels::NetworkSettingsViewModel ViewModel() { return m_viewModel ? m_viewModel : (m_viewModel = winrt::OpenNet::ViewModels::NetworkSettingsViewModel()); }

        // Tracker properties
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> TrackerList() const;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> SubscriptionList() const;

        // Event Handlers
        void AddTrackerButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RemoveTrackerButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void AddSubscriptionButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RemoveSubscriptionButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RefreshTrackersButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void AutoAddTrackersToggle_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void AddPresetTrackerListButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ListenEndpoint_ValueChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);

    private:
        void LoadTrackers();
        void LoadSubscriptions();
        winrt::fire_and_forget InitializeTrackerManagerAsync();
        winrt::Windows::Foundation::IAsyncAction AddSubscriptionAsync();
        winrt::Windows::Foundation::IAsyncAction AddPresetSubscriptionAsync();
        void LoadListenEndpointSettings();
        void SaveAndApplyListenEndpoints();
        void RefreshRuntimeListenStatus();

        winrt::OpenNet::ViewModels::NetworkSettingsViewModel m_viewModel{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_trackerList;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_subscriptionList;
        bool m_loadingTrackerSettings{ true };
        bool m_loadingListenPort{ true };
        bool m_listenSettingsDirty{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer
            m_listenStatusTimer{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer
            m_listenApplyTimer{ nullptr };
    };
}
namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct NetworkSettingsPage : NetworkSettingsPageT<NetworkSettingsPage, implementation::NetworkSettingsPage>
    {
    };
}
