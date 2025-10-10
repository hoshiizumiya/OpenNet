#include "pch.h"
#include "ViewModels/TasksViewModel.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>
#include "Core/P2PManager.h"
#include "mvvm_framework/mvvm_hresult_helper.h"

using namespace std::string_literals;

namespace winrt::OpenNet::ViewModels::implementation
{
    TasksViewModel::TasksViewModel()
    {
        m_dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        m_tasks = winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TaskViewModel>();
        m_filteredTasks = winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::TaskViewModel>();

        // NewCommand 需要访问 VM（触发事件），使用弱引用
        m_newCommand = mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](winrt::Windows::Foundation::IInspectable const&)
            {
                if (auto self = weak.get())
                {
                    self->m_addTaskRequested(*self, winrt::hstring());
                }
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
        RebuildFiltered();
    }

    void TasksViewModel::Shutdown()
    {
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
        vm.AddDate(winrt::clock().now().time_since_epoch().count() ? L"" : L""); // placeholder
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
