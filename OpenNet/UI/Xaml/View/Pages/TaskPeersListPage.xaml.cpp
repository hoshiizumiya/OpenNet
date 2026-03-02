#include "pch.h"
#include "TaskPeersListPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskPeersListPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskPeersListPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <format>

#include "Core/P2PManager.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    TaskPeersListPage::TaskPeersListPage()
    {
        InitializeComponent();
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

    void TaskPeersListPage::RefreshPeerList()
    {
        auto listView = PeersListView();
        auto emptyText = EmptyStateText();
        if (!listView) return;

        if (!m_viewModel || !m_viewModel.SelectedTask())
        {
            listView.ItemsSource(nullptr);
            if (emptyText)
            {
                emptyText.Visibility(Visibility::Visible);
            }
            return;
        }

        auto selectedTask = m_viewModel.SelectedTask();
        auto taskType = selectedTask.TaskType();

        // Only show peers for BitTorrent tasks
        if (taskType != winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
        {
            listView.ItemsSource(nullptr);
            if (emptyText)
            {
                emptyText.Visibility(Visibility::Visible);
            }
            return;
        }

        auto taskId = winrt::to_string(selectedTask.TaskId());
        if (taskId.empty())
        {
            listView.ItemsSource(nullptr);
            if (emptyText)
            {
                emptyText.Visibility(Visibility::Visible);
            }
            return;
        }

        // Get peer info from libtorrent on a background thread equivalent
        // Since this runs on the UI thread via timer, and GetTorrentDetail is thread-safe,
        // we can call it directly (it's a quick snapshot). For very large peer lists
        // this could be moved to background, but for typical use it's fine.
        auto& p2p = ::OpenNet::Core::P2PManager::Instance();
        if (!p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore())
        {
            listView.ItemsSource(nullptr);
            if (emptyText)
            {
                emptyText.Visibility(Visibility::Visible);
            }
            return;
        }

        auto detail = p2p.TorrentCore()->GetTorrentDetail(taskId);

        if (detail.peers.empty())
        {
            listView.ItemsSource(nullptr);
            if (emptyText)
            {
                emptyText.Visibility(Visibility::Visible);
            }
            return;
        }

        // Build an observable collection of PropertySet items for Binding
        auto items = winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();

        for (auto const& peer : detail.peers)
        {
            auto peerMap = winrt::Windows::Foundation::Collections::PropertySet();
            // IP:Port
            auto ipPort = winrt::to_hstring(peer.ip) + L":" + winrt::to_hstring(peer.port);
            peerMap.Insert(L"IP", winrt::box_value(ipPort));
            peerMap.Insert(L"Client", winrt::box_value(winrt::to_hstring(peer.client)));

            wchar_t progBuf[32];
            swprintf(progBuf, 32, L"%.1f%%", peer.progress * 100.0);
            peerMap.Insert(L"Progress", winrt::box_value(winrt::hstring{ progBuf }));

            peerMap.Insert(L"DLSpeed", winrt::box_value(FormatSpeed(peer.downloadRateKB)));
            peerMap.Insert(L"ULSpeed", winrt::box_value(FormatSpeed(peer.uploadRateKB)));
            peerMap.Insert(L"Downloaded", winrt::box_value(FormatBytes(peer.totalDownloaded)));

            items.Append(peerMap);
        }

        listView.ItemsSource(items);
        if (emptyText)
        {
            emptyText.Visibility(Visibility::Collapsed);
        }
    }
}
