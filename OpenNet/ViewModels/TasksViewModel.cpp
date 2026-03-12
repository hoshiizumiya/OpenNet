#include "pch.h"
#include "ViewModels/TasksViewModel.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Pickers.h>
#include "Core/P2PManager.h"
#include "Core/DownloadManager.h"
#include "Core/HttpStateManager.h"
#include "Core/torrentCore/TorrentStateManager.h"
#include "Core/DataGraph/SpeedGraphDatabase.h"
#include "Core/AppSettingsDatabase.h"
#include "Core/IPFilter/IPFilterManager.h"
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

		// NewFromHttpCommand: Trigger showing the HTTP download dialog
		m_newFromHttpCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
			.ExecuteAsync([](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
		{
			// This will be handled by the code-behind in TasksPage
			co_return;
		})
			.Build();

		// StartCommand: Resume the selected task (or initialize the torrent core if not yet running)
		m_startCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
			.ExecuteAsync([weak = get_weak()](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
		{
			co_await winrt::resume_background();
			auto self = weak.get();
			if (!self) co_return;

			auto selectedTask = self->m_selectedTask;
			if (selectedTask)
			{
				auto taskId = winrt::to_string(selectedTask.TaskId());
				auto taskType = selectedTask.TaskType();

				if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
				{
					auto& mgr = ::OpenNet::Core::P2PManager::Instance();
					co_await mgr.EnsureTorrentCoreInitializedAsync();
					if (auto core = mgr.TorrentCore())
					{
						if (!taskId.empty())
							core->ResumeTorrent(taskId);
						else
							core->Start();
					}
				}
				else if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::Http)
				{
					auto gid = winrt::to_string(selectedTask.Gid());
					auto& dlMgr = ::OpenNet::Core::DownloadManager::Instance();
					if (!gid.empty())
						dlMgr.ResumeHttpDownload(gid);
				}
			}
			else
			{
				// No task selected - just ensure the core is initialized
				auto& mgr = ::OpenNet::Core::P2PManager::Instance();
				co_await mgr.EnsureTorrentCoreInitializedAsync();
			}
			co_return;
		})
			.Build();

		// PauseCommand: Pause the selected task
		m_pauseCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
			.ExecuteAsync([weak = get_weak()](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
		{
			co_await winrt::resume_background();
			auto self = weak.get();
			if (!self) co_return;

			auto selectedTask = self->m_selectedTask;
			if (selectedTask)
			{
				auto taskId = winrt::to_string(selectedTask.TaskId());
				auto taskType = selectedTask.TaskType();

				if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
				{
					if (auto core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
					{
						if (!taskId.empty())
							core->PauseTorrent(taskId);
					}
				}
				else if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::Http)
				{
					auto gid = winrt::to_string(selectedTask.Gid());
					auto& dlMgr = ::OpenNet::Core::DownloadManager::Instance();
					if (!gid.empty())
						dlMgr.PauseHttpDownload(gid);
				}
			}
			co_return;
		})
			.Build();

		// DeleteCommand: Delete only the selected task (not all tasks)
		// Uses AsyncCommandBuilder to avoid blocking the STA thread with Aria2 HTTP calls.
		m_deleteCommand = mvvm::AsyncCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
			.ExecuteAsync([weak = get_weak()](winrt::Windows::Foundation::IInspectable const&) -> winrt::Windows::Foundation::IAsyncAction
		{
			auto self = weak.get();
			if (!self) co_return;

			// Capture selection info on the UI thread before switching threads
			auto selectedTask = self->m_selectedTask;
			if (!selectedTask) co_return;

			auto taskId = winrt::to_string(selectedTask.TaskId());
			auto taskType = selectedTask.TaskType();
			auto gid = winrt::to_string(selectedTask.Gid());

			// Do backend removal on a background thread to avoid STA blocking
			co_await winrt::resume_background();
			try
			{
				if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
				{
					auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
					if (core && !taskId.empty())
					{
						core->RemoveTorrent(taskId, false);
					}
					auto* stateMgr = ::OpenNet::Core::P2PManager::Instance().StateManager();
					if (stateMgr && !taskId.empty())
					{
						stateMgr->DeleteTask(taskId);
					}
					// Clean up speed graph persistence
					if (!taskId.empty())
					{
						::OpenNet::Core::SpeedGraphDatabase::Instance().DeleteTask(taskId);
					}
				}
				else if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::Http)
				{
					// Mark GID as deleted BEFORE cancelling, so callbacks won't
					// re-create the task via FindOrCreateHttpItem.
					if (!gid.empty())
					{
						self->m_deletedGids.insert(gid);
					}

					auto& dlMgr = ::OpenNet::Core::DownloadManager::Instance();
					if (!gid.empty())
					{
						// First cancel the active download (aria2.remove),
						// then clean up the result (aria2.removeDownloadResult).
						try { dlMgr.CancelHttpDownload(gid); } catch (...) {}
						try { dlMgr.RemoveHttpDownload(gid); } catch (...) {}
					}
					// Delete the persisted HTTP download record.
					// TaskId now holds the stable recordId (not GID).
					auto& httpState = ::OpenNet::Core::HttpStateManager::Instance();
					if (!taskId.empty())
					{
						httpState.DeleteRecord(taskId);
					}
					else if (!gid.empty())
					{
						// Fallback: look up recordId from GID mapping
						auto recordId = dlMgr.GetRecordIdForGid(gid);
						if (!recordId.empty())
						{
							httpState.DeleteRecord(recordId);
						}
					}
					// Clean up speed graph persistence
					if (!taskId.empty())
					{
						::OpenNet::Core::SpeedGraphDatabase::Instance().DeleteTask(taskId);
					}
				}
			}
			catch (const std::exception& ex)
			{
				OutputDebugStringW((L"DeleteCommand backend error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
			}
			catch (...) {}

			// Back to UI thread for observable collection mutation
			auto dispatcher = self->m_dispatcher;
			if (!dispatcher) co_return;
			dispatcher.TryEnqueue([weak, selectedTask]()
			{
				auto self = weak.get();
				if (!self) return;

				auto& tasks = self->m_tasks;
				for (uint32_t i = 0; i < tasks.Size(); ++i)
				{
					if (tasks.GetAt(i) == selectedTask)
					{
						tasks.RemoveAt(i);
						break;
					}
				}

				self->m_selectedTask = nullptr;
				self->RebuildFiltered();
			});
			co_return;
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

		// Register Aria2 (HTTP) callbacks
		auto& dlMgr = ::OpenNet::Core::DownloadManager::Instance();
		dlMgr.SetHttpProgressCallback([weak = get_weak()](::OpenNet::Core::HttpTaskProgress const& p)
		{
			if (auto self = weak.get()) self->OnHttpProgress(p);
		});
		dlMgr.SetHttpFinishedCallback([weak = get_weak()](std::string const& gid, std::string const& name)
		{
			if (auto self = weak.get()) self->OnHttpFinished(gid, name);
		});
		dlMgr.SetHttpErrorCallback([weak = get_weak()](std::string const& gid, std::string const& msg)
		{
			if (auto self = weak.get()) self->OnHttpError(gid, msg);
		});

		Initialize();
	}

	void TasksViewModel::Initialize()
	{
		// Initialize HTTP download manager synchronously
		::OpenNet::Core::DownloadManager::Instance().Initialize();

		// Initialize application settings database (used by RSS, UI settings, etc.)
		::OpenNet::Core::AppSettingsDatabase::Instance().Initialize();

		// Initialize speed graph persistence database
		::OpenNet::Core::SpeedGraphDatabase::Instance().Initialize();

		// Initialize IP filter database
		::OpenNet::Core::IPFilterManager::Instance().Initialize();

		// Fire-and-forget: await torrent core init on background, then load tasks
		[](auto weak) -> winrt::Windows::Foundation::IAsyncAction
		{
			co_await winrt::resume_background();
			co_await ::OpenNet::Core::P2PManager::Instance().EnsureTorrentCoreInitializedAsync();

			// Apply stored IP filter rules to the now-running session
			::OpenNet::Core::IPFilterManager::Instance().ApplyToSession();

			if (auto self = weak.get())
			{
				self->LoadSavedTasks();
			}
		}(get_weak());
	}

	void TasksViewModel::Shutdown()
	{
		::OpenNet::Core::DownloadManager::Instance().Shutdown();
	}

	// Helper function to format timestamp to date string
	static winrt::hstring FormatTimestamp(int64_t timestamp)
	{
		if (timestamp <= 0)
			return L"-";

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
		if (!dispatcher)
			return;

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

		// Also load persisted HTTP download records
		auto httpRecords = ::OpenNet::Core::HttpStateManager::Instance().LoadAllRecords();

		dispatcher.TryEnqueue([weak = get_weak(), httpRecords = std::move(httpRecords)]()
		{
			if (auto self = weak.get())
			{
				for (auto const& rec : httpRecords)
				{
					winrt::hstring name = winrt::to_hstring(rec.name);
					winrt::hstring gid = winrt::to_hstring(rec.lastGid.empty() ? rec.recordId : rec.lastGid);

					auto vm = self->FindOrCreateHttpItem(gid, name);

					// Ensure TaskId always points to the stable recordId
					vm.TaskId(winrt::to_hstring(rec.recordId));

					// Set add date
					vm.AddDate(FormatTimestamp(rec.addedTimestamp));

					// Set progress
					if (rec.status == 3) // Completed
					{
						vm.Progress(L"100%");
						vm.DownloadRate(L"0 KB/s");
					}
					else if (rec.totalSize > 0 && rec.completedSize > 0)
					{
						int pct = static_cast<int>((rec.completedSize * 100) / rec.totalSize);
						wchar_t buf[32];
						swprintf(buf, 32, L"%d%%", pct);
						vm.Progress(buf);
					}
					else
					{
						vm.Progress(L"0%");
					}

					// Format size
					if (rec.totalSize > 0)
					{
						wchar_t sizeBuf[64];
						double sizeGB = rec.totalSize / (1024.0 * 1024.0 * 1024.0);
						if (sizeGB >= 1.0)
							swprintf(sizeBuf, 64, L"%.2f GB", sizeGB);
						else
						{
							double sizeMB = rec.totalSize / (1024.0 * 1024.0);
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
		auto htaskId = winrt::to_hstring(taskId);
		// First try to find by taskId
		for (auto const& item : m_tasks)
		{
			if (item.TaskId() == htaskId)
			{
				return item;
			}
		}
		// Fallback: try by name
		for (auto const& item : m_tasks)
		{
			if (item.Name() == name && item.TaskId().empty())
			{
				item.TaskId(htaskId);
				return item;
			}
		}

		auto vm = winrt::make<winrt::OpenNet::ViewModels::implementation::TaskViewModel>();
		vm.Name(name);
		vm.TaskId(htaskId);
		vm.TaskType(winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent);
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
		if (!dispatcher)
			return;
		auto name = winrt::to_hstring(e.name);
		dispatcher.TryEnqueue([weak = get_weak(), name, e]()
		{
			if (auto self = weak.get())
			{
				auto sizeBefore = self->m_tasks.Size();
				auto item = self->FindOrCreateItem(name);
				bool isNewItem = (self->m_tasks.Size() > sizeBefore);
				if (item.TaskId().empty())
				{
					auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
					if (core)
					{
						auto taskId = core->GetTaskIdByName(winrt::to_string(name));
						if (!taskId.empty())
						{
							item.TaskId(winrt::to_hstring(taskId));
							item.TaskType(winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent);
						}
					}
				}
				item.Progress(to_hstring_percent(e.progressPercent));
				item.DownloadRate(to_hstring_rate(e.downloadRateKB));
				item.UploadRate(to_hstring_rate(e.uploadRateKB));
				item.DownloadSpeedKB(static_cast<uint64_t>(e.downloadRateKB));
				item.ProgressPercent(static_cast<double>(e.progressPercent));
				item.UpdateSpeedGraph(static_cast<double>(e.progressPercent), static_cast<uint64_t>(e.downloadRateKB));
				if (item.Size().empty()) item.Size(L"-");
				if (item.Remaining().empty()) item.Remaining(L"-");
				// Only RebuildFiltered when a brand-new item was created so it
				// appears in the filtered list. Subsequent ticks skip this.
				if (isNewItem) self->RebuildFiltered();
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
				self->RebuildFiltered();
			}
		});
	}

	void TasksViewModel::OnError(std::string const& msg)
	{
		(void)msg;
	}

	// ------------------------------------------------------------------
	//  HTTP (Aria2) helpers & callbacks
	// ------------------------------------------------------------------

	winrt::OpenNet::ViewModels::TaskViewModel TasksViewModel::FindOrCreateHttpItem(winrt::hstring const& gid, winrt::hstring const& name)
	{
		// If this GID was explicitly deleted, do NOT re-create it.
		auto gidStr = winrt::to_string(gid);
		if (m_deletedGids.count(gidStr))
		{
			return nullptr;
		}

		// Lookup by GID first
		for (auto const& item : m_tasks)
		{
			if (item.Gid() == gid)
			{
				return item;
			}
		}
		// Create new
		auto vm = winrt::make<winrt::OpenNet::ViewModels::implementation::TaskViewModel>();
		vm.Name(name.empty() ? gid : name);
		vm.Gid(gid);
		// Use recordId (not GID) as TaskId for HTTP downloads.
		// GIDs are Aria2 session-ephemeral and change across restarts.
		auto recordId = ::OpenNet::Core::DownloadManager::Instance().GetRecordIdForGid(gidStr);
		vm.TaskId(recordId.empty() ? gid : winrt::to_hstring(recordId));
		vm.TaskType(winrt::OpenNet::ViewModels::DownloadTaskType::Http);
		auto now = std::chrono::system_clock::now();
		auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
		vm.AddDate(FormatTimestamp(ts));
		m_tasks.Append(vm);
		return vm;
	}

	static winrt::hstring FormatByteSize(std::uint64_t bytes)
	{
		if (bytes == 0)
			return L"-";
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

	static winrt::hstring FormatByteRate(std::uint64_t bytesPerSec)
	{
		wchar_t buf[64];
		double kbs = bytesPerSec / 1024.0;
		if (kbs > 1024.0)
			swprintf(buf, 64, L"%.1f MB/s", kbs / 1024.0);
		else
			swprintf(buf, 64, L"%.0f KB/s", kbs);
		return winrt::hstring{ buf };
	}

	void TasksViewModel::OnHttpProgress(::OpenNet::Core::HttpTaskProgress const& progress) const
	{
		auto dispatcher = m_dispatcher;
		if (!dispatcher)
			return;

		// Copy values for capture
		auto gid = winrt::to_hstring(progress.gid);
		auto name = winrt::to_hstring(progress.name);
		auto totalLen = progress.totalLength;
		auto completedLen = progress.completedLength;
		auto dlSpeed = progress.downloadSpeed;
		auto ulSpeed = progress.uploadSpeed;
		auto percent = progress.progressPercent;
		auto status = progress.status;

		dispatcher.TryEnqueue([weak = get_weak(), gid, name, totalLen, completedLen, dlSpeed, ulSpeed, percent, status]()
		{
			if (auto self = weak.get())
			{
				auto sizeBefore = self->m_tasks.Size();
				auto item = self->FindOrCreateHttpItem(gid, name);
				if (!item) return; // GID was deleted
				bool isNewItem = (self->m_tasks.Size() > sizeBefore);

				// Update name if it changed (aria2 resolves filename later)
				if (!name.empty() && item.Name() != name)
					item.Name(name);

				item.Progress(to_hstring_percent(percent));
				item.DownloadRate(FormatByteRate(dlSpeed));
				item.UploadRate(FormatByteRate(ulSpeed));
				item.Size(FormatByteSize(totalLen));
				item.DownloadSpeedKB(dlSpeed / 1024);
				item.ProgressPercent(static_cast<double>(percent));
				item.UpdateSpeedGraph(static_cast<double>(percent), dlSpeed / 1024);

				// Estimate remaining time
				if (dlSpeed > 0 && totalLen > completedLen)
				{
					auto remainingSec = (totalLen - completedLen) / dlSpeed;
					wchar_t buf[64];
					if (remainingSec > 3600)
						swprintf(buf, 64, L"%lluh %llum", remainingSec / 3600, (remainingSec % 3600) / 60);
					else if (remainingSec > 60)
						swprintf(buf, 64, L"%llum %llus", remainingSec / 60, remainingSec % 60);
					else
						swprintf(buf, 64, L"%llus", remainingSec);
					item.Remaining(buf);
				}
				else
				{
					item.Remaining(L"-");
				}
				// Only RebuildFiltered when a brand-new item was created so it
				// appears in the filtered list. Subsequent ticks skip this.
				if (isNewItem) self->RebuildFiltered();
			}
		});
	}

	void TasksViewModel::OnHttpFinished(std::string const& gid, std::string const& name) const
	{
		auto dispatcher = m_dispatcher;
		if (!dispatcher)
			return;
		auto hgid = winrt::to_hstring(gid);
		auto hname = winrt::to_hstring(name);
		dispatcher.TryEnqueue([weak = get_weak(), hgid, hname]()
		{
			if (auto self = weak.get())
			{
				auto item = self->FindOrCreateHttpItem(hgid, hname);
				if (!item) return; // GID was deleted
				item.Progress(L"100%");
				item.DownloadRate(L"0 KB/s");
				item.UploadRate(L"0 KB/s");
				item.Remaining(L"0");
				self->RebuildFiltered();
			}
		});
	}

	void TasksViewModel::OnHttpError(std::string const& gid, std::string const& message) const
	{
		(void)message;
		auto dispatcher = m_dispatcher;
		if (!dispatcher)
			return;
		auto hgid = winrt::to_hstring(gid);
		dispatcher.TryEnqueue([weak = get_weak(), hgid]()
		{
			if (auto self = weak.get())
			{
				auto item = self->FindOrCreateHttpItem(hgid, L"");
				if (!item) return; // GID was deleted
				item.DownloadRate(L"Error");
				self->RebuildFiltered();
			}
		});
	}

	void TasksViewModel::ApplyFilter(winrt::hstring const& tag)
	{
		if (tag != m_currentFilter)
		{
			m_currentFilter = tag;
		}
		RebuildFiltered();
	}

	void TasksViewModel::SetSearchFilter(winrt::hstring const& text)
	{
		m_searchText = text;
		RebuildFiltered();
	}

	// Helper: case-insensitive substring check
	static bool ContainsIgnoreCase(winrt::hstring const& source, winrt::hstring const& sub)
	{
		if (sub.empty()) return true;
		std::wstring src(source);
		std::wstring s(sub);
		std::transform(src.begin(), src.end(), src.begin(), ::towlower);
		std::transform(s.begin(), s.end(), s.begin(), ::towlower);
		return src.find(s) != std::wstring::npos;
	}

	void TasksViewModel::RebuildFiltered()
	{
		auto dispatcher = m_dispatcher;
		if (!dispatcher)
			return;
		auto tag = m_currentFilter;
		auto searchText = m_searchText;
		auto tasks = m_tasks;
		auto filtered = m_filteredTasks;
		dispatcher.TryEnqueue([tag, searchText, tasks, filtered]()
		{
			if (!tasks || !filtered) return;

			// Build desired set
			std::vector<winrt::OpenNet::ViewModels::TaskViewModel> desired;
			for (auto const& item : tasks)
			{
				bool include = false;
				if (tag == L"AllTasks")
				{
					include = true;
				}
				else if (tag == L"Downloading")
				{
					include = (item.Progress() != L"100%");
				}
				else if (tag == L"Completed")
				{
					include = (item.Progress() == L"100%");
				}
				else if (tag == L"Failed")
				{
					include = (item.Progress() != L"100%" && item.DownloadRate() == L"0 KB/s");
				}
				else
				{
					include = true;
				}

				// Apply search text filter
				if (include && !searchText.empty())
				{
					include = ContainsIgnoreCase(item.Name(), searchText);
				}

				if (include)
				{
					desired.push_back(item);
				}
			}

			// Incremental update: remove items not in desired, add missing items.
			// This avoids clearing the list (which triggers ListView re-entrance animations).

			// 1. Remove items no longer matching
			for (int i = static_cast<int>(filtered.Size()) - 1; i >= 0; --i)
			{
				auto existing = filtered.GetAt(static_cast<uint32_t>(i));
				bool found = false;
				for (auto const& d : desired)
				{
					if (d == existing) { found = true; break; }
				}
				if (!found)
				{
					filtered.RemoveAt(static_cast<uint32_t>(i));
				}
			}

			// 2. Add items that are in desired but not yet in filtered
			for (auto const& d : desired)
			{
				bool found = false;
				for (uint32_t j = 0; j < filtered.Size(); ++j)
				{
					if (filtered.GetAt(j) == d) { found = true; break; }
				}
				if (!found)
				{
					filtered.Append(d);
				}
			}
		});
	}
}
