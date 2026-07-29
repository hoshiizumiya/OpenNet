#include "XamlWorkaround.h"
#include "UI/Xaml/View/Pages/NetworkSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/NetworkSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/NetworkSettingsPage.g.cpp"
#endif

import winrt.Microsoft.UI.Dispatching;
import OpenNet.Core.P2PManager;
import OpenNet.Core.TorrentSettings;

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

        auto& trackerManager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
        AutoAddTrackersToggle().IsOn(trackerManager.AutoAddToNewTorrents());
        m_loadingTrackerSettings = false;

        auto& torrentSettingsManager =
            ::OpenNet::Core::TorrentSettingsManager::Instance();
        torrentSettingsManager.Load();
        auto torrentSettings = torrentSettingsManager.Get();
        int listenPort = 0;
        auto const marker = torrentSettings.listenInterfaces.find("0.0.0.0:");
        if (marker != std::string::npos)
        {
            auto const start = marker + std::string_view{ "0.0.0.0:" }.size();
            auto const end = torrentSettings.listenInterfaces.find(',', start);
            try
            {
                listenPort = std::stoi(
                    torrentSettings.listenInterfaces.substr(start, end - start));
            }
            catch (...)
            {
                listenPort = 0;
            }
        }
        if (listenPort < 1024 || listenPort > 65535)
        {
            listenPort = 6881;
            torrentSettings.listenInterfaces =
                std::format("0.0.0.0:{0},[::]:{0}", listenPort);
            torrentSettingsManager.Set(torrentSettings);
        }
        ListenPortBox().Value(static_cast<double>(listenPort));
        m_loadingListenPort = false;

        LoadTrackers();
        LoadSubscriptions();
        InitializeTrackerManagerAsync();
    }

    winrt::fire_and_forget NetworkSettingsPage::InitializeTrackerManagerAsync()
    {
        auto lifetime = get_strong();
        try
        {
            co_await ::OpenNet::Core::Torrent::TrackerManager::Instance()
                .InitializeAsync();
            DispatcherQueue().TryEnqueue([this]()
            {
                LoadTrackers();
                LoadSubscriptions();
            });
        }
        catch (...)
        {
        }
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

    void NetworkSettingsPage::AutoAddTrackersToggle_Toggled(
        IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingTrackerSettings)
        {
            return;
        }
        ::OpenNet::Core::Torrent::TrackerManager::Instance()
            .AutoAddToNewTorrents(AutoAddTrackersToggle().IsOn());
    }

    IAsyncAction NetworkSettingsPage::AddPresetSubscriptionAsync()
    {
        auto lifetime = get_strong();
        auto selected = PresetTrackerListComboBox().SelectedItem()
            .try_as<ComboBoxItem>();
        if (!selected)
        {
            co_return;
        }

        auto url = unbox_value_or<hstring>(selected.Tag(), L"");
        auto name = unbox_value_or<hstring>(selected.Content(), L"Tracker list");
        if (url.empty())
        {
            co_return;
        }

        auto& manager = ::OpenNet::Core::Torrent::TrackerManager::Instance();
        co_await manager.InitializeAsync();
        co_await manager.SubscribeToTrackerListAsync(
            std::wstring{ url.c_str() },
            std::wstring{ name.c_str() });
        LoadTrackers();
        LoadSubscriptions();
    }

    void NetworkSettingsPage::AddPresetTrackerListButton_Click(
        IInspectable const&, RoutedEventArgs const&)
    {
        AddPresetSubscriptionAsync();
    }

    void NetworkSettingsPage::ListenPortBox_ValueChanged(
        NumberBox const&,
        NumberBoxValueChangedEventArgs const& args)
    {
        if (m_loadingListenPort || std::isnan(args.NewValue()))
        {
            return;
        }

        auto const port = static_cast<int>(std::clamp(
            args.NewValue(), 1024.0, 65535.0));
        auto& manager = ::OpenNet::Core::TorrentSettingsManager::Instance();
        manager.Load();
        auto settings = manager.Get();
        settings.listenInterfaces =
            std::format("0.0.0.0:{0},[::]:{0}", port);
        manager.Set(settings);

        if (auto core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
        {
            auto pack = core->GetSettings();
            pack.set_str(
                libtorrent::settings_pack::listen_interfaces,
                settings.listenInterfaces);
            core->ApplySettings(pack);
        }
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
