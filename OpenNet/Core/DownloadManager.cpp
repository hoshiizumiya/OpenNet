/*
 * PROJECT:   OpenNet
 * FILE:      Core/DownloadManager.cpp
 * PURPOSE:   Unified download manager – aria2 HTTP engine integration
 *
 * LICENSE:   The MIT License
 */

#include "pch.h"
#include "Core/DownloadManager.h"

#include <chrono>
#include <sstream>

using namespace std::chrono_literals;

namespace OpenNet::Core
{
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
		catch (...) {}
	}

	// ------------------------------------------------------------------
	//  Lifecycle
	// ------------------------------------------------------------------
	winrt::Windows::Foundation::IAsyncAction DownloadManager::InitializeAsync()
	{
		co_await winrt::resume_background();

		std::lock_guard lock(m_mutex);
		if (m_initialized)
			co_return;

		// Create and start the local Aria2 instance
		// LocalAria2Instance::constructor calls Startup() internally
		m_aria2 = std::make_unique<Aria2::LocalAria2Instance>();

		// Initialize HTTP download record persistence
		HttpStateManager::Instance().Initialize();

		// Rebuild GID→recordId mapping from persisted records so that
		// GetRecordIdForGid() works correctly after an app restart.
		{
			auto records = HttpStateManager::Instance().LoadAllRecords();
			for (auto const& rec : records)
			{
				if (!rec.lastGid.empty())
				{
					m_gidToRecordId[rec.lastGid] = rec.recordId;
				}
			}
		}

		// Start periodic refresh thread
		m_stopRefresh.store(false);
		m_refreshThread = std::thread([this]()
		{
			RefreshThreadEntry();
		});

		m_initialized = true;
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
		if (!IsAria2Available())
			return {};

		try
		{
			std::string gid;
			if (dir.empty() && fileName.empty())
			{
				gid = m_aria2->AddUri(url);
			}
			else
			{
				gid = m_aria2->AddUriWithOptions(url, dir, fileName);
			}

			// Persist the download record
			if (!gid.empty())
			{
				auto recordId = HttpStateManager::Instance().AddRecord(url, dir, fileName);
				HttpStateManager::Instance().UpdateRecordGid(recordId, gid);
				{
					std::lock_guard lock(m_mutex);
					m_gidToRecordId[gid] = recordId;
				}
			}

			return gid;
		}
		catch (...)
		{
			return {};
		}
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
			m_aria2->Remove(gid);
		}
		catch (...)
		{
		}
	}

	void DownloadManager::PauseAllHttp()
	{
		if (!IsAria2Available())
			return;
		try
		{
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
				m_stopCv.wait_for(lock, kInterval, [this] { return m_stopRefresh.load(); });
			}
		}
	}

	void DownloadManager::ProcessAria2Tasks()
	{
		try
		{
			m_aria2->RefreshInformation();

			// Update global speed stats
			m_totalDlSpeed.store(m_aria2->TotalDownloadSpeed());
			m_totalUlSpeed.store(m_aria2->TotalUploadSpeed());

			// Get task list and fire progress callbacks
			auto gids = m_aria2->GetTaskList();

			HttpProgressCallback progressCb;
			HttpFinishedCallback finishedCb;
			HttpErrorCallback errorCb;
			{
				std::lock_guard lock(m_mutex);
				progressCb = m_progressCb;
				finishedCb = m_finishedCb;
				errorCb = m_errorCb;
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
					}
				}

				// Fire completion / error callbacks
				if (task.Status == Aria2::DownloadStatus::Complete)
				{
					if (m_knownGids.count(gid) == 0)
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

						if (finishedCb) finishedCb(gid, Aria2::ToFriendlyName(task));
					}
				}
				else if (task.Status == Aria2::DownloadStatus::Error)
				{
					if (m_knownGids.count(gid) == 0)
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
}
