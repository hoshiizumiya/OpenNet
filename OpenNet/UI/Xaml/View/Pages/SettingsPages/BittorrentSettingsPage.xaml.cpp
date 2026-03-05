#include "pch.h"
#include "BittorrentSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.g.cpp"
#endif

#include "Core/P2PManager.h"
#include "Core/TorrentSettings.h"
#include <libtorrent/settings_pack.hpp>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <wil/cppwinrt_helpers.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;

namespace lt = libtorrent;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    BittorrentSettingsPage::BittorrentSettingsPage()
    {
        InitializeComponent();

        Loaded([this](IInspectable const &, RoutedEventArgs const &)
               { LoadSettings(); });

        // Save settings when the page is unloaded (navigated away).
        // This ensures TextBox changes that only use LostFocus are persisted
        // even if the user navigates away without clicking elsewhere first.
        Unloaded([this](IInspectable const &, RoutedEventArgs const &)
        {
            if (!m_loading)
            {
                SaveAndApply();
            }
        });
    }

    winrt::fire_and_forget BittorrentSettingsPage::LoadSettings()
    {
        auto strong = get_strong();
        auto dispatcher = DispatcherQueue();

        // Heavy I/O + libtorrent calls on background thread
        co_await winrt::resume_background();

        auto &mgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
        mgr.Load();
        auto s = mgr.Get();

        // If the session is running, prefer live values
        auto *core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
        if (core && core->IsRunning())
        {
            try
            {
                auto pack = core->GetSettings();
                auto live = ::OpenNet::Core::BuildTorrentSettingsFromPack(pack);
                live.defaultSavePath = s.defaultSavePath;
                live.preallocateStorage = s.preallocateStorage;
                live.autoStartDownloads = s.autoStartDownloads;
                live.moveCompletedEnabled = s.moveCompletedEnabled;
                live.moveCompletedPath = s.moveCompletedPath;
                s = live;
            }
            catch (...) { /* use persisted settings if live read fails */ }
        }

        // Back to UI thread to populate controls
        co_await wil::resume_foreground(dispatcher);

        m_loading = true;
        PopulateFromSettings(s);
        m_loading = false;
    }

    void BittorrentSettingsPage::PopulateFromSettings(::OpenNet::Core::TorrentSettings const &s)
    {
        // Connection
        ListenInterfacesTextBox().Text(winrt::to_hstring(s.listenInterfaces));
        ConnectionsLimitNumberBox().Value(s.connectionsLimit);
        EnableIncomingTcpToggle().IsOn(s.enableIncomingTcp);
        EnableOutgoingTcpToggle().IsOn(s.enableOutgoingTcp);
        EnableIncomingUtpToggle().IsOn(s.enableIncomingUtp);
        EnableOutgoingUtpToggle().IsOn(s.enableOutgoingUtp);
        AllowMultipleConnectionsPerIpToggle().IsOn(s.allowMultipleConnectionsPerIp);
        AnonymousModeToggle().IsOn(s.anonymousMode);

        // Discovery
        EnableDhtToggle().IsOn(s.enableDht);
        EnableLsdToggle().IsOn(s.enableLsd);
        EnableUpnpToggle().IsOn(s.enableUpnp);
        EnableNatpmpToggle().IsOn(s.enableNatpmp);

        // Tracker
        AnnounceToAllTiersToggle().IsOn(s.announceToAllTiers);
        AnnounceToAllTrackersToggle().IsOn(s.announceToAllTrackers);

        // Limits
        ActiveDownloadsNumberBox().Value(s.activeDownloads);
        ActiveSeedsNumberBox().Value(s.activeSeeds);
        ActiveLimitNumberBox().Value(s.activeLimit);

        // Speed limits (stored as bytes/sec; display as KB/s)
        DownloadRateLimitNumberBox().Value(s.downloadRateLimit / 1024);
        UploadRateLimitNumberBox().Value(s.uploadRateLimit / 1024);

        // Seeding
        SeedRatioLimitNumberBox().Value(s.seedingRatioLimit);
        SeedTimeLimitNumberBox().Value(s.seedingTimeLimit);

        // Peer
        PeerTimeoutNumberBox().Value(s.peerTimeout);
        HandshakeTimeoutNumberBox().Value(s.handshakeTimeout);
        CloseRedundantConnectionsToggle().IsOn(s.closeRedundantConnections);

        // Disk I/O
        AioThreadsNumberBox().Value(s.aioThreads);
        CheckingMemUsageNumberBox().Value(s.checkingMemUsage);

        // Identity
        UserAgentTextBox().Text(winrt::to_hstring(s.userAgent));

        // Encryption
        EncryptionPolicyComboBox().SelectedIndex(static_cast<int>(s.encryptionPolicy));
        PreferRc4Toggle().IsOn(s.preferRc4);

        // Proxy
        ProxyTypeComboBox().SelectedIndex(static_cast<int>(s.proxyType));
        ProxyHostnameTextBox().Text(winrt::to_hstring(s.proxyHostname));
        ProxyPortNumberBox().Value(s.proxyPort);
        ProxyUsernameTextBox().Text(winrt::to_hstring(s.proxyUsername));
        ProxyPasswordBox().Password(winrt::to_hstring(s.proxyPassword));
        ProxyPeerConnectionsToggle().IsOn(s.proxyPeerConnections);
        ProxyTrackerConnectionsToggle().IsOn(s.proxyTrackerConnections);

        // Advanced libtorrent
        MaxPeerListSizeNumberBox().Value(s.maxPeerListSize);
        PeerFingerprintTextBox().Text(winrt::to_hstring(s.peerFingerprint));

        // Download Defaults
        DefaultSavePathTextBox().Text(winrt::to_hstring(s.defaultSavePath));
        PreallocateStorageToggle().IsOn(s.preallocateStorage);
        AutoStartDownloadsToggle().IsOn(s.autoStartDownloads);
        MoveCompletedToggle().IsOn(s.moveCompletedEnabled);
        MoveCompletedPathTextBox().Text(winrt::to_hstring(s.moveCompletedPath));
    }

    ::OpenNet::Core::TorrentSettings BittorrentSettingsPage::CollectFromUI()
    {
        // Start from current persisted settings to preserve fields not on this page
        auto s = ::OpenNet::Core::TorrentSettingsManager::Instance().Get();

        // Connection
        s.listenInterfaces = winrt::to_string(ListenInterfacesTextBox().Text());
        s.connectionsLimit = static_cast<int>(ConnectionsLimitNumberBox().Value());
        s.enableIncomingTcp = EnableIncomingTcpToggle().IsOn();
        s.enableOutgoingTcp = EnableOutgoingTcpToggle().IsOn();
        s.enableIncomingUtp = EnableIncomingUtpToggle().IsOn();
        s.enableOutgoingUtp = EnableOutgoingUtpToggle().IsOn();
        s.allowMultipleConnectionsPerIp = AllowMultipleConnectionsPerIpToggle().IsOn();
        s.anonymousMode = AnonymousModeToggle().IsOn();

        // Discovery
        s.enableDht = EnableDhtToggle().IsOn();
        s.enableLsd = EnableLsdToggle().IsOn();
        s.enableUpnp = EnableUpnpToggle().IsOn();
        s.enableNatpmp = EnableNatpmpToggle().IsOn();

        // Tracker
        s.announceToAllTiers = AnnounceToAllTiersToggle().IsOn();
        s.announceToAllTrackers = AnnounceToAllTrackersToggle().IsOn();

        // Limits
        s.activeDownloads = static_cast<int>(ActiveDownloadsNumberBox().Value());
        s.activeSeeds = static_cast<int>(ActiveSeedsNumberBox().Value());
        s.activeLimit = static_cast<int>(ActiveLimitNumberBox().Value());

        // Speed limits (UI in KB/s, store as bytes/sec)
        s.downloadRateLimit = static_cast<int>(DownloadRateLimitNumberBox().Value()) * 1024;
        s.uploadRateLimit = static_cast<int>(UploadRateLimitNumberBox().Value()) * 1024;

        // Seeding
        s.seedingRatioLimit = SeedRatioLimitNumberBox().Value();
        s.seedingTimeLimit = static_cast<int>(SeedTimeLimitNumberBox().Value());

        // Peer
        s.peerTimeout = static_cast<int>(PeerTimeoutNumberBox().Value());
        s.handshakeTimeout = static_cast<int>(HandshakeTimeoutNumberBox().Value());
        s.closeRedundantConnections = CloseRedundantConnectionsToggle().IsOn();

        // Disk I/O
        s.aioThreads = static_cast<int>(AioThreadsNumberBox().Value());
        s.checkingMemUsage = static_cast<int>(CheckingMemUsageNumberBox().Value());

        // Identity
        s.userAgent = winrt::to_string(UserAgentTextBox().Text());

        // Encryption
        s.encryptionPolicy = static_cast<::OpenNet::Core::EncryptionPolicy>(
            EncryptionPolicyComboBox().SelectedIndex());
        s.preferRc4 = PreferRc4Toggle().IsOn();

        // Proxy
        s.proxyType = static_cast<::OpenNet::Core::ProxyType>(
            ProxyTypeComboBox().SelectedIndex());
        s.proxyHostname = winrt::to_string(ProxyHostnameTextBox().Text());
        s.proxyPort = static_cast<int>(ProxyPortNumberBox().Value());
        s.proxyUsername = winrt::to_string(ProxyUsernameTextBox().Text());
        s.proxyPassword = winrt::to_string(ProxyPasswordBox().Password());
        s.proxyPeerConnections = ProxyPeerConnectionsToggle().IsOn();
        s.proxyTrackerConnections = ProxyTrackerConnectionsToggle().IsOn();

        // Advanced libtorrent
        s.maxPeerListSize = static_cast<int>(MaxPeerListSizeNumberBox().Value());
        s.peerFingerprint = winrt::to_string(PeerFingerprintTextBox().Text());

        // Download Defaults
        s.defaultSavePath = winrt::to_string(DefaultSavePathTextBox().Text());
        s.preallocateStorage = PreallocateStorageToggle().IsOn();
        s.autoStartDownloads = AutoStartDownloadsToggle().IsOn();
        s.moveCompletedEnabled = MoveCompletedToggle().IsOn();
        s.moveCompletedPath = winrt::to_string(MoveCompletedPathTextBox().Text());

        return s;
    }

    void BittorrentSettingsPage::SaveAndApply()
    {
        auto s = CollectFromUI();

        // Persist to JSON
        ::OpenNet::Core::TorrentSettingsManager::Instance().Set(s);

        // Apply to live session
        auto *core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
        if (core && core->IsRunning())
        {
            lt::settings_pack pack;
            ::OpenNet::Core::ApplyTorrentSettingsToSettingsPack(s, pack);
            core->ApplySettings(pack);
        }
    }

    void BittorrentSettingsPage::OnSettingChanged(IInspectable const &, IInspectable const &)
    {
        if (m_loading)
            return;
        SaveAndApply();
    }

}
