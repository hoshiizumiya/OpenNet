#include "pch.h"
#include "TaskPeersListPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskPeersListPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskPeersListPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <format>
#include <unordered_map>
#include <unordered_set>

#include "Core/P2PManager.h"
#include "Core/IPFilter/IPFilterManager.h"
#include "Core/GeoIP/GeoIPManager.h"
#include "ViewModels/DisplayItems.h"
#include "Helpers/ColumnWidthHelper.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    TaskPeersListPage::TaskPeersListPage()
    {
        InitializeComponent();
        this->NavigationCacheMode(winrt::Microsoft::UI::Xaml::Navigation::NavigationCacheMode::Required);

        Loaded([this](auto, auto) {
            using namespace ::OpenNet::Helpers;
            RestoreColumn(ColPeerLocation(), "Peers.Location");
            RestoreColumn(ColPeerProgress(), "Peers.Progress");
            RestoreColumn(ColPeerDLSpeed(), "Peers.DLSpeed");
            RestoreColumn(ColPeerULSpeed(), "Peers.ULSpeed");
            RestoreColumn(ColPeerDownloaded(), "Peers.Downloaded");
            RestoreColumn(ColPeerClient(), "Peers.Client");
            RestoreColumn(ColPeerStatus(), "Peers.Status");
            RestoreColumn(ColPeerProtocol(), "Peers.Protocol");
            RestoreColumn(ColPeerInitiator(), "Peers.Initiator");
            RestoreColumn(ColPeerSource(), "Peers.Source");
        });
        Unloaded([this](auto, auto) {
            using namespace ::OpenNet::Helpers;
            SaveColumnWidth("Peers.Location", ColPeerLocation().ActualWidth());
            SaveColumnWidth("Peers.Progress", ColPeerProgress().ActualWidth());
            SaveColumnWidth("Peers.DLSpeed", ColPeerDLSpeed().ActualWidth());
            SaveColumnWidth("Peers.ULSpeed", ColPeerULSpeed().ActualWidth());
            SaveColumnWidth("Peers.Downloaded", ColPeerDownloaded().ActualWidth());
            SaveColumnWidth("Peers.Client", ColPeerClient().ActualWidth());
            SaveColumnWidth("Peers.Status", ColPeerStatus().ActualWidth());
            SaveColumnWidth("Peers.Protocol", ColPeerProtocol().ActualWidth());
            SaveColumnWidth("Peers.Initiator", ColPeerInitiator().ActualWidth());
            SaveColumnWidth("Peers.Source", ColPeerSource().ActualWidth());
        });
    }

    TaskPeersListPage::~TaskPeersListPage()
    {
        Unsubscribe();
        if (m_refreshTimer)
        {
            m_refreshTimer.Stop();
            m_refreshTimer.Tick(m_timerTickToken);
            m_refreshTimer = nullptr;
        }
    }

    void TaskPeersListPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
    {
        Unsubscribe();

        m_viewModel = e.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
        if (!m_viewModel)
        {
            m_viewModel = this->DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
        }

        if (m_viewModel)
        {
            this->DataContext(m_viewModel);
            m_vmPropertyChangedToken = m_viewModel.PropertyChanged(
                { this, &TaskPeersListPage::OnViewModelPropertyChanged });
        }

        // Set up periodic refresh timer (every 2 seconds)
        if (!m_refreshTimer)
        {
            m_refreshTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
            m_refreshTimer.Interval(std::chrono::seconds(2));
            m_timerTickToken = m_refreshTimer.Tick(
                { this, &TaskPeersListPage::OnRefreshTimerTick });
        }
        m_refreshTimer.Start();

        RefreshPeerList();
    }

    void TaskPeersListPage::OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
    {
        if (m_refreshTimer)
        {
            m_refreshTimer.Stop();
        }
        Unsubscribe();
    }

    void TaskPeersListPage::Unsubscribe()
    {
        if (m_viewModel && m_vmPropertyChangedToken.value)
        {
            m_viewModel.PropertyChanged(m_vmPropertyChangedToken);
            m_vmPropertyChangedToken = {};
        }
        m_viewModel = nullptr;
    }

    void TaskPeersListPage::OnViewModelPropertyChanged(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
    {
        if (args.PropertyName() == L"SelectedTask")
        {
            // Task changed — force full rebuild
            m_lastTaskId.clear();
            m_peerItems = nullptr;
            RefreshPeerList();
        }
    }

    void TaskPeersListPage::OnRefreshTimerTick(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Windows::Foundation::IInspectable const&)
    {
        RefreshPeerList();
    }

    // Helper to format speed
    static winrt::hstring FormatSpeed(int kbps)
    {
        if (kbps >= 1024)
        {
            wchar_t buf[64];
            swprintf(buf, 64, L"%.1f MB/s", kbps / 1024.0);
            return winrt::hstring{ buf };
        }
        wchar_t buf[64];
        swprintf(buf, 64, L"%d KB/s", kbps);
        return winrt::hstring{ buf };
    }

    static winrt::hstring FormatBytes(int64_t bytes)
    {
        if (bytes <= 0) return L"-";
        wchar_t buf[64];
        double gb = bytes / (1024.0 * 1024.0 * 1024.0);
        if (gb >= 1.0)
        {
            swprintf(buf, 64, L"%.2f GB", gb);
        }
        else
        {
            double mb = bytes / (1024.0 * 1024.0);
            if (mb >= 1.0)
                swprintf(buf, 64, L"%.2f MB", mb);
            else
                swprintf(buf, 64, L"%.1f KB", bytes / 1024.0);
        }
        return winrt::hstring{ buf };
    }

    // Format peer status flags to qBittorrent-style string: D=downloading, U=uploading, etc.
    static winrt::hstring FormatPeerStatus(uint32_t flags)
    {
        std::wstring result;
        // lt::peer_info flag bits (libtorrent 2.0)
        constexpr uint32_t interesting = 0x1;
        constexpr uint32_t choked = 0x2;
        constexpr uint32_t remote_interested = 0x4;
        constexpr uint32_t remote_choked = 0x8;
        constexpr uint32_t seed = 0x200;
        constexpr uint32_t optimistic_unchoke = 0x800;
        constexpr uint32_t snubbed = 0x1000;
        constexpr uint32_t rc4_encrypted = 0x100000;
        constexpr uint32_t plaintext_encrypted = 0x200000;

        if (flags & interesting) result += L"D";        // we want data from them
        if (flags & remote_interested) result += L"U";  // they want data from us
        if (flags & choked) result += L"d";              // we are choked by them
        if (flags & remote_choked) result += L"u";       // we are choking them
        if (flags & seed) result += L"S";                // seed
        if (flags & optimistic_unchoke) result += L"O";  // optimistic unchoke
        if (flags & snubbed) result += L"H";             // snubbed
        if (flags & rc4_encrypted) result += L"E";       // encrypted (RC4)
        else if (flags & plaintext_encrypted) result += L"e"; // encrypted (plaintext)

        return winrt::hstring{ result };
    }

    static winrt::hstring FormatConnectionType(int connType)
    {
        // 0_bit = 1, 1_bit = 2, 2_bit = 4 in libtorrent bitfield flags
        switch (connType)
        {
        case 1: return L"BT";         // standard_bittorrent = 0_bit -> value 1
        case 2: return L"WebSeed";    // web_seed = 1_bit -> value 2
        case 4: return L"HTTP";       // http_seed = 2_bit -> value 4
        default: return L"BT";
        }
    }

    static winrt::hstring FormatPeerSource(int source)
    {
        std::wstring parts;
        if (source & 1) parts += L"Tracker ";   // tracker = 0_bit -> 1
        if (source & 2) parts += L"DHT ";       // dht = 1_bit -> 2
        if (source & 4) parts += L"PEX ";       // pex = 2_bit -> 4
        if (source & 8) parts += L"LSD ";       // lsd = 3_bit -> 8
        if (source & 16) parts += L"Resume ";   // resume_data = 4_bit -> 16
        if (source & 32) parts += L"Incoming ";  // incoming = 5_bit -> 32
        // Trim trailing space
        if (!parts.empty() && parts.back() == L' ')
            parts.pop_back();
        return parts.empty() ? L"-" : winrt::hstring{ parts };
    }

    void TaskPeersListPage::RefreshPeerList()
    {
        auto listView = PeersListView();
        auto emptyText = EmptyStateText();
        if (!listView) return;

        if (!m_viewModel || !m_viewModel.SelectedTask())
        {
            listView.ItemsSource(nullptr);
            m_peerItems = nullptr;
            m_lastTaskId.clear();
            if (emptyText) emptyText.Visibility(Visibility::Visible);
            return;
        }

        auto selectedTask = m_viewModel.SelectedTask();
        auto taskType = selectedTask.TaskType();

        // Only show peers for BitTorrent tasks
        if (taskType != winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
        {
            listView.ItemsSource(nullptr);
            m_peerItems = nullptr;
            m_lastTaskId.clear();
            if (emptyText) emptyText.Visibility(Visibility::Visible);
            return;
        }

        auto taskId = winrt::to_string(selectedTask.TaskId());
        if (taskId.empty())
        {
            listView.ItemsSource(nullptr);
            m_peerItems = nullptr;
            m_lastTaskId.clear();
            if (emptyText) emptyText.Visibility(Visibility::Visible);
            return;
        }

        // If task changed, clear cached items
        if (taskId != m_lastTaskId)
        {
            m_peerItems = nullptr;
            m_lastTaskId = taskId;
        }

        auto& p2p = ::OpenNet::Core::P2PManager::Instance();
        if (!p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore())
        {
            listView.ItemsSource(nullptr);
            m_peerItems = nullptr;
            if (emptyText) emptyText.Visibility(Visibility::Visible);
            return;
        }

        auto detail = p2p.TorrentCore()->GetTorrentDetail(taskId);

        if (detail.peers.empty())
        {
            listView.ItemsSource(nullptr);
            m_peerItems = nullptr;
            if (emptyText) emptyText.Visibility(Visibility::Visible);
            return;
        }

        // Build a map of new peers keyed by IP:port
        std::unordered_map<std::string, size_t> newPeerMap;
        for (size_t i = 0; i < detail.peers.size(); ++i)
        {
            auto key = detail.peers[i].ip + ":" + std::to_string(detail.peers[i].port);
            newPeerMap[key] = i;
        }

        if (!m_peerItems)
        {
            // First time or task changed: create fresh collection
            m_peerItems = winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();

            for (auto const& peer : detail.peers)
            {
                auto item = winrt::make<winrt::OpenNet::ViewModels::implementation::PeerDisplayItem>();
                auto ipPort = winrt::to_hstring(peer.ip) + L":" + winrt::to_hstring(peer.port);
                item.IP(ipPort);
                item.Client(winrt::to_hstring(peer.client));
                wchar_t progBuf[32];
                swprintf(progBuf, 32, L"%.1f%%", peer.progress * 100.0);
                item.Progress(winrt::hstring{ progBuf });
                item.DLSpeed(FormatSpeed(peer.downloadRateKB));
                item.ULSpeed(FormatSpeed(peer.uploadRateKB));
                item.Downloaded(FormatBytes(peer.totalDownloaded));
                item.PeerStatus(FormatPeerStatus(peer.flags));
                {
                    auto& geo = ::OpenNet::Core::GeoIPManager::Instance();
                    auto country = geo.LookupCountryName(peer.ip);
                    item.Location(country.empty() ? L"-" : winrt::to_hstring(country));
                }
                item.ConnectionTime(L"-");
                item.Protocol(FormatConnectionType(peer.connectionType));
                item.Initiator(peer.isIncoming ? L"Remote" : L"Local");
                item.Source(FormatPeerSource(peer.source));

                m_peerItems.Append(item);
            }

            listView.ItemsSource(m_peerItems);
        }
        else
        {
            // Incremental update: build index of existing items
            std::unordered_map<std::string, uint32_t> existingMap;
            for (uint32_t i = 0; i < m_peerItems.Size(); ++i)
            {
                auto item = m_peerItems.GetAt(i).as<winrt::OpenNet::ViewModels::PeerDisplayItem>();
                existingMap[winrt::to_string(item.IP())] = i;
            }

            // Remove peers that no longer exist (iterate backwards to preserve indices)
            std::vector<uint32_t> toRemove;
            for (auto const& [key, idx] : existingMap)
            {
                if (newPeerMap.find(key) == newPeerMap.end())
                {
                    toRemove.push_back(idx);
                }
            }
            std::sort(toRemove.rbegin(), toRemove.rend());
            for (auto idx : toRemove)
            {
                m_peerItems.RemoveAt(idx);
            }

            // Update existing and add new peers
            // Rebuild existingMap after removals
            existingMap.clear();
            for (uint32_t i = 0; i < m_peerItems.Size(); ++i)
            {
                auto item = m_peerItems.GetAt(i).as<winrt::OpenNet::ViewModels::PeerDisplayItem>();
                existingMap[winrt::to_string(item.IP())] = i;
            }

            for (auto const& peer : detail.peers)
            {
                auto key = peer.ip + ":" + std::to_string(peer.port);
                auto it = existingMap.find(key);
                if (it != existingMap.end())
                {
                    // Update existing item in-place (no structural change → no ListView flicker)
                    auto item = m_peerItems.GetAt(it->second).as<winrt::OpenNet::ViewModels::PeerDisplayItem>();
                    wchar_t progBuf[32];
                    swprintf(progBuf, 32, L"%.1f%%", peer.progress * 100.0);
                    item.Progress(winrt::hstring{ progBuf });
                    item.DLSpeed(FormatSpeed(peer.downloadRateKB));
                    item.ULSpeed(FormatSpeed(peer.uploadRateKB));
                    item.Downloaded(FormatBytes(peer.totalDownloaded));
                    item.Client(winrt::to_hstring(peer.client));
                    item.PeerStatus(FormatPeerStatus(peer.flags));
                    item.Protocol(FormatConnectionType(peer.connectionType));
                    item.Initiator(peer.isIncoming ? L"Remote" : L"Local");
                    item.Source(FormatPeerSource(peer.source));
                }
                else
                {
                    // Add new peer
                    auto item = winrt::make<winrt::OpenNet::ViewModels::implementation::PeerDisplayItem>();
                    auto ipPort = winrt::to_hstring(peer.ip) + L":" + winrt::to_hstring(peer.port);
                    item.IP(ipPort);
                    item.Client(winrt::to_hstring(peer.client));
                    wchar_t progBuf[32];
                    swprintf(progBuf, 32, L"%.1f%%", peer.progress * 100.0);
                    item.Progress(winrt::hstring{ progBuf });
                    item.DLSpeed(FormatSpeed(peer.downloadRateKB));
                    item.ULSpeed(FormatSpeed(peer.uploadRateKB));
                    item.Downloaded(FormatBytes(peer.totalDownloaded));
                    item.PeerStatus(FormatPeerStatus(peer.flags));
                    {
                        auto& geo = ::OpenNet::Core::GeoIPManager::Instance();
                        auto country = geo.LookupCountryName(peer.ip);
                        item.Location(country.empty() ? L"-" : winrt::to_hstring(country));
                    }
                    item.ConnectionTime(L"-");
                    item.Protocol(FormatConnectionType(peer.connectionType));
                    item.Initiator(peer.isIncoming ? L"Remote" : L"Local");
                    item.Source(FormatPeerSource(peer.source));

                    m_peerItems.Append(item);
                }
            }
        }

        if (emptyText) emptyText.Visibility(Visibility::Collapsed);
    }

    // ---- Ban peer handlers ----

    void TaskPeersListPage::BanSelectedPeer(winrt::hstring const& description)
    {
        auto listView = PeersListView();
        if (!listView) return;
        auto selected = listView.SelectedItem();
        if (!selected) return;
        auto peer = selected.try_as<winrt::OpenNet::ViewModels::PeerDisplayItem>();
        if (!peer) return;

        auto ipPortStr = winrt::to_string(peer.IP());
        // Extract IP (strip port)
        auto colonPos = ipPortStr.rfind(':');
        std::string ip = (colonPos != std::string::npos) ? ipPortStr.substr(0, colonPos) : ipPortStr;

        try
        {
            auto& ipFilter = ::OpenNet::Core::IPFilterManager::Instance();
            ipFilter.AddRule(ip, ip, 1, winrt::to_string(description));
            ipFilter.ApplyToSession();

            OutputDebugStringW((L"Banned peer: " + winrt::to_hstring(ip) + L" - " + description + L"\n").c_str());
        }
        catch (...)
        {
            OutputDebugStringA("BanSelectedPeer: Error adding IP filter rule\n");
        }
    }

    void TaskPeersListPage::BanPeer1h_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        BanSelectedPeer(L"Banned for 1 hour");
    }

    void TaskPeersListPage::BanPeer24h_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        BanSelectedPeer(L"Banned for 24 hours");
    }

    void TaskPeersListPage::BanPeerPermanent_Click(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        BanSelectedPeer(L"Banned permanently");
    }
}
