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

namespace
{
    struct ParsedListenEndpoints
    {
        std::string ipv4Address{ "0.0.0.0" };
        std::uint32_t ipv4Port{};
        std::string ipv6Address{ "::" };
        std::uint32_t ipv6Port{};
        bool hasIpv4{};
        bool hasIpv6{};
    };

    std::string_view Trim(std::string_view value)
    {
        auto const first = value.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos)
        {
            return {};
        }
        auto const last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    bool TryParsePort(std::string_view value, std::uint32_t& port)
    {
        value = Trim(value);
        if (value.empty())
        {
            return false;
        }

        std::uint32_t parsed{};
        auto const [end, error] = std::from_chars(
            value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} ||
            end != value.data() + value.size() ||
            parsed > 65535)
        {
            return false;
        }
        port = parsed;
        return true;
    }

    ParsedListenEndpoints ParseListenEndpoints(std::string_view value)
    {
        ParsedListenEndpoints parsed;
        std::size_t offset{};
        while (offset <= value.size())
        {
            auto const comma = value.find(',', offset);
            auto const segment = Trim(value.substr(
                offset,
                comma == std::string_view::npos
                    ? value.size() - offset
                    : comma - offset));
            if (!segment.empty())
            {
                std::string_view address;
                std::string_view portText;
                bool ipv6{};

                if (segment.front() == '[')
                {
                    auto const closing = segment.find(']');
                    if (closing != std::string_view::npos &&
                        closing + 1 < segment.size() &&
                        segment[closing + 1] == ':')
                    {
                        address = segment.substr(1, closing - 1);
                        portText = segment.substr(closing + 2);
                        ipv6 = true;
                    }
                }
                else
                {
                    auto const colon = segment.rfind(':');
                    if (colon != std::string_view::npos)
                    {
                        address = segment.substr(0, colon);
                        portText = segment.substr(colon + 1);
                    }
                }

                std::uint32_t port{};
                if (!address.empty() && TryParsePort(portText, port))
                {
                    if (ipv6)
                    {
                        parsed.ipv6Address.assign(address);
                        parsed.ipv6Port = port;
                        parsed.hasIpv6 = true;
                    }
                    else
                    {
                        parsed.ipv4Address.assign(address);
                        parsed.ipv4Port = port;
                        parsed.hasIpv4 = true;
                    }
                }
            }

            if (comma == std::string_view::npos)
            {
                break;
            }
            offset = comma + 1;
        }

        if (parsed.hasIpv4 && !parsed.hasIpv6)
        {
            parsed.ipv6Port = parsed.ipv4Port;
        }
        else if (parsed.hasIpv6 && !parsed.hasIpv4)
        {
            parsed.ipv4Port = parsed.ipv6Port;
        }
        return parsed;
    }
}

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

        LoadListenEndpointSettings();

        auto weak = get_weak();
        m_listenStatusTimer = DispatcherQueue().CreateTimer();
        m_listenStatusTimer.IsRepeating(true);
        m_listenStatusTimer.Interval(std::chrono::milliseconds(750));
        m_listenStatusTimer.Tick([weak](auto const&, auto const&)
        {
            if (auto self = weak.get())
            {
                self->RefreshRuntimeListenStatus();
            }
        });
        m_listenApplyTimer = DispatcherQueue().CreateTimer();
        m_listenApplyTimer.IsRepeating(false);
        m_listenApplyTimer.Interval(std::chrono::milliseconds(450));
        m_listenApplyTimer.Tick([weak](auto const& sender, auto const&)
        {
            sender.as<Microsoft::UI::Dispatching::DispatcherQueueTimer>()
                .Stop();
            if (auto self = weak.get())
            {
                self->SaveAndApplyListenEndpoints();
            }
        });
        Loaded([weak](auto const&, auto const&)
        {
            if (auto self = weak.get())
            {
                self->RefreshRuntimeListenStatus();
                self->m_listenStatusTimer.Start();
            }
        });
        Unloaded([weak](auto const&, auto const&)
        {
            if (auto self = weak.get())
            {
                self->m_listenStatusTimer.Stop();
                self->m_listenApplyTimer.Stop();
                if (self->m_listenSettingsDirty)
                {
                    self->SaveAndApplyListenEndpoints();
                }
            }
        });

        LoadTrackers();
        LoadSubscriptions();
        InitializeTrackerManagerAsync();
    }

    NetworkSettingsPage::~NetworkSettingsPage()
    {
        if (m_listenStatusTimer)
        {
            m_listenStatusTimer.Stop();
        }
        if (m_listenApplyTimer)
        {
            m_listenApplyTimer.Stop();
        }
    }

    void NetworkSettingsPage::LoadListenEndpointSettings()
    {
        auto& manager = ::OpenNet::Core::TorrentSettingsManager::Instance();
        manager.Load();
        auto const settings = manager.Get();
        auto const endpoints = ParseListenEndpoints(settings.listenInterfaces);

        m_loadingListenPort = true;
        IPv4ListenEndpoint().Address(to_hstring(endpoints.ipv4Address));
        IPv4ListenEndpoint().Port(endpoints.ipv4Port);
        IPv6ListenEndpoint().Address(to_hstring(endpoints.ipv6Address));
        IPv6ListenEndpoint().Port(endpoints.ipv6Port);
        ViewModel().ListenPort(
            static_cast<std::uint16_t>(endpoints.ipv4Port));
        m_loadingListenPort = false;

        RefreshRuntimeListenStatus();
    }

    void NetworkSettingsPage::SaveAndApplyListenEndpoints()
    {
        if (!IPv4ListenEndpoint().Validate() ||
            !IPv6ListenEndpoint().Validate())
        {
            ActualListenStatusText().Text(
                L"Fix the invalid endpoint before the listener is updated.");
            return;
        }

        auto const ipv4 =
            to_string(IPv4ListenEndpoint().NormalizedEndpoint());
        auto const ipv6 =
            to_string(IPv6ListenEndpoint().NormalizedEndpoint());
        auto const listenInterfaces = ipv4 + "," + ipv6;

        auto& manager = ::OpenNet::Core::TorrentSettingsManager::Instance();
        manager.Load();
        auto settings = manager.Get();
        if (settings.listenInterfaces != listenInterfaces)
        {
            settings.listenInterfaces = listenInterfaces;
            manager.Set(settings);
        }

        if (auto core =
            ::OpenNet::Core::P2PManager::Instance().TorrentCore();
            core && core->IsRunning())
        {
            auto pack = core->GetSettings();
            pack.set_str(
                libtorrent::settings_pack::listen_interfaces,
                listenInterfaces);
            pack.set_bool(
                libtorrent::settings_pack::listen_system_port_fallback,
                false);
            core->ApplySettings(pack);
            core->RefreshPortMappings();
            ActualListenStatusText().Text(
                L"Applying the endpoint configuration…");
        }
        else
        {
            ActualListenStatusText().Text(
                L"Saved. The endpoints will be applied when BitTorrent starts.");
        }
        m_listenSettingsDirty = false;
    }

    void NetworkSettingsPage::RefreshRuntimeListenStatus()
    {
        auto const ipv4Valid = IPv4ListenEndpoint().Validate();
        auto const ipv6Valid = IPv6ListenEndpoint().Validate();
        auto const inputsValid = ipv4Valid && ipv6Valid;

        auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
        if (!core || !core->IsRunning())
        {
            ActualListenPortText().Text(L"Not running");
            ActualListenEndpointText().Text(
                L"The BitTorrent engine is not running.");
            ActualListenStatusText().Text(
                inputsValid
                    ? L"The configured endpoints are saved and will be used at startup."
                    : L"The edited endpoint is invalid and has not been saved.");
            return;
        }

        auto const status = core->GetListenStatus();
        if (!status.isListening || status.port <= 0)
        {
            ActualListenPortText().Text(L"Not listening");
            ActualListenEndpointText().Text(
                L"libtorrent has not opened a listening socket.");
            if (!inputsValid)
            {
                ActualListenStatusText().Text(
                    L"The edited endpoint is invalid and has not been applied.");
            }
            else if (!status.error.empty())
            {
                ActualListenStatusText().Text(to_hstring(status.error));
            }
            else
            {
                ActualListenStatusText().Text(
                    L"Waiting for the listener to become available.");
            }
            return;
        }

        auto const actualPort =
            static_cast<std::uint32_t>(status.port);
        ViewModel().ListenPort(
            static_cast<std::uint16_t>(actualPort));
        ActualListenPortText().Text(to_hstring(
            std::format("Port {}", actualPort)));
        ActualListenEndpointText().Text(to_hstring(
            std::format("Currently listening on port {}", actualPort)));

        if (!inputsValid)
        {
            ActualListenStatusText().Text(
                L"The edited endpoint is invalid; the displayed listener still uses the last valid configuration.");
            return;
        }

        auto const ipv4Port = IPv4ListenEndpoint().Port();
        auto const ipv6Port = IPv6ListenEndpoint().Port();
        if (ipv4Port == 0 || ipv6Port == 0)
        {
            ActualListenStatusText().Text(
                L"The operating system selected this runtime port because at least one configured endpoint uses port 0.");
        }
        else if (actualPort == ipv4Port || actualPort == ipv6Port)
        {
            ActualListenStatusText().Text(
                L"The runtime listener matches the saved endpoint configuration.");
        }
        else
        {
            ActualListenStatusText().Text(
                L"The runtime port differs from the saved fixed port. Check whether the configured port is already in use.");
        }
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

    void NetworkSettingsPage::ListenEndpoint_ValueChanged(
        IInspectable const&,
        IInspectable const&)
    {
        if (m_loadingListenPort)
        {
            return;
        }
        m_listenSettingsDirty = true;
        m_listenApplyTimer.Stop();
        m_listenApplyTimer.Start();
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
