#include "pch.h"
#include "ViewModels/TasksViewModel.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Pickers.h>
#include "Core/P2PManager.h"
#include "Core/torrentCore/TorrentStateManager.h"
#include "mvvm_framework/mvvm_hresult_helper.h"

#include <ctime>

using namespace std::string_literals;

namespace winrt::OpenNet::ViewModels::implementation
{
    // Tip: All constructions will do in winrt::make<>().
    TasksViewModel::TasksViewModel()
    {
        m_dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        m_tasks = winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TaskViewModel>();
        m_filteredTasks = winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TaskViewModel>();

        // NewCommand: Shows the dropdown menu (no action here, menu items use their own commands)
        m_newCommand = mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](winrt::Windows::Foundation::IInspectable const&)
            {
                if (auto self = weak.get())
                {
                    self->m_addTaskRequested(*self, winrt::hstring());
                }
            })
            .Build();

        // NewFromUrlCommand: Trigger showing the magnet link dialog
        m_newFromUrlCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .ExecuteAsync([](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
            {
                // This will be handled by the code-behind in TasksPage
                // The command simply triggers the event which the page handles
                co_return;
            })
            .Build();

        // NewFromFileCommand: Trigger showing the file picker dialog
        m_newFromFileCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .ExecuteAsync([](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
            {
                // This will be handled by the code-behind in TasksPage
                // The command simply triggers the event which the page handles
                co_return;
            })
            .Build();

        // StartCommand: 必须返回 IAsyncAction（原先 lambda 返回 void 导致赋值到 std::function<IAsyncAction(...)> 失败）
        m_startCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .ExecuteAsync([](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
            {
                co_await winrt::resume_background();
                auto& mgr = ::OpenNet::Core::P2PManager::Instance();
                if (auto core = mgr.TorrentCore())
                {
                    core->Start();
                }
                else
                {
                    co_await mgr.EnsureTorrentCoreInitializedAsync();
                }
                co_return;
            })
            .Build();

        // PauseCommand: 同样需要返回 IAsyncAction
        m_pauseCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .ExecuteAsync([](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
            {
                co_await winrt::resume_background();
                if (auto core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
                {
                    core->Stop();
                }
                co_return;
            })
            .Build();

        // DeleteCommand: 原代码错误使用 winrt::make<DelegateCommandBuilder>（Builder 不是 runtime class——存疑）
        m_deleteCommand = mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](winrt::Windows::Foundation::IInspectable const&)
            {
                if (auto self = weak.get())
                {
                    auto dispatcher = self->m_dispatcher;
                    if (!dispatcher) return;
                    auto vec = self->m_tasks;
                    // 封送正确线程UI线程执行
                    dispatcher.TryEnqueue([weak, vec]()
                    {
                        if (vec)
                        {
                            vec.Clear();
                        }
                        if (auto s = weak.get()) s->RebuildFiltered();
                    });
                }
            })
            .Build();

        // ExportCommand: Export tasks to a file
        m_exportCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .ExecuteAsync([](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
            {
                co_await winrt::resume_background();
                auto& mgr = ::OpenNet::Core::P2PManager::Instance();
                if (mgr.StateManager())
                {
                    // Use a default export path in LocalFolder
                    auto localFolder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
                    std::wstring exportPath = localFolder.Path().c_str();
                    exportPath += L"\\tasks_export.dat";
                    co_await mgr.ExportTasksAsync(exportPath);
                }
                co_return;
            })
            .Build();

        // ImportCommand: Import tasks from a file
        m_importCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .ExecuteAsync([weak = get_weak()](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
            {
                co_await winrt::resume_background();
                auto& mgr = ::OpenNet::Core::P2PManager::Instance();
                if (mgr.StateManager())
                {
                    // Use a default import path in LocalFolder
                    auto localFolder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
                    std::wstring importPath = localFolder.Path().c_str();
                    importPath += L"\\tasks_export.dat";
                    bool result = co_await mgr.ImportTasksAsync(importPath);
                    if (result)
                    {
                        if (auto self = weak.get())
                        {
                            self->LoadSavedTasks();
                        }
                    }
                }
                co_return;
            })
            .Build();

        // 注册回调
        ::OpenNet::Core::P2PManager::Instance().SetProgressCallback([weak = get_weak()](const ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent& e)
        {
            if (auto self = weak.get()) self->OnProgress(e);
        });
        ::OpenNet::Core::P2PManager::Instance().SetFinishedCallback([weak = get_weak()](const std::string& name)
        {
            if (auto self = weak.get()) self->OnFinished(name);
        });
        ::OpenNet::Core::P2PManager::Instance().SetErrorCallback([weak = get_weak()](const std::string& msg)
        {
            if (auto self = weak.get()) self->OnError(msg);
        });

        Initialize();
    }

    void TasksViewModel::Initialize()
    {
        (void)::OpenNet::Core::P2PManager::Instance().EnsureTorrentCoreInitializedAsync();
        LoadSavedTasks();
        RebuildFiltered();
    }

    void TasksViewModel::Shutdown()
    {
    }

    // Helper function to format timestamp to date string
    static winrt::hstring FormatTimestamp(int64_t timestamp)
    {
        if (timestamp <= 0) return L"-";
        
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif
        wchar_t buf[64];
        swprintf(buf, 64, L"%04d-%02d-%02d %02d:%02d",
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min);
        return winrt::hstring{ buf };
    }

    void TasksViewModel::LoadSavedTasks()
    {
        auto dispatcher = m_dispatcher;
        if (!dispatcher) return;

        auto tasks = ::OpenNet::Core::P2PManager::Instance().GetAllTasks();
        
        dispatcher.TryEnqueue([weak = get_weak(), tasks = std::move(tasks)]()
        {
            if (auto self = weak.get())
            {
                for (auto const& task : tasks)
                {
                    winrt::hstring name = task.name.empty() 
                        ? winrt::to_hstring(task.magnetUri.substr(0, 40) + "...") 
                        : winrt::to_hstring(task.name);
                    
                    auto vm = self->FindOrCreateItemByTaskId(task.taskId, name);
                    
                    // Set add date from timestamp
                    vm.AddDate(FormatTimestamp(task.addedTimestamp));
                    
                    // Set progress based on status
                    if (task.status == 3) // Completed
                    {
                        vm.Progress(L"100%");
                        vm.DownloadRate(L"0 KB/s");
                    }
                    else if (task.totalSize > 0 && task.downloadedSize > 0)
                    {
                        int percent = static_cast<int>((task.downloadedSize * 100) / task.totalSize);
                        wchar_t buf[32];
                        swprintf(buf, 32, L"%d%%", percent);
                        vm.Progress(buf);
                    }
                    else
                    {
                        vm.Progress(L"0%");
                    }
                    
                    // Format size
                    if (task.totalSize > 0)
                    {
                        wchar_t sizeBuf[64];
                        double sizeGB = task.totalSize / (1024.0 * 1024.0 * 1024.0);
                        if (sizeGB >= 1.0)
                        {
                            swprintf(sizeBuf, 64, L"%.2f GB", sizeGB);
                        }
                        else
                        {
                            double sizeMB = task.totalSize / (1024.0 * 1024.0);
                            swprintf(sizeBuf, 64, L"%.2f MB", sizeMB);
                        }
                        vm.Size(sizeBuf);
                    }
                    else
                    {
                        vm.Size(L"-");
                    }
                }
                self->RebuildFiltered();
            }
        });
    }

    void TasksViewModel::SelectedTask(winrt::OpenNet::ViewModels::TaskViewModel const& value)
    {
        if (m_selectedTask != value)
        {
            m_selectedTask = value;
            RaisePropertyChangedEvent(L"SelectedTask");
        }
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
        auto vm = winrt::make<winrt::OpenNet::ViewModels::implementation::TaskViewModel>();
        vm.Name(name);
        // Set current time as add date for new items
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        vm.AddDate(FormatTimestamp(timestamp));
        m_tasks.Append(vm);
        return vm;
    }

    winrt::OpenNet::ViewModels::TaskViewModel TasksViewModel::FindOrCreateItemByTaskId(std::string const& taskId, winrt::hstring const& name)
    {
        // First try to find by name (for display purposes)
        for (auto const& item : m_tasks)
        {
            if (item.Name() == name)
            {
                return item;
            }
        }
        
        auto vm = winrt::make<winrt::OpenNet::ViewModels::implementation::TaskViewModel>();
        vm.Name(name);
        // Date will be set by the caller with the correct timestamp
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

    void TasksViewModel::OnProgress(const ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent& e)
    {
        auto dispatcher = m_dispatcher;
        if (!dispatcher) return;
        auto name = winrt::to_hstring(e.name);
        dispatcher.TryEnqueue([weak = get_weak(), name, e]()
        {
            if (auto self = weak.get())
            {
                auto item = self->FindOrCreateItem(name);
                item.Progress(to_hstring_percent(e.progressPercent));
                item.DownloadRate(to_hstring_rate(e.downloadRateKB));
                item.DownloadSpeedKB(static_cast<uint64_t>(e.downloadRateKB));
                item.ProgressPercent(static_cast<double>(e.progressPercent));
                // Update speed graph for this task
                item.UpdateSpeedGraph(static_cast<double>(e.progressPercent), static_cast<uint64_t>(e.downloadRateKB));
                if (item.Size().empty()) item.Size(L"-");
                if (item.Remaining().empty()) item.Remaining(L"-");
                self->RebuildFiltered();
            }
        });
    }

    void TasksViewModel::OnFinished(std::string const& name)
    {
        auto dispatcher = m_dispatcher;
        if (!dispatcher) return;
        auto hname = winrt::to_hstring(name);
        dispatcher.TryEnqueue([weak = get_weak(), hname]()
        {
            if (auto self = weak.get())
            {
                auto item = self->FindOrCreateItem(hname);
                item.Progress(L"100%");
                item.DownloadRate(L"0 KB/s");
                item.Remaining(L"0");
                self->RebuildFiltered();
            }
        });
    }

    void TasksViewModel::OnError(std::string const& msg)
    {
        (void)msg;
    }

    void TasksViewModel::ApplyFilter(winrt::hstring const& tag)
    {
        if (tag != m_currentFilter)
        {
            m_currentFilter = tag;
        }
        RebuildFiltered();
    }

    void TasksViewModel::RebuildFiltered()
    {
        auto dispatcher = m_dispatcher;
        if (!dispatcher) return;
        auto tag = m_currentFilter;
        auto tasks = m_tasks;
        auto filtered = m_filteredTasks;
        dispatcher.TryEnqueue([tag, tasks, filtered]()
        {
            if (!tasks || !filtered) return;
            filtered.Clear();

            for (auto const& item : tasks)
            {
                bool include = false;
                if (tag == L"AllTasks")
                {
                    include = true;
                }
                else if (tag == L"Downloading")
                {
                    // 进度不是100%则认为下载中
                    include = (item.Progress() != L"100%");
                }
                else if (tag == L"Completed")
                {
                    include = (item.Progress() == L"100%");
                }
                else if (tag == L"Failed")
                {
                    // 简化：通过下载速率为0且非100%作为失败/停滞示例
                    include = (item.Progress() != L"100%" && item.DownloadRate() == L"0 KB/s");
                }
                else
                {
                    include = true;
                }

                if (include)
                {
                    filtered.Append(item);
                }
            }
        });
    }
}
