#include "pch.h"
#include "TrackersSettingsViewModel.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <chrono>

namespace winrt::OpenNet::ViewModels
{
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Foundation::Collections;

    TrackersSettingsViewModel::TrackersSettingsViewModel()
    {
        m_trackers = single_threaded_observable_vector<winrt::hstring>();
        m_subscriptions = single_threaded_observable_vector<winrt::hstring>();
        LoadTrackers();
        LoadSubscriptions();
    }

    IObservableVector<winrt::hstring> TrackersSettingsViewModel::Trackers() const
    {
        return m_trackers;
    }

    IObservableVector<winrt::hstring> TrackersSettingsViewModel::Subscriptions() const
    {
        return m_subscriptions;
    }

    void TrackersSettingsViewModel::SelectedTrackerIndex(int value)
    {
        m_selectedTrackerIndex = value;
    }

    winrt::Windows::Foundation::IAsyncAction TrackersSettingsViewModel::AddCustomTrackerAsync(winrt::hstring name, winrt::hstring url)
    {
        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();

            ::OpenNet::Core::Torrent::TrackerInfo info;
            info.id = L"custom_" + std::to_wstring(
                std::chrono::system_clock::now().time_since_epoch().count());
            info.name = std::wstring(name.c_str());
            info.url = std::wstring(url.c_str());
            info.category = L"Custom";
            info.enabled = true;
            info.addedTime = std::chrono::system_clock::now().time_since_epoch().count();

            trackerManager.AddTracker(info);
            co_await resume_background();
            RefreshTrackers();
        }
        catch (...) {}
    }

    winrt::Windows::Foundation::IAsyncAction TrackersSettingsViewModel::RemoveTrackerAsync(int index)
    {
        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
            auto trackers = trackerManager.GetAllTrackers();

            if (index >= 0 && index < static_cast<int>(trackers.size()))
            {
                trackerManager.RemoveTracker(trackers[index].id);
                co_await resume_background();
                RefreshTrackers();
            }
        }
        catch (...) {}
    }

    winrt::Windows::Foundation::IAsyncAction TrackersSettingsViewModel::SubscribeToTrackerListAsync(winrt::hstring subscriptionUrl, winrt::hstring subscriptionName)
    {
        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
            co_await trackerManager.SubscribeToTrackerListAsync(
                std::wstring(subscriptionUrl.c_str()),
                std::wstring(subscriptionName.c_str()));

            RefreshTrackers();
            LoadSubscriptions();
        }
        catch (...) {}
    }

    winrt::Windows::Foundation::IAsyncAction TrackersSettingsViewModel::RemoveSubscriptionAsync(int index)
    {
        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
            auto subscriptions = trackerManager.GetSubscriptions();

            if (index >= 0 && index < static_cast<int>(subscriptions.size()))
            {
                trackerManager.RemoveSubscription(subscriptions[index].first);
                co_await resume_background();
                RefreshTrackers();
                LoadSubscriptions();
            }
        }
        catch (...) {}
    }

    void TrackersSettingsViewModel::RefreshTrackers()
    {
        LoadTrackers();
    }

    void TrackersSettingsViewModel::LoadTrackers()
    {
        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
            auto trackers = trackerManager.GetAllTrackers();

            m_trackers.Clear();
            for (const auto& tracker : trackers)
            {
                m_trackers.Append(winrt::hstring(tracker.name));
            }
        }
        catch (...) {}
    }

    void TrackersSettingsViewModel::LoadSubscriptions()
    {
        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
            auto subscriptions = trackerManager.GetSubscriptions();

            m_subscriptions.Clear();
            for (const auto& [id, url] : subscriptions)
            {
                m_subscriptions.Append(winrt::hstring(url));
            }
        }
        catch (...) {}
    }
}
