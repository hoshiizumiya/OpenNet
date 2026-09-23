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
import OpenNet.Core.Content.ContentCatalogService;
import OpenNet.Core.Content.ContentDirectoryClient;
import OpenNet.Core.Content.ResourceKey;

namespace OpenNet::Core
{
	using namespace std::chrono_literals;

	namespace
	{
		constexpr auto ResourceHintEligibilityCategory =
			"http_resource_hint_eligible";

		bool IsResourceHintSafe(
			Aria2::HttpDownloadOptions const& options)
		{
			// Any caller-controlled request context can change the bytes
			// returned by the same URL. V1 resource hints therefore only
			// share plain public requests.
			return options.Cookie.empty()
				&& options.Username.empty()
				&& options.Password.empty()
				&& options.Headers.empty()
				&& options.Referer.empty()
				&& options.UserAgent.empty();
		}

		std::optional<::OpenNet::Core::Content::ContentIdentity>
			ParseExpectedSha256(std::string checksum)
		{
			auto const equals = checksum.find('=');
			if (equals == std::string::npos) return std::nullopt;

			auto algorithm = checksum.substr(0, equals);
			std::ranges::transform(
				algorithm,
				algorithm.begin(),
				[](unsigned char value)
				{
					return static_cast<char>(std::tolower(value));
				});
			if (algorithm != "sha-256" && algorithm != "sha256")
				return std::nullopt;

			auto hex = checksum.substr(equals + 1);
			std::erase_if(hex, [](unsigned char value)
			{
				return std::isspace(value) != 0;
			});
			if (hex.size() != 64) return std::nullopt;

			auto valueOf = [](char value) -> int
			{
				if (value >= '0' && value <= '9') return value - '0';
				if (value >= 'a' && value <= 'f') return value - 'a' + 10;
				if (value >= 'A' && value <= 'F') return value - 'A' + 10;
				return -1;
			};

			::OpenNet::Core::Content::ContentIdentity identity;
			identity.algorithm =
				::OpenNet::Core::Content::ContentIdentityAlgorithm::WholeFileSha256;
			identity.digest.reserve(32);
			for (std::size_t index = 0; index < hex.size(); index += 2)
			{
				int const high = valueOf(hex[index]);
				int const low = valueOf(hex[index + 1]);
				if (high < 0 || low < 0) return std::nullopt;
				identity.digest.push_back(
					static_cast<std::uint8_t>((high << 4) | low));
			}
			return identity;
		}

		std::vector<::OpenNet::Core::Content::ResourceKey>
			BuildResourceKeys(std::ranges::input_range auto const& uris)
		{
			std::vector<::OpenNet::Core::Content::ResourceKey> keys;
			for (auto const& uri : uris)
			{
				auto key =
					::OpenNet::Core::Content::ResourceKeyFactory::FromHttpUrl(uri);
				if (key
					&& std::ranges::find(keys, *key) == keys.end())
					keys.push_back(*key);
			}
			return keys;
		}
	}

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

			{
				std::lock_guard lock(m_resourceDiscoveryMutex);
				m_stopResourceDiscovery.store(true);
				m_resourceDiscoveryJobs.clear();
			}
			m_resourceDiscoveryCv.notify_all();
			if (m_resourceDiscoveryThread.joinable())
				m_resourceDiscoveryThread.join();

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

				// Start periodic refresh and resource-discovery workers.
				m_stopRefresh.store(false);
				m_refreshThread = std::thread([this]()
				{
					RefreshThreadEntry();
				});
				m_stopResourceDiscovery.store(false);
				m_resourceDiscoveryThread = std::thread([this]()
				{
					ResourceDiscoveryThreadEntry();
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

		{
			std::lock_guard lock(m_resourceDiscoveryMutex);
			m_stopResourceDiscovery.store(true);
			m_resourceDiscoveryJobs.clear();
		}
		m_resourceDiscoveryCv.notify_all();
		if (m_resourceDiscoveryThread.joinable())
			m_resourceDiscoveryThread.join();

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

		bool const resourceHintSafe = IsResourceHintSafe(options);
		auto resourceKeys = resourceHintSafe
			? BuildResourceKeys(options.Uris)
			: std::vector<::OpenNet::Core::Content::ResourceKey>{};
		auto expectedSha256 = ParseExpectedSha256(options.Checksum);

		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			auto& stateManager = HttpStateManager::Instance();
			stateManager.Initialize();
			if (auto const existing = stateManager.FindActiveByUrl(options.Uris.front());
				existing && !existing->lastGid.empty())
			{
				try
				{
					auto const information = m_aria2->GetTaskInformation(existing->lastGid);
					if (information.Status != Aria2::DownloadStatus::Removed
						&& information.Status != Aria2::DownloadStatus::Error)
					{
						return existing->lastGid;
					}
				}
				catch (...)
				{
				}
			}
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
				auto recordId = stateManager.AddRecord(effectiveOptions.Uris.front(), effectiveOptions.Dir, effectiveOptions.OutFileName);
				stateManager.UpdateRecordGid(recordId, gid);
				database.SetInt(
					ResourceHintEligibilityCategory,
					recordId,
					resourceHintSafe && !resourceKeys.empty() ? 1 : 0);
				{
					std::lock_guard lock(m_mutex);
					m_gidToRecordId[gid] = recordId;
					m_lastHttpStatuses[gid] = effectiveOptions.StartPaused ? Aria2::DownloadStatus::Paused : Aria2::DownloadStatus::Waiting;
					m_httpTaskLogs[gid].push_back({ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(), effectiveOptions.StartPaused ? "Task added in paused state." : "Download task started." });
					if (!effectiveOptions.Description.empty()) m_httpTaskLogs[gid].push_back({ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(), "Description: " + effectiveOptions.Description });
				}
			}

			if (!gid.empty() && !resourceKeys.empty())
			{
				QueueResourceDiscovery(
					gid,
					std::move(resourceKeys),
					std::move(expectedSha256));
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
		std::lock_guard lock(m_mutex);
		if (auto const snapshot = m_httpTaskSnapshots.find(gid); snapshot != m_httpTaskSnapshots.end()) return snapshot->second;
		return std::nullopt;
	}

	std::vector<Aria2::ServersInformation> DownloadManager::GetHttpTaskServers(std::string const& gid)
	{
		if (!IsAria2Available() || gid.empty()) return {};
		std::vector<Aria2::ServersInformation> cached;
		{
			std::lock_guard lock(m_mutex);
			if (auto const entry = m_httpServerSnapshots.find(gid);
				entry != m_httpServerSnapshots.end())
			{
				cached = entry->second;
			}
			if (auto const task = m_httpTaskSnapshots.find(gid);
				task != m_httpTaskSnapshots.end()
				&& task->second.Status != Aria2::DownloadStatus::Active)
			{
				return cached;
			}
		}
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			auto servers = m_aria2->GetTaskServers(gid);
			if (!servers.empty())
			{
				std::lock_guard lock(m_mutex);
				m_httpServerSnapshots.insert_or_assign(gid, servers);
			}
			return servers.empty() ? cached : servers;
		}
		catch (...)
		{
			return {};
		}
	}

	std::vector<HttpTaskLogEntry> DownloadManager::GetHttpTaskLog(std::string const& gid) const
	{
		std::lock_guard lock(m_mutex);
		if (auto const entries = m_httpTaskLogs.find(gid); entries != m_httpTaskLogs.end()) return entries->second;
		return {};
	}

	std::optional<HttpResourceDiscovery>
		DownloadManager::GetHttpResourceDiscovery(
			std::string const& gid) const
	{
		std::lock_guard lock(m_mutex);
		if (auto const entry = m_httpResourceDiscoveries.find(gid);
			entry != m_httpResourceDiscoveries.end())
			return entry->second;
		return std::nullopt;
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

		std::string resourceRecordId;
		if (auto const record = HttpStateManager::Instance().FindByGid(gid))
			resourceRecordId = record->recordId;

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
		catch (...)
		{
		}
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
		try
		{
			m_aria2->Cancel(gid, true);
		}
		catch (...)
		{
		}
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
			m_httpResourceDiscoveries.erase(gid);
		}

		if (!resourceRecordId.empty())
		{
			auto& settings = AppSettingsDatabase::Instance();
			settings.Initialize();
			settings.Delete(
				ResourceHintEligibilityCategory,
				resourceRecordId);
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

	void DownloadManager::QueueResourceDiscovery(
		std::string gid,
		std::vector<::OpenNet::Core::Content::ResourceKey> resourceKeys,
		std::optional<::OpenNet::Core::Content::ContentIdentity> expectedSha256)
	{
		if (gid.empty() || resourceKeys.empty()) return;
		{
			std::lock_guard lock(m_resourceDiscoveryMutex);
			m_resourceDiscoveryJobs.push_back({
				std::move(gid),
				std::move(resourceKeys),
				std::move(expectedSha256)
			});
		}
		m_resourceDiscoveryCv.notify_one();
	}

	void DownloadManager::ResourceDiscoveryThreadEntry()
	{
		winrt::init_apartment(winrt::apartment_type::multi_threaded);
		::OpenNet::Core::Content::ContentDirectoryClient client;

		for (;;)
		{
			ResourceDiscoveryJob job;
			{
				std::unique_lock lock(m_resourceDiscoveryMutex);
				m_resourceDiscoveryCv.wait(lock, [this]
				{
					return m_stopResourceDiscovery.load()
						|| !m_resourceDiscoveryJobs.empty();
				});
				if (m_stopResourceDiscovery.load()
					&& m_resourceDiscoveryJobs.empty())
					break;

				job = std::move(m_resourceDiscoveryJobs.front());
				m_resourceDiscoveryJobs.pop_front();
			}

			HttpResourceDiscovery summary;
			summary.completed = true;

			for (auto const& key : job.resourceKeys)
			{
				auto lookup = client.LookupResource(key, 8);
				if (!lookup || lookup->candidates.empty())
					continue;

				auto const& candidate = lookup->candidates.front();
				summary.contentId = candidate.contentId;
				summary.size = candidate.size;
				summary.observationCount = candidate.observationCount;

				for (auto const& identity : candidate.identities)
				{
					if (identity.algorithm
						== ::OpenNet::Core::Content::ContentIdentityAlgorithm::Bep52FileRootSha256)
						summary.bep52Identity = identity;

					if (job.expectedSha256
						&& identity == *job.expectedSha256)
						summary.checksumValidated = true;
				}

				break;
			}

			{
				std::lock_guard lock(m_mutex);
				m_httpResourceDiscoveries.insert_or_assign(
					job.gid, summary);

				auto const timestamp =
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now()
							.time_since_epoch()).count();

				if (!summary.contentId.empty())
				{
					std::string message =
						"OpenNet resource hint found: "
						+ std::to_string(summary.observationCount)
						+ " observation(s)";
					if (summary.checksumValidated)
						message +=
							"; supplied SHA-256 matches candidate.";
					else
						message +=
							"; hint is not authoritative, origin download remains active.";
					m_httpTaskLogs[job.gid].push_back({
						timestamp,
						std::move(message)
					});
				}
			}
		}

		winrt::uninit_apartment();
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
			auto const now = std::chrono::steady_clock::now();
			auto const includeStopped =
				now - m_lastStoppedListRefresh >= std::chrono::seconds(10);
			auto gids = m_aria2->GetTaskList(includeStopped);
			if (includeStopped) m_lastStoppedListRefresh = now;

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
				std::optional<Aria2::DownloadInformation> previousSnapshot;
				{
					std::lock_guard lock(m_mutex);
					if (auto const previous = m_httpTaskSnapshots.find(gid);
						previous != m_httpTaskSnapshots.end())
					{
						previousSnapshot = previous->second;
					}
				}
				try
				{
					auto const includeStaticDetails = !previousSnapshot
						|| previousSnapshot->Files.empty();
					task = m_aria2->GetTaskInformation(gid, includeStaticDetails);
					if (!includeStaticDetails && previousSnapshot)
					{
						task.InfoHash = previousSnapshot->InfoHash;
						task.FollowedBy = previousSnapshot->FollowedBy;
						task.Following = previousSnapshot->Following;
						task.BelongsTo = previousSnapshot->BelongsTo;
						task.Dir = previousSnapshot->Dir;
						task.Files = previousSnapshot->Files;
						task.BitTorrent = previousSnapshot->BitTorrent;
					}
				}
				catch (...)
				{
					continue;
				}
				{
					std::lock_guard lock(m_mutex);
					m_httpTaskSnapshots[gid] = task;
				}

				std::string recordId;
				{
					std::lock_guard lock(m_mutex);
					if (auto const entry = m_gidToRecordId.find(gid); entry != m_gidToRecordId.end())
						recordId = entry->second;
				}
				if (recordId.empty())
				{
					std::string sourceUrl;
					for (auto const& file : task.Files)
					{
						if (!file.Uris.empty() && !file.Uris.front().Uri.empty())
						{
							sourceUrl = file.Uris.front().Uri;
							break;
						}
					}
					if (!sourceUrl.empty())
					{
						auto& stateManager = HttpStateManager::Instance();
						auto record = stateManager.FindActiveByUrl(sourceUrl);
						if (record)
						{
							recordId = record->recordId;
							if (record->lastGid.empty() || task.CompletedLength >= static_cast<std::size_t>((std::max)(std::int64_t{}, record->completedSize)))
								stateManager.UpdateRecordGid(recordId, gid);
						}
						else
						{
							std::string outputName;
							if (!task.Files.empty() && !task.Files.front().Path.empty())
								outputName = std::filesystem::path{ task.Files.front().Path }.filename().string();
							recordId = stateManager.AddRecord(sourceUrl, task.Dir, outputName);
							stateManager.UpdateRecordGid(recordId, gid);
						}
						if (!recordId.empty())
						{
							std::lock_guard lock(m_mutex);
							m_gidToRecordId[gid] = recordId;
						}
					}
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

						std::vector<::OpenNet::Core::Content::ResourceKey>
							resourceKeys;
						auto& settings =
							::OpenNet::Core::AppSettingsDatabase::Instance();
						bool const resourceHintEligible =
							!recordId.empty()
							&& settings.GetInt(
								ResourceHintEligibilityCategory,
								recordId,
								0) != 0;

						if (resourceHintEligible)
						{
							std::vector<std::string> uris;
							if (auto record =
								HttpStateManager::Instance()
									.FindByRecordId(recordId))
							{
								if (!record->url.empty())
									uris.push_back(record->url);
							}

							for (auto const& file : task.Files)
							{
								for (auto const& uri : file.Uris)
								{
									if (!uri.Uri.empty())
										uris.push_back(uri.Uri);
								}
							}

							try
							{
								auto const servers =
									m_aria2->GetTaskServers(gid);
								for (auto const& group : servers)
								{
									for (auto const& server : group.Servers)
									{
										if (!server.Uri.empty())
											uris.push_back(server.Uri);
										if (!server.CurrentUri.empty())
											uris.push_back(server.CurrentUri);
									}
								}
							}
							catch (...)
							{
							}

							resourceKeys = BuildResourceKeys(uris);
						}

						for (auto const& file : task.Files)
						{
							if (file.Path.empty()) continue;
							::OpenNet::Core::Content::ContentCatalogService::Instance().EnqueueFile(
								std::filesystem::path{ winrt::to_hstring(file.Path).c_str() },
								{
									::OpenNet::Core::Content::ContentSourceKind::Http,
									recordId.empty() ? gid : recordId,
									std::nullopt
								},
								{},
								resourceKeys);
						}

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
