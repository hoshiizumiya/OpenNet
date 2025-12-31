#pragma once
#include "ViewModels/TasksViewModel.g.h"
#include "ViewModels/TaskViewModel.h"
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>
#include <memory>

#include "Core/torrentCore/libtorrentHandle.h"
#include "mvvm_framework/view_model.h"
#include "mvvm_framework/delegate_command.h"
#include "mvvm_framework/delegate_command_builder.h"
#include "mvvm_framework/async_command_builder.h"

namespace winrt::OpenNet::ViewModels::implementation
{
    struct TasksViewModel : TasksViewModelT<TasksViewModel>, ::mvvm::ViewModel<TasksViewModel>
    {
        TasksViewModel();

        // All tasks source collection
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> Tasks() const { return m_tasks; }
        // Filtered view for UI binding
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> FilteredTasks() const { return m_filteredTasks; }

        // Currently selected task
        winrt::OpenNet::ViewModels::TaskViewModel SelectedTask() const { return m_selectedTask; }
        void SelectedTask(winrt::OpenNet::ViewModels::TaskViewModel const& value);

        winrt::Microsoft::UI::Xaml::Input::ICommand NewCommand()   const { return m_newCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand StartCommand() const { return m_startCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand PauseCommand() const { return m_pauseCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand DeleteCommand()const { return m_deleteCommand; }

        void Initialize();
        void Shutdown();

        // Apply filter by tag: "AllTasks", "Downloading", "Completed", "Failed"
        void ApplyFilter(winrt::hstring const& tag);

        winrt::Microsoft::UI::Dispatching::DispatcherQueue Dispatcher() const { return m_dispatcher; }

        // Event API
        winrt::event_token AddTaskRequested(winrt::Windows::Foundation::EventHandler<winrt::hstring> const& handler) { return m_addTaskRequested.add(handler); }
        void AddTaskRequested(winrt::event_token const& token) noexcept { m_addTaskRequested.remove(token); }

    private:
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> m_tasks{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> m_filteredTasks{ nullptr };
        winrt::OpenNet::ViewModels::TaskViewModel m_selectedTask{ nullptr };

        winrt::Microsoft::UI::Xaml::Input::ICommand m_newCommand{ nullptr };
        winrt::Microsoft::UI::Xaml::Input::ICommand m_startCommand{ nullptr };
        winrt::Microsoft::UI::Xaml::Input::ICommand m_pauseCommand{ nullptr };
        winrt::Microsoft::UI::Xaml::Input::ICommand m_deleteCommand{ nullptr };

        // 缺失的调度器字段（原 cpp 使用但未声明）
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

        // Current filter tag
        winrt::hstring m_currentFilter{ L"AllTasks" };

        winrt::OpenNet::ViewModels::TaskViewModel FindOrCreateItem(winrt::hstring const& name);
        void OnProgress(const struct ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent& e);
        void OnFinished(std::string const& name);
        void OnError(std::string const& msg);

        // Rebuild filtered view from full list according to current filter
        void RebuildFiltered();

        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::hstring>> m_addTaskRequested;
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct TasksViewModel : TasksViewModelT<TasksViewModel, implementation::TasksViewModel>
    {
    };
}
