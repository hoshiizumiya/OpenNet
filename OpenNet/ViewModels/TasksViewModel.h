#pragma once
#include "ViewModels/TasksViewModel.g.h"
#include "ViewModels/TaskViewModel.h"
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <memory>
#include <mutex>

#include "Core/torrentCore/libtorrentHandle.h"
#include "mvvm_framework/view_model.h"
#include "mvvm_framework/delegate_command.h"

namespace winrt::OpenNet::ViewModels::implementation
{
    // ViewModel managing torrent tasks list populated from libtorrent
    struct TasksViewModel : TasksViewModelT<TasksViewModel>, ::mvvm::view_model<TasksViewModel>
    {
        TasksViewModel();

        // IObservableVector of TaskViewModel for binding
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> Tasks() const { return m_tasks; }

        // Toolbar commands
        winrt::Microsoft::UI::Xaml::Input::ICommand StartCommand() const { return m_startCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand PauseCommand() const { return m_pauseCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand NewCommand() const { return m_newCommand; }

        // lifecycle
        void Initialize();
        void Shutdown();

        // Internal: wire to native core
        void WireLibtorrentCallbacks();

        // Dispatcher accessor required by mvvm::view_model_base
        winrt::Microsoft::UI::Dispatching::DispatcherQueue Dispatcher() const { return m_dispatcher; }

    private:
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> m_tasks{ nullptr };

        winrt::Microsoft::UI::Xaml::Input::ICommand m_startCommand{ nullptr };
        winrt::Microsoft::UI::Xaml::Input::ICommand m_pauseCommand{ nullptr };
        winrt::Microsoft::UI::Xaml::Input::ICommand m_newCommand{ nullptr };

        std::unique_ptr<::OpenNet::Core::Torrent::LibtorrentHandle> m_core;
        std::mutex m_coreMutex;

        // Helper: find existing item by name
        winrt::OpenNet::ViewModels::TaskViewModel FindOrCreateItem(winrt::hstring const& name);

        void OnProgress(::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent const& e);
        void OnFinished(std::string const& name);
        void OnError(std::string const& msg);
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct TasksViewModel : TasksViewModelT<TasksViewModel, implementation::TasksViewModel>
    {
    };
}
