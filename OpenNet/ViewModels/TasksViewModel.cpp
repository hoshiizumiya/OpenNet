#include "pch.h"
#include "ViewModels/TasksViewModel.h"
#include "ViewModels/TasksViewModel.g.cpp"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>
#include "Core/torrentCore/libtorrentHandle.h"

using namespace std::string_literals;

namespace winrt::OpenNet::ViewModels::implementation
{
    TasksViewModel::TasksViewModel() : ::mvvm::view_model<TasksViewModel>(winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread())
    {
        m_tasks = winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TaskViewModel>();

        // Simple commands (wire actual behavior later) - specify template arg and use winrt::make
        m_startCommand = winrt::make<mvvm::delegate_command<winrt::Windows::Foundation::IInspectable>>(
            [this](winrt::Windows::Foundation::IInspectable const&)
            {
                std::scoped_lock lk(m_coreMutex);
                if (!m_core) return;
                // could iterate torrents and resume
            }
        );
        m_pauseCommand = winrt::make<mvvm::delegate_command<winrt::Windows::Foundation::IInspectable>>(
            [this](winrt::Windows::Foundation::IInspectable const&)
            {
                std::scoped_lock lk(m_coreMutex);
                if (!m_core) return;
                // could iterate torrents and pause
            }
        );
        m_newCommand = winrt::make<mvvm::delegate_command<winrt::Windows::Foundation::IInspectable>>(
            [this](winrt::Windows::Foundation::IInspectable const&)
            {
                // open add magnet dialog in future
				//::OpenNet::Core::Torrent::LibtorrentHandle::LibtorrentInitCore->AddMagnet("magnet:?xt=urn:btih:9a316b69b22250a87b794ab9002137576dea4302&tr=https%3A%2F%2Ftr.bangumi.moe%3A9696%2Fannounce&tr=http%3A%2F%2Ftr.bangumi.moe%3A6969%2Fannounce&tr=udp%3A%2F%2Ftr.bangumi.moe%3A6969%2Fannounce&tr=http%3A%2F%2Fopen.acgtracker.com%3A1096%2Fannounce&tr=http%3A%2F%2F208.67.16.113%3A8000%2Fannounce&tr=udp%3A%2F%2F208.67.16.113%3A8000%2Fannounce&tr=http%3A%2F%2Ftracker.ktxp.com%3A6868%2Fannounce&tr=http%3A%2F%2Ftracker.ktxp.com%3A7070%2Fannounce&tr=http%3A%2F%2Ft2.popgo.org%3A7456%2Fannonce&tr=http%3A%2F%2Fbt.sc-ol.com%3A2710%2Fannounce&tr=http%3A%2F%2Fshare.camoe.cn%3A8080%2Fannounce&tr=http%3A%2F%2F61.154.116.205%3A8000%2Fannounce&tr=http%3A%2F%2Fbt.rghost.net%3A80%2Fannounce&tr=http%3A%2F%2Ftracker.openbittorrent.com%3A80%2Fannounce&tr=http%3A%2F%2Ftracker.publicbt.com%3A80%2Fannounce&tr=http%3A%2F%2Ftracker.prq.to%2Fannounce&tr=http%3A%2F%2Fopen.nyaatorrents.info%3A6544%2Fannounce");

            }
        );

        Initialize();
    }

    void TasksViewModel::Initialize()
    {
        std::scoped_lock lk(m_coreMutex);
        if (!m_core)
        {
            m_core = std::make_unique<::OpenNet::Core::Torrent::LibtorrentHandle>();
            m_core->Initialize();
            WireLibtorrentCallbacks();
            m_core->Start();
        }
    }

    void TasksViewModel::Shutdown()
    {
        std::unique_ptr<::OpenNet::Core::Torrent::LibtorrentHandle> tmp;
        {
            std::scoped_lock lk(m_coreMutex);
            tmp = std::move(m_core);
        }
        if (tmp)
        {
            tmp->Stop();
        }
    }

    void TasksViewModel::WireLibtorrentCallbacks()
    {
        if (!m_core) return;
        auto weak = get_weak();
        m_core->SetProgressCallback([weak](::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent const& e)
        {
            if (auto self = weak.get())
            {
                self->OnProgress(e);
            }
        });
        m_core->SetFinishedCallback([weak](std::string const& name)
        {
            if (auto self = weak.get())
            {
                self->OnFinished(name);
            }
        });
        m_core->SetErrorCallback([weak](std::string const& msg)
        {
            if (auto self = weak.get())
            {
                self->OnError(msg);
            }
        });
    }

    winrt::OpenNet::ViewModels::TaskViewModel TasksViewModel::FindOrCreateItem(winrt::hstring const& name)
    {
        for (auto const& item : m_tasks)
        {
            if (item.Name() == name)
            {
                return item;
            }
        }
        // create new
        auto vm = winrt::make<winrt::OpenNet::ViewModels::implementation::TaskViewModel>();
        vm.Name(name);
        m_tasks.Append(vm);
        return vm;
    }

    static winrt::hstring to_hstring_percent(int v)
    {
        wchar_t buf[32];
        swprintf(buf, 32, L"%d%%", v);
        return winrt::hstring{ buf };
    }

    static winrt::hstring to_hstring_rate(int kbps)
    {
        // simplistic formatting
        if (kbps > 1024)
        {
            wchar_t buf[64];
            swprintf(buf, 64, L"%.1f MB/s", kbps / 1024.0);
            return winrt::hstring{ buf };
        }
        wchar_t buf[64];
        swprintf(buf, 64, L"%d KB/s", kbps);
        return winrt::hstring{ buf };
    }

    void TasksViewModel::OnProgress(::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent const& e)
    {
        auto dispatcher = m_dispatcher;
        if (!dispatcher)
            return;
        auto name = winrt::to_hstring(e.name);
        dispatcher.TryEnqueue([weak = get_weak(), name, e]()
        {
            if (auto self = weak.get())
            {
                auto item = self->FindOrCreateItem(name);
                item.Progress(to_hstring_percent(e.progressPercent));
                item.DownloadRate(to_hstring_rate(e.downloadRateKB));
                // Remaining and Size require torrent status; keep placeholders for now
                if (item.Size().empty()) item.Size(L"-");
                if (item.Remaining().empty()) item.Remaining(L"-");
            }
        });
    }

    void TasksViewModel::OnFinished(std::string const& name)
    {
        auto dispatcher = m_dispatcher;
        if (!dispatcher)
            return;
        auto hname = winrt::to_hstring(name);
        dispatcher.TryEnqueue([weak = get_weak(), hname]()
        {
            if (auto self = weak.get())
            {
                auto item = self->FindOrCreateItem(hname);
                item.Progress(L"100%");
                item.DownloadRate(L"0 KB/s");
                item.Remaining(L"0");
            }
        });
    }

    void TasksViewModel::OnError(std::string const& msg)
    {
        // For now, do nothing. Could log to a status view model.
        (void)msg;
    }
}
