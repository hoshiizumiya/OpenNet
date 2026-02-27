#pragma once
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include "Core/Torrent/TrackerManager.h"

namespace winrt::OpenNet::ViewModels
{
    struct TrackersSettingsViewModel
    {
        TrackersSettingsViewModel();

        // Properties
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> Trackers() const;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> Subscriptions() const;
        int SelectedTrackerIndex() const { return m_selectedTrackerIndex; }
        void SelectedTrackerIndex(int value);

        // Commands
        winrt::Windows::Foundation::IAsyncAction AddCustomTrackerAsync(winrt::hstring name, winrt::hstring url);
        winrt::Windows::Foundation::IAsyncAction RemoveTrackerAsync(int index);
        winrt::Windows::Foundation::IAsyncAction SubscribeToTrackerListAsync(winrt::hstring subscriptionUrl, winrt::hstring subscriptionName);
        winrt::Windows::Foundation::IAsyncAction RemoveSubscriptionAsync(int index);
        void RefreshTrackers();

    private:
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_trackers;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_subscriptions;
        int m_selectedTrackerIndex{ 0 };

        void LoadTrackers();
        void LoadSubscriptions();
    };
}
