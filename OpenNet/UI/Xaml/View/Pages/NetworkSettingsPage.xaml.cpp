#include "pch.h"
#include "UI/Xaml/View/Pages/NetworkSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/NetworkSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/NetworkSettingsPage.g.cpp"
#endif

#include "Core/Torrent/TrackerManager.h"
#include <chrono>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    NetworkSettingsPage::NetworkSettingsPage()
    {
        InitializeComponent();

        m_trackerList = single_threaded_observable_vector<winrt::hstring>();
        m_subscriptionList = single_threaded_observable_vector<winrt::hstring>();

        LoadTrackers();
        LoadSubscriptions();
    }

    IObservableVector<winrt::hstring> NetworkSettingsPage::TrackerList() const
    {
        return m_trackerList;
    }

    IObservableVector<winrt::hstring> NetworkSettingsPage::SubscriptionList() const
    {
        return m_subscriptionList;
    }

    void NetworkSettingsPage::LoadTrackers()
    {
        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
            auto trackers = trackerManager.GetAllTrackers();

            m_trackerList.Clear();
            for (const auto& tracker : trackers)
            {
                m_trackerList.Append(winrt::hstring(tracker.url));
            }
        }
        catch (...) {}
    }

    void NetworkSettingsPage::LoadSubscriptions()
    {
        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
            auto subscriptions = trackerManager.GetSubscriptions();

            m_subscriptionList.Clear();
            for (const auto& [id, url] : subscriptions)
            {
                m_subscriptionList.Append(winrt::hstring(url));
            }
        }
        catch (...) {}
    }

    IAsyncAction NetworkSettingsPage::AddSubscriptionAsync()
    {
        auto lifetime = get_strong();

        auto subscriptionUrl = SubscriptionUrlTextBox().Text();
        auto subscriptionName = SubscriptionNameTextBox().Text();

        if (subscriptionUrl.empty())
        {
            co_return;
        }

        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();

            std::wstring name = subscriptionName.empty() 
                ? L"Subscription" 
                : std::wstring(subscriptionName.c_str());

            co_await trackerManager.SubscribeToTrackerListAsync(
                std::wstring(subscriptionUrl.c_str()),
                name);

            // Clear inputs on UI thread
            DispatcherQueue().TryEnqueue([this]()
            {
                SubscriptionUrlTextBox().Text(L"");
                SubscriptionNameTextBox().Text(L"");

                LoadTrackers();
                LoadSubscriptions();
            });
        }
        catch (...) {}
    }

    void NetworkSettingsPage::AddTrackerButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto trackerUrl = NewTrackerTextBox().Text();

        if (trackerUrl.empty())
        {
            return;
        }

        try
        {
            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();

            ::OpenNet::Core::Torrent::TrackerInfo info;
            info.id = L"custom_" + std::to_wstring(
                std::chrono::system_clock::now().time_since_epoch().count());
            info.name = std::wstring(trackerUrl.c_str());
            info.url = std::wstring(trackerUrl.c_str());
            info.category = L"Custom";
            info.enabled = true;
            info.addedTime = std::chrono::system_clock::now().time_since_epoch().count();

            trackerManager.AddTracker(info);

            NewTrackerTextBox().Text(L"");
            LoadTrackers();
        }
        catch (...) {}
    }

    void NetworkSettingsPage::RemoveTrackerButton_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        try
        {
            auto button = sender.as<Button>();
            auto tag = unbox_value_or<winrt::hstring>(button.Tag(), L"");

            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
            auto trackers = trackerManager.GetAllTrackers();

            // Find by URL
            for (const auto& tracker : trackers)
            {
                if (tracker.url == std::wstring(tag.c_str()))
                {
                    trackerManager.RemoveTracker(tracker.id);
                    break;
                }
            }

            LoadTrackers();
        }
        catch (...) {}
    }

    void NetworkSettingsPage::AddSubscriptionButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        AddSubscriptionAsync();
    }

    void NetworkSettingsPage::RemoveSubscriptionButton_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        try
        {
            auto button = sender.as<Button>();
            auto tag = unbox_value_or<winrt::hstring>(button.Tag(), L"");

            auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
            auto subscriptions = trackerManager.GetSubscriptions();

            // Find by URL
            for (const auto& [id, url] : subscriptions)
            {
                if (url == std::wstring(tag.c_str()))
                {
                    trackerManager.RemoveSubscription(id);
                    break;
                }
            }

            LoadTrackers();
            LoadSubscriptions();
        }
        catch (...) {}
    }

    void NetworkSettingsPage::RefreshTrackersButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        LoadTrackers();
        LoadSubscriptions();
    }
}
