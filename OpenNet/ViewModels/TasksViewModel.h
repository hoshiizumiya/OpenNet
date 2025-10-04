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

namespace winrt::OpenNet::ViewModels::implementation
{
	struct TasksViewModel : TasksViewModelT<TasksViewModel>, ::mvvm::view_model<TasksViewModel>
	{
		TasksViewModel();

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> Tasks() const { return m_tasks; }

		winrt::Microsoft::UI::Xaml::Input::ICommand NewCommand() const { return m_newCommand; }
		winrt::Microsoft::UI::Xaml::Input::ICommand StartCommand() const { return m_startCommand; }
		winrt::Microsoft::UI::Xaml::Input::ICommand PauseCommand() const { return m_pauseCommand; }
		winrt::Microsoft::UI::Xaml::Input::ICommand DeleteCommand() const { return m_deleteCommand; }

		void Initialize();
		void Shutdown();

		winrt::Microsoft::UI::Dispatching::DispatcherQueue Dispatcher() const { return m_dispatcher; }

        // Event API (public so projected type can subscribe)
        winrt::event_token AddTaskRequested(winrt::Windows::Foundation::EventHandler<winrt::hstring> const& handler) { return m_addTaskRequested.add(handler); }
        void AddTaskRequested(winrt::event_token const& token) noexcept { m_addTaskRequested.remove(token); }

	private:
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> m_tasks{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_newCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_startCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_pauseCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_deleteCommand{ nullptr };

		winrt::OpenNet::ViewModels::TaskViewModel FindOrCreateItem(winrt::hstring const& name);
		void OnProgress(const struct ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent& e);
		void OnFinished(std::string const& name);
		void OnError(std::string const& msg);

        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::hstring>> m_addTaskRequested;
	};
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct TasksViewModel : TasksViewModelT<TasksViewModel, implementation::TasksViewModel>
	{
	};
}
