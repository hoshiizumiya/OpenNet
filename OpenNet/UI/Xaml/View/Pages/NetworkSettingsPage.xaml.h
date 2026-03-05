#pragma once
#include "pch.h"
#include "UI/Xaml/View/Pages/NetworkSettingsPage.g.h"
#include "ViewModels/NetworkSettingsViewModel.h"
#include "Core/Torrent/TrackerManager.h"
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct NetworkSettingsPage : NetworkSettingsPageT<NetworkSettingsPage>
    {
        NetworkSettingsPage();

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

    private:
        void LoadTrackers();
        void LoadSubscriptions();
        winrt::Windows::Foundation::IAsyncAction AddSubscriptionAsync();

        winrt::OpenNet::ViewModels::NetworkSettingsViewModel m_viewModel{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_trackerList;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_subscriptionList;
    };
}
namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct NetworkSettingsPage : NetworkSettingsPageT<NetworkSettingsPage, implementation::NetworkSettingsPage>
    {
    };
}
