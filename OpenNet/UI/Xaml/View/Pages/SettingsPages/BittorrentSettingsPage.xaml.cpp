#include "pch.h"
#include "BittorrentSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.g.cpp"
#endif

#include "AdvancedOptionItem.h"
#include "Core/P2PManager.h"
#include <algorithm>
#include "UI/Xaml/Behaviors/Headers/StickyHeaderBehavior.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    BittorrentSettingsPage::BittorrentSettingsPage()
    {
        InitializeComponent();
        m_options = single_threaded_observable_vector<IInspectable>();
        LoadOptions();

        Loaded([this](IInspectable const&, RoutedEventArgs const&)
        {
            auto headerBorder = HeaderBorder();
            if (headerBorder)
            {
                m_stickyBehavior = winrt::make<winrt::OpenNet::UI::Xaml::Behaviors::implementation::StickyHeaderBehavior>();
                m_stickyBehavior.Attach(headerBorder);
            }
        });
    }

    IObservableVector<IInspectable> BittorrentSettingsPage::AdvancedOptions() const
    {
        return m_options;
    }

    void BittorrentSettingsPage::SearchFilter(winrt::hstring const& value)
    {
        m_searchFilter = value;
        FilterOptions();
    }

    void BittorrentSettingsPage::ApplySettings()
    {
        // Apply settings to libtorrent session
        // This would be implemented when we have direct session access
    }

    void BittorrentSettingsPage::ResetToDefaults()
    {
        for (auto& option : m_allOptions)
        {
            option.Value(option.OriginalValue());
        }
        FilterOptions();
    }

    void BittorrentSettingsPage::LoadOptions()
    {
        m_allOptions.clear();

        // Add common libtorrent settings
        // These are typical settings from libtorrent's settings_pack

        // Anti-leech settings
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.anti_leech_min_byte", L"10000", true));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.anti_leech_private_torrent", L"false", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.anti_leech_stable_sec", L"90", true));

        // Cache settings
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.cache.ltseed_cache_min_size_mb", L"64", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.cache.piece_cache_min_size_mb", L"64", false));

        // Connection settings
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.connection.ltseed_protocol_selection", L"TCP only", true));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.enable_lsd", L"true", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.enable_sequential_download_mode_for_new_task", L"false", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.enable_torrent_query", L"true", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.enable_v1_upgrade_to_v2", L"true", true));

        // Hash check settings
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.hash_check_for_ltseed", L"true", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.hash_check_if_file_changed", L"true", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.hash_check_on_finished", L"false", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.hash_check_thread_low_priority", L"true", false));

        // Connection limits
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.max_connections_per_ltseed", L"10", true));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.max_connections_per_task", L"9999", true));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.max_torrent_size_mb", L"20", false));

        // Peer settings
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.multi_peers_same_ip", L"false", true));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.peer_dual_ip", L"true", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.peer_exchange", L"true", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.peer_hole_punch", L"true", false));

        // DHT settings
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.dht.enabled", L"true", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.dht.max_peers_reply", L"100", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.dht.search_branching", L"5", false));

        // Tracker settings
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.tracker.announce_to_all_tiers", L"false", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.tracker.announce_to_all_trackers", L"false", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.tracker.max_failcount", L"5", false));

        // Disk I/O settings
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.disk.max_queued_disk_bytes", L"1048576", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.disk.cache_size", L"1024", false));
        m_allOptions.push_back(winrt::make<AdvancedOptionItem>(L"bittorrent.disk.low_prio_disk", L"true", false));

        FilterOptions();
    }

    void BittorrentSettingsPage::FilterOptions()
    {
        m_options.Clear();

        std::wstring filter = m_searchFilter.c_str();
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

        for (const auto& option : m_allOptions)
        {
            if (filter.empty())
            {
                m_options.Append(option);
            }
            else
            {
                std::wstring name = option.Name().c_str();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                if (name.find(filter) != std::wstring::npos)
                {
                    m_options.Append(option);
                }
            }
        }
    }

    void BittorrentSettingsPage::SearchBox_TextChanged(IInspectable const& sender, Controls::AutoSuggestBoxTextChangedEventArgs const&)
    {
        auto box = sender.as<Controls::AutoSuggestBox>();
        SearchFilter(box.Text());
    }

    void BittorrentSettingsPage::ApplyButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        ApplySettings();
    }

    void BittorrentSettingsPage::ResetButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        ResetToDefaults();
    }
}
