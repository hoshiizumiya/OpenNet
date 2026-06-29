#pragma once
#include "ViewModels/TasksViewModel.g.h"
#include "ViewModels/TaskViewModel.h"

#include "mvvm_framework/view_model.h"
#include "mvvm_framework/delegate_command.h"
#include "mvvm_framework/delegate_command_builder.h"
#include "mvvm_framework/async_command_builder.h"

import OpenNet.Core.DownloadManager;
import OpenNet.Core.torrentCore.LibtorrentHandle;
import winrt.Microsoft.UI.Xaml.Input;
import winrt.Windows.Foundation.Collections;
import winrt.Microsoft.UI.Dispatching;
import winrt.Windows.Foundation;

namespace winrt::OpenNet::ViewModels::implementation
{
	struct TasksViewModel : TasksViewModelT<TasksViewModel>, ::mvvm::ViewModel<TasksViewModel>
	{
		TasksViewModel();

		// All tasks source collection
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> Tasks() const
		{
			return m_tasks;
		}
		// Filtered view for UI binding
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> FilteredTasks() const
		{
			return m_filteredTasks;
		}

		// Currently selected task
		winrt::OpenNet::ViewModels::TaskViewModel SelectedTask() const
		{
			return m_selectedTask;
		}
		void SelectedTask(winrt::OpenNet::ViewModels::TaskViewModel const& value);

		winrt::Microsoft::UI::Xaml::Input::ICommand NewCommand() const
		{
			return m_newCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand NewFromUrlCommand() const
		{
			return m_newFromUrlCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand NewFromFileCommand() const
		{
			return m_newFromFileCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand NewFromHttpCommand() const
		{
			return m_newFromHttpCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand StartCommand() const
		{
			return m_startCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand PauseCommand() const
		{
			return m_pauseCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand DeleteCommand() const
		{
			return m_deleteCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand ExportCommand() const
		{
			return m_exportCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand ImportCommand() const
		{
			return m_importCommand;
		}

		void Initialize();
		void Shutdown();

		// Apply filter by tag: "AllTasks", "Downloading", "Completed", "Failed"
		void ApplyFilter(winrt::hstring const& tag);

		// Text search filter
		void SetSearchFilter(winrt::hstring const& text);

		winrt::Microsoft::UI::Dispatching::DispatcherQueue Dispatcher() const
		{
			return m_dispatcher;
		}

		// Event API
		winrt::event_token AddTaskRequested(winrt::Windows::Foundation::EventHandler<winrt::hstring> const& handler)
		{
			return m_addTaskRequested.add(handler);
		}
		void AddTaskRequested(winrt::event_token const& token) noexcept
		{
			m_addTaskRequested.remove(token);
		}

		bool IsColNameLoad();
		void IsColNameLoad(bool value);
		bool IsColSizeLoad();
		void IsColSizeLoad(bool value);
		bool IsColProgressLoad();
		void IsColProgressLoad(bool value);
		bool IsColDownloadSizeLoad();
		void IsColDownloadSizeLoad(bool value);
		bool IsColUploadSizeLoad();
		void IsColUploadSizeLoad(bool value);
		bool IsColumnTotalDownloadSizeLoad();
		void IsColumnTotalDownloadSizeLoad(bool value);
		bool IsColumnTotalUploadSizeLoad();
		void IsColumnTotalUploadSizeLoad(bool value);
		bool IsColDLRateLoad();
		void IsColDLRateLoad(bool value);
		bool IsColULRateLoad();
		void IsColULRateLoad(bool value);
		bool IsColRemainingLoad();
		void IsColRemainingLoad(bool value);
		bool IsColAddDateLoad();
		void IsColAddDateLoad(bool value);

	private:
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> m_tasks{ nullptr };
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::ViewModels::TaskViewModel> m_filteredTasks{ nullptr };
		winrt::OpenNet::ViewModels::TaskViewModel m_selectedTask{ nullptr };

		winrt::Microsoft::UI::Xaml::Input::ICommand m_newCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_newFromUrlCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_newFromFileCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_newFromHttpCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_startCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_pauseCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_deleteCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_exportCommand{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_importCommand{ nullptr };

		// 缺失的调度器字段（原 cpp 使用但未声明）
		winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

		// Current filter tag
		winrt::hstring m_currentFilter{ L"AllTasks" };
		winrt::hstring m_searchText;

		winrt::OpenNet::ViewModels::TaskViewModel FindOrCreateItem(winrt::hstring const& name);
		winrt::OpenNet::ViewModels::TaskViewModel FindOrCreateItemByTaskId(std::string const& taskId, winrt::hstring const& name);
		winrt::OpenNet::ViewModels::TaskViewModel FindOrCreateHttpItem(winrt::hstring const& gid, winrt::hstring const& name);
		void OnProgress(const struct ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent& e);
		void OnFinished(std::string const& name);
		void OnError(std::string const& msg);
		void OnHttpProgress(::OpenNet::Core::HttpTaskProgress const& progress);
		void OnHttpFinished(std::string const& gid, std::string const& name);
		void OnHttpError(std::string const& gid, std::string const& message);
		void LoadSavedTasks();

		// Rebuild filtered view from full list according to current filter
		void RebuildFiltered();

		winrt::event<winrt::Windows::Foundation::EventHandler<winrt::hstring>> m_addTaskRequested;

		// GIDs that were explicitly deleted ─ prevents OnHttpProgress/Finished
		// from re-creating the task after deletion.
		std::unordered_set<std::string> m_deletedGids;

		bool m_isColNameLoad{ true };
		bool m_isColSizeLoad{ true };
		bool m_isColProgressLoad{ true };
		bool m_isColDownloadSizeLoad{ true };
		bool m_isColUploadSizeLoad{ true };
		bool m_isColumnTotalDownloadSizeLoad{ true };
		bool m_isColumnTotalUploadSizeLoad{ true };
		bool m_isColDLRateLoad{ true };
		bool m_isColULRateLoad{ true };
		bool m_isColRemainingLoad{ true };
		bool m_isColAddDateLoad{ true };
	};
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct TasksViewModel : TasksViewModelT<TasksViewModel, implementation::TasksViewModel>
	{
	};
}
