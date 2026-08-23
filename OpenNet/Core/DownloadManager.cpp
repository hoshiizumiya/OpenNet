/*
 * PROJECT:   OpenNet
 * FILE:      Core/DownloadManager.cpp
 * PURPOSE:   Unified download manager – aria2 HTTP engine integration
 *
 * LICENSE:   The MIT License
 */
module;

#include "Core/Notification/HttpToastNotification.h"

module OpenNet.Core.DownloadManager;

import OpenNet.Core.AppSettingsDatabase;

namespace OpenNet::Core
{
	using namespace std::chrono_literals;

	// ------------------------------------------------------------------
	//  Singleton accessor
	// ------------------------------------------------------------------
	DownloadManager& DownloadManager::Instance()
	{
		static DownloadManager s_instance;
		return s_instance;
	}

	DownloadManager::~DownloadManager()
	{
		// Destructor runs during atexit on the main (STA) thread.
		// Only stop the refresh thread and force-kill aria2 process.
		// Never do RPC calls (.get()) here — that triggers STA assertion.
		// Graceful shutdown (RPC + wait) is done by ShutdownEngines() on a
		// background thread before we get here.
		try
		{
			{
				std::lock_guard stopLock(m_stopMutex);
				m_stopRefresh.store(true);
			}
			m_stopCv.notify_all();
			if (m_refreshThread.joinable())
				m_refreshThread.join();

			if (m_aria2)
				m_aria2->ForceTerminate();
		}
		catch (...)
		{
		}
	}

	// ------------------------------------------------------------------
	//  Lifecycle
	// ------------------------------------------------------------------
	winrt::Windows::Foundation::IAsyncAction DownloadManager::InitializeAsync()
	{
		co_await winrt::resume_background();

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_initialized || m_initializing)
				co_return;
			m_initializing = true;
		}

		std::unique_ptr<Aria2::LocalAria2Instance> aria2 = std::make_unique<Aria2::LocalAria2Instance>();

		try
		{
			// Async startup: locate aria2c, prepare process, and start
			co_await aria2->StartupAsync();

			// Initialize HTTP download record persistence
			HttpStateManager::Instance().Initialize();

			// Rebuild GID→recordId mapping from persisted records so that
			// GetRecordIdForGid() works correctly after an app restart.
			auto records = HttpStateManager::Instance().LoadAllRecords();

			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_aria2 = std::move(aria2);

				for (auto const& rec : records)
				{
					if (!rec.lastGid.empty())
					{
						m_gidToRecordId[rec.lastGid] = rec.recordId;
					}
				}

				// Start periodic refresh thread
				m_stopRefresh.store(false);
				m_refreshThread = std::thread([this]()
				{
					RefreshThreadEntry();
				});

				m_initialized = true;
				m_initializing = false;
			}
		}
		catch (...)
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_initializing = false;
			}
			throw;
		}
	}

	void DownloadManager::Shutdown()
	{
		{
			std::lock_guard lock(m_mutex);
			if (!m_initialized)
				return;
			m_initialized = false;
		}

		// Stop refresh thread
		{
			std::lock_guard stopLock(m_stopMutex);
			m_stopRefresh.store(true);
		}
		m_stopCv.notify_all();
		if (m_refreshThread.joinable())
			m_refreshThread.join();

		// Graceful aria2 shutdown following NanaGet pattern:
		// RPC Shutdown → wait up to 30s for process exit → ForceTerminate.
		// This runs on a background thread (ShutdownEngines) so .get() is safe.
		if (m_aria2)
		{
			try
			{
				m_aria2->Terminate();
			}
			catch (...)
			{
			}
		}

		m_aria2.reset();
	}

	bool DownloadManager::IsAria2Available() const
	{
		if (!m_aria2)
			return false;
		return m_aria2->Available();
	}

	// ------------------------------------------------------------------
	//  HTTP download operations
	// ------------------------------------------------------------------
	std::string DownloadManager::AddHttpDownload(
		std::string const& url,
		std::string const& dir,
		std::string const& fileName)
	{
		Aria2::HttpDownloadOptions options;
		options.Uris.push_back(url);
		options.Dir = dir;
		options.OutFileName = fileName;
		return AddHttpDownload(options);
	}

	std::string DownloadManager::AddHttpDownload(Aria2::HttpDownloadOptions const& options)
	{
		if (!IsAria2Available() || options.Uris.empty()) return {};

		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			auto& database = AppSettingsDatabase::Instance();
			database.Initialize();
			auto effectiveOptions = options;
			if (effectiveOptions.ConnectionsPerServer == 0)
				effectiveOptions.ConnectionsPerServer = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
				database.GetInt(AppSettingsDatabase::CAT_DOWNLOAD,
					"aria2_connections_per_server", 8), 1, 16));
			auto const gid = m_aria2->AddUriWithOptions(effectiveOptions);

			// Persist the download record
			if (!gid.empty())
			{
				if (!effectiveOptions.Description.empty()) database.SetString("http_task_description", gid, effectiveOptions.Description);
				auto recordId = HttpStateManager::Instance().AddRecord(effectiveOptions.Uris.front(), effectiveOptions.Dir, effectiveOptions.OutFileName);
				HttpStateManager::Instance().UpdateRecordGid(recordId, gid);
				{
					std::lock_guard lock(m_mutex);
					m_gidToRecordId[gid] = recordId;
					m_lastHttpStatuses[gid] = effectiveOptions.StartPaused ? Aria2::DownloadStatus::Paused : Aria2::DownloadStatus::Waiting;
					m_httpTaskLogs[gid].push_back({ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(), effectiveOptions.StartPaused ? "Task added in paused state." : "Download task started." });
					if (!effectiveOptions.Description.empty()) m_httpTaskLogs[gid].push_back({ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(), "Description: " + effectiveOptions.Description });
				}
			}

			return gid;
		}
		catch (...)
		{
			return {};
		}
	}

	std::optional<Aria2::DownloadInformation> DownloadManager::GetHttpTaskInformation(std::string const& gid)
	{
		if (!IsAria2Available() || gid.empty()) return std::nullopt;
		{
			std::lock_guard lock(m_mutex);
			if (auto const snapshot = m_httpTaskSnapshots.find(gid); snapshot != m_httpTaskSnapshots.end()) return snapshot->second;
		}
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			return m_aria2->GetTaskInformation(gid);
		}
		catch (...) { return std::nullopt; }
	}

	std::vector<Aria2::ServersInformation> DownloadManager::GetHttpTaskServers(std::string const& gid)
	{
		if (!IsAria2Available() || gid.empty()) return {};
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			return m_aria2->GetTaskServers(gid);
		}
		catch (...) { return {}; }
	}

	std::vector<HttpTaskLogEntry> DownloadManager::GetHttpTaskLog(std::string const& gid) const
	{
		std::lock_guard lock(m_mutex);
		if (auto const entries = m_httpTaskLogs.find(gid); entries != m_httpTaskLogs.end()) return entries->second;
		return {};
	}

	std::string DownloadManager::GetRecordIdForGid(std::string const& gid) const
	{
		// First try in-memory cache
		{
			std::lock_guard lock(m_mutex);
			auto it = m_gidToRecordId.find(gid);
			if (it != m_gidToRecordId.end())
				return it->second;
		}
		// Fallback: look up in SQLite by lastGid column
		auto rec = HttpStateManager::Instance().FindByGid(gid);
		if (rec.has_value())
		{
			// Cache for future lookups
			std::lock_guard lock(m_mutex);
			m_gidToRecordId[gid] = rec->recordId;
			return rec->recordId;
		}
		return {};
	}

	void DownloadManager::PauseHttpDownload(std::string const& gid)
	{
		if (!IsAria2Available() || gid.empty())
			return;
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			m_aria2->Pause(gid);
		}
		catch (...)
		{
		}
	}

	void DownloadManager::ResumeHttpDownload(std::string const& gid)
	{
		if (!IsAria2Available() || gid.empty())
			return;
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			m_aria2->Resume(gid);
		}
		catch (...)
		{
		}
	}

	void DownloadManager::CancelHttpDownload(std::string const& gid)
	{
		if (!IsAria2Available() || gid.empty())
			return;
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			m_aria2->Cancel(gid);
		}
		catch (...)
		{
		}
	}

	void DownloadManager::RemoveHttpDownload(std::string const& gid)
	{
		if (!IsAria2Available() || gid.empty())
			return;
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			m_aria2->Remove(gid);
		}
		catch (...)
		{
		}
	}

	void DownloadManager::DeleteHttpDownload(
		std::string const& gid, bool const deleteDownloadedFiles)
	{
		if (gid.empty())
		{
			return;
		}
		bool const engineAvailable = IsAria2Available();
		if (!engineAvailable)
		{
			// Do not delete only the SQLite row while download.session still owns the
			// task: aria2 would restore it on the next launch. Keep the task intact and
			// let the UI report a retryable failure instead.
			throw std::runtime_error("aria2 is not available; the task was not deleted");
		}
		std::lock_guard rpcLock(m_aria2->InstanceLock());

		std::vector<std::filesystem::path> downloadedFiles;
		std::vector<std::filesystem::path> controlFiles;
		try
		{
			auto const information = m_aria2->GetTaskInformation(gid);
			for (auto const& file : information.Files)
			{
				if (file.Path.empty()) continue;
				auto path = std::filesystem::path{ winrt::to_hstring(file.Path).c_str() };
				downloadedFiles.push_back(path);
				path += L".aria2";
				controlFiles.push_back(std::move(path));
			}
			// Aria2 may have resolved a server-provided file name after the task was
			// created. Persist that actual path before forceRemove makes tellStatus
			// unavailable, so a later retry can still clean a temporarily locked file.
			if (!downloadedFiles.empty())
			{
				if (auto const record = HttpStateManager::Instance().FindByGid(gid))
				{
					auto const& path = downloadedFiles.front();
					HttpStateManager::Instance().UpdateRecordOutputPath(
						record->recordId,
						winrt::to_string(winrt::hstring{ path.parent_path().wstring() }),
						winrt::to_string(winrt::hstring{ path.filename().wstring() }));
				}
			}
		}
		catch (...) { }
		// A previous delete attempt may already have removed the aria2 result while
		// Windows still held the output file open. Keep the SQLite record until all
		// file operations succeed so a retry can reconstruct the output path.
		if (downloadedFiles.empty())
		{
			if (auto const record = HttpStateManager::Instance().FindByGid(gid))
			{
				auto const& leafName = record->fileName.empty() ? record->name : record->fileName;
				if (!record->savePath.empty() && !leafName.empty())
				{
					auto path = std::filesystem::path{
						winrt::to_hstring(record->savePath).c_str() }
						/ std::filesystem::path{ winrt::to_hstring(leafName).c_str() };
					downloadedFiles.push_back(path);
					path += L".aria2";
					controlFiles.push_back(std::move(path));
				}
			}
		}

		// NanaGet uses forceRemove for active tasks and removeDownloadResult for
		// stopped tasks. Aria2 moves a cancelled task to the stopped list
		// asynchronously, so wait briefly before removing its result.
		bool reachedStoppedList = false;
		try { m_aria2->Cancel(gid, true); } catch (...) { }
		for (int attempt = 0; attempt < 20; ++attempt)
		{
			try
			{
				auto const status = m_aria2->GetTaskInformation(gid).Status;
				if (status == Aria2::DownloadStatus::Removed
					|| status == Aria2::DownloadStatus::Complete
					|| status == Aria2::DownloadStatus::Error)
				{
					reachedStoppedList = true;
					break;
				}
			}
			catch (...)
			{
				// tellStatus fails after removeDownloadResult. Treat this as already gone.
				reachedStoppedList = true;
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		if (!reachedStoppedList)
		{
			throw std::runtime_error("aria2 did not stop the task before deletion");
		}

		bool resultRemoved = false;
		for (int attempt = 0; attempt < 20 && !resultRemoved; ++attempt)
		{
			try
			{
				m_aria2->Remove(gid);
				resultRemoved = true;
			}
			catch (...)
			{
				try
				{
					(void)m_aria2->GetTaskInformation(gid);
				}
				catch (...)
				{
					resultRemoved = true;
				}
				if (!resultRemoved)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}
		}
		if (!resultRemoved)
		{
			throw std::runtime_error("aria2 did not remove the stopped task result");
		}
		m_aria2->SaveSession();

		auto removeWithRetry = [](std::filesystem::path const& path, bool const directory)
		{
			for (int attempt = 0; attempt < 30; ++attempt)
			{
				std::error_code error;
				auto const exists = std::filesystem::exists(path, error);
				if (error)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}
				if (!exists) return;
				if (directory)
					std::filesystem::remove_all(path, error);
				else
					std::filesystem::remove(path, error);
				if (!error) return;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			throw std::filesystem::filesystem_error(
				"aria2 released the task but the download file is still locked",
				path, std::make_error_code(std::errc::device_or_resource_busy));
		};

		for (auto const& path : controlFiles)
		{
			removeWithRetry(path, false);
		}
		if (deleteDownloadedFiles)
		{
			for (auto const& path : downloadedFiles)
			{
				std::error_code error;
				auto const isDirectory = std::filesystem::is_directory(path, error);
				removeWithRetry(path, !error && isDirectory);
			}
		}

		// The refresh thread owns its own task snapshot. Forget local identity caches
		// only after engine/session/file cleanup succeeded so a failed deletion can
		// still be retried from the persisted record.
		{
			std::lock_guard lock(m_mutex);
			m_gidToRecordId.erase(gid);
			m_knownGids.erase(gid);
			m_lastHttpStatuses.erase(gid);
			m_httpTaskSnapshots.erase(gid);
			m_httpTaskLogs.erase(gid);
		}
	}

	void DownloadManager::PauseAllHttp()
	{
		if (!IsAria2Available())
			return;
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			m_aria2->PauseAll();
		}
		catch (...)
		{
		}
	}

	void DownloadManager::ResumeAllHttp()
	{
		if (!IsAria2Available())
			return;
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			m_aria2->ResumeAll();
		}
		catch (...)
		{
		}
	}

	void DownloadManager::ClearCompletedHttp()
	{
		if (!IsAria2Available())
			return;
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			m_aria2->ClearList();
		}
		catch (...)
		{
		}
	}

	// ------------------------------------------------------------------
	//  Speed accessors (thread-safe atomic reads)
	// ------------------------------------------------------------------
	std::uint64_t DownloadManager::TotalHttpDownloadSpeed() const
	{
		return m_totalDlSpeed.load();
	}

	std::uint64_t DownloadManager::TotalHttpUploadSpeed() const
	{
		return m_totalUlSpeed.load();
	}

	// ------------------------------------------------------------------
	//  Callbacks
	// ------------------------------------------------------------------
	void DownloadManager::SetHttpProgressCallback(HttpProgressCallback cb)
	{
		std::lock_guard lock(m_mutex);
		m_progressCb = std::move(cb);
	}

	void DownloadManager::SetHttpFinishedCallback(HttpFinishedCallback cb)
	{
		std::lock_guard lock(m_mutex);
		m_finishedCb = std::move(cb);
	}

	void DownloadManager::SetHttpErrorCallback(HttpErrorCallback cb)
	{
		std::lock_guard lock(m_mutex);
		m_errorCb = std::move(cb);
	}

	// ------------------------------------------------------------------
	//  Refresh thread
	// ------------------------------------------------------------------
	void DownloadManager::RefreshThreadEntry()
	{
		using clock = std::chrono::steady_clock;
		constexpr auto kInterval = 1000ms;

		while (!m_stopRefresh.load())
		{
			auto start = clock::now();

			if (IsAria2Available())
			{
				ProcessAria2Tasks();
			}

			// Sleep remainder of interval, wake immediately if stopped
			{
				std::unique_lock<std::mutex> lock(m_stopMutex);
				m_stopCv.wait_for(lock, kInterval, [this]
				{
					return m_stopRefresh.load();
				});
			}
		}
	}

	void DownloadManager::ProcessAria2Tasks()
	{
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			m_aria2->RefreshInformation();

			// Update global speed stats
			m_totalDlSpeed.store(m_aria2->TotalDownloadSpeed());
			m_totalUlSpeed.store(m_aria2->TotalUploadSpeed());

			// Get task list and fire progress callbacks
			auto gids = m_aria2->GetTaskList();

			HttpProgressCallback progressCb;
			HttpFinishedCallback finishedCb;
			HttpErrorCallback errorCb;
			std::set<std::string> knownGids;
			{
				std::lock_guard lock(m_mutex);
				progressCb = m_progressCb;
				finishedCb = m_finishedCb;
				errorCb = m_errorCb;
				knownGids = m_knownGids;
			}

			std::set<std::string> currentGids;

			for (auto const& gid : gids)
			{
				currentGids.insert(gid);

				Aria2::DownloadInformation task;
				try
				{
					task = m_aria2->GetTaskInformation(gid);
				}
				catch (...)
				{
					continue;
				}
				{
					std::lock_guard lock(m_mutex);
					m_httpTaskSnapshots[gid] = task;
				}

				if (progressCb)
				{
					HttpTaskProgress progress;
					progress.gid = gid;
					progress.name = Aria2::ToFriendlyName(task);
					progress.status = task.Status;
					progress.totalLength = task.TotalLength;
					progress.completedLength = task.CompletedLength;
					progress.downloadSpeed = task.DownloadSpeed;
					progress.uploadSpeed = task.UploadSpeed;
					progress.progressPercent = (task.TotalLength > 0)
						? static_cast<int>((task.CompletedLength * 100) / task.TotalLength)
						: 0;

					progressCb(progress);
				}

				// Update persisted record with latest info
				{
					std::string recordId;
					{
						std::lock_guard lock(m_mutex);
						auto it = m_gidToRecordId.find(gid);
						if (it != m_gidToRecordId.end()) recordId = it->second;
					}
					if (!recordId.empty())
					{
						auto& hsm = HttpStateManager::Instance();
						auto friendlyName = Aria2::ToFriendlyName(task);
						hsm.UpdateRecordName(recordId, friendlyName);
						hsm.UpdateRecordProgress(recordId, task.CompletedLength, task.TotalLength);
						int const persistedStatus = task.Status == Aria2::DownloadStatus::Paused ? 2 : task.Status == Aria2::DownloadStatus::Complete ? 3 : task.Status == Aria2::DownloadStatus::Error ? 4 : 1;
						hsm.UpdateRecordStatus(recordId, persistedStatus);
					}
				}

				std::optional<Aria2::DownloadStatus> previousStatus;
				{
					std::lock_guard lock(m_mutex);
					if (auto const previous = m_lastHttpStatuses.find(gid); previous != m_lastHttpStatuses.end()) previousStatus = previous->second;
					m_lastHttpStatuses[gid] = task.Status;
					if (previousStatus && *previousStatus != task.Status)
					{
						auto const text = task.Status == Aria2::DownloadStatus::Complete ? "Download completed." : task.Status == Aria2::DownloadStatus::Error ? "Download failed: " + task.ErrorMessage : task.Status == Aria2::DownloadStatus::Paused ? "Task paused." : task.Status == Aria2::DownloadStatus::Active ? "Task active." : "Task state changed.";
						m_httpTaskLogs[gid].push_back({ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(), text });
					}
				}

				// Fire completion / error callbacks on state transitions.
				if (task.Status == Aria2::DownloadStatus::Complete)
				{
					if (previousStatus && *previousStatus != Aria2::DownloadStatus::Complete)
					{
						// Persist completed status
						std::string recordId;
						{
							std::lock_guard lock(m_mutex);
							auto it = m_gidToRecordId.find(gid);
							if (it != m_gidToRecordId.end()) recordId = it->second;
						}
						if (!recordId.empty())
							HttpStateManager::Instance().UpdateRecordStatus(recordId, 3); // completed

						ShowHttpCompletionToast(gid, task);
						if (finishedCb) finishedCb(gid, Aria2::ToFriendlyName(task));
					}
				}
				else if (task.Status == Aria2::DownloadStatus::Error)
				{
					if (previousStatus && *previousStatus != Aria2::DownloadStatus::Error)
					{
						// Persist failed status
						std::string recordId;
						{
							std::lock_guard lock(m_mutex);
							auto it = m_gidToRecordId.find(gid);
							if (it != m_gidToRecordId.end()) recordId = it->second;
						}
						if (!recordId.empty())
							HttpStateManager::Instance().UpdateRecordStatus(recordId, 4); // failed

						if (errorCb) errorCb(gid, Aria2::ToFriendlyName(task));
					}
				}
			}

			{
				std::lock_guard lock(m_mutex);
				m_knownGids = currentGids;
			}
		}
		catch (...)
		{
			// Swallow exceptions in background thread
		}
	}

	void DownloadManager::ShowHttpCompletionToast(std::string const& gid, Aria2::DownloadInformation const& task)
	{
		std::filesystem::path outputPath;
		if (!task.Files.empty() && !task.Files.front().Path.empty()) outputPath = winrt::to_hstring(task.Files.front().Path).c_str();
		auto const now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		std::int64_t elapsed = 1;
		if (auto const record = HttpStateManager::Instance().FindByGid(gid)) elapsed = (std::max<std::int64_t>)(1, now - record->addedTimestamp);
		::OpenNet::Core::Notification::ShowHttpDownloadCompleted(Aria2::ToFriendlyName(task), outputPath, elapsed, task.CompletedLength);
	}
}
