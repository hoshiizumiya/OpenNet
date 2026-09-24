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
import OpenNet.Core.Content.ContentHasher;
import OpenNet.Core.Content.ResourceKey;
import OpenNet.Core.P2PManager;

namespace OpenNet::Core
{
	using namespace std::chrono_literals;

	namespace
	{
		constexpr auto ResourceHintEligibilityCategory =
			"http_resource_hint_eligible";
		constexpr auto ResourceValidatorKeyCategory =
			"http_resource_validator_key";

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

		std::optional<::OpenNet::Core::Content::ResourceKey>
			ParsePersistedResourceKey(std::string_view value)
		{
			auto const separator = value.find(':');
			if (separator == std::string_view::npos)
				return std::nullopt;

			int algorithm{};
			auto const algorithmText = value.substr(0, separator);
			auto const [end, error] = std::from_chars(
				algorithmText.data(),
				algorithmText.data() + algorithmText.size(),
				algorithm);
			if (error != std::errc{}
				|| end != algorithmText.data() + algorithmText.size()
				|| algorithm <= 0
				|| algorithm > 255)
				return std::nullopt;

			auto const hex = value.substr(separator + 1);
			if (hex.size() != 64) return std::nullopt;

			auto nibble = [](char value) -> int
			{
				if (value >= '0' && value <= '9') return value - '0';
				if (value >= 'a' && value <= 'f') return value - 'a' + 10;
				if (value >= 'A' && value <= 'F') return value - 'A' + 10;
				return -1;
			};

			::OpenNet::Core::Content::ResourceKey key;
			key.algorithm =
				static_cast<::OpenNet::Core::Content::ResourceKeyAlgorithm>(
					static_cast<std::uint8_t>(algorithm));
			for (std::size_t index = 0; index < key.digest.size(); ++index)
			{
				int const high = nibble(hex[index * 2]);
				int const low = nibble(hex[index * 2 + 1]);
				if (high < 0 || low < 0) return std::nullopt;
				key.digest[index] =
					static_cast<std::uint8_t>((high << 4) | low);
			}
			return key;
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

			{
				std::lock_guard lock(m_peerFallbackMutex);
				m_stopPeerFallback.store(true);
				m_peerFallbackJobs.clear();
				for (auto& [gid, state] : m_peerFallbacks)
				{
					(void)gid;
					state.cancelRequested = true;
				}
			}
			m_peerFallbackCv.notify_all();
			for (auto& worker : m_peerFallbackWorkers)
			{
				if (worker.joinable()) worker.join();
			}
			m_peerFallbackWorkers.clear();
			{
				std::lock_guard lock(m_peerFallbackMutex);
				m_peerFallbackSuppressedGids.clear();
				m_peerFallbacks.clear();
			}

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

				m_stopPeerFallback.store(false);
				m_peerFallbackWorkers.clear();
				for (int index = 0; index < 2; ++index)
				{
					m_peerFallbackWorkers.emplace_back([this]()
					{
						PeerFallbackThreadEntry();
					});
				}

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

		{
			std::lock_guard lock(m_peerFallbackMutex);
			m_stopPeerFallback.store(true);
			m_peerFallbackJobs.clear();
			for (auto& [gid, state] : m_peerFallbacks)
			{
				(void)gid;
				state.cancelRequested = true;
			}
		}
		m_peerFallbackCv.notify_all();
		for (auto& worker : m_peerFallbackWorkers)
		{
			if (worker.joinable()) worker.join();
		}
		m_peerFallbackWorkers.clear();
		{
			std::lock_guard lock(m_peerFallbackMutex);
			m_peerFallbackSuppressedGids.clear();
			m_peerFallbacks.clear();
		}

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
		std::vector<std::string> initialResourceUris = options.Uris;
		if (!options.ResourceFinalUrl.empty())
			initialResourceUris.push_back(options.ResourceFinalUrl);
		auto resourceKeys = resourceHintSafe
			? BuildResourceKeys(initialResourceUris)
			: std::vector<::OpenNet::Core::Content::ResourceKey>{};

		std::optional<::OpenNet::Core::Content::ResourceKey>
			preflightValidatorKey;
		if (resourceHintSafe
			&& !options.ResourceFinalUrl.empty()
			&& !options.ResourceStrongETag.empty()
			&& options.ResourceContentLength != 0)
		{
			preflightValidatorKey =
				::OpenNet::Core::Content::ResourceKeyFactory::FromHttpValidator({
					options.ResourceFinalUrl,
					options.ResourceStrongETag,
					options.ResourceContentLength
				});
			if (preflightValidatorKey
				&& std::ranges::find(
					resourceKeys, *preflightValidatorKey)
					== resourceKeys.end())
				resourceKeys.push_back(*preflightValidatorKey);
		}

		auto expectedSha256 = ParseExpectedSha256(options.Checksum);

		std::filesystem::path peerFallbackTarget;
		if (expectedSha256
			&& !options.Dir.empty()
			&& !options.OutFileName.empty())
		{
			peerFallbackTarget =
				std::filesystem::path{
					winrt::to_hstring(options.Dir).c_str() }
				/ std::filesystem::path{
					winrt::to_hstring(options.OutFileName).c_str() };
		}

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
				if (preflightValidatorKey)
				{
					database.SetString(
						ResourceValidatorKeyCategory,
						recordId,
						std::format(
							"{}:{}",
							static_cast<int>(
								preflightValidatorKey->algorithm),
							preflightValidatorKey->ToHex()));
				}
				else
				{
					database.Delete(
						ResourceValidatorKeyCategory,
						recordId);
				}
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
					std::move(expectedSha256),
					std::move(peerFallbackTarget),
					options.ResourceContentLength);
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
		CancelPeerFallback(gid);
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
		CancelPeerFallback(gid);
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
		CancelPeerFallback(gid);
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
		CancelPeerFallback(gid);
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
			settings.Delete(
				ResourceValidatorKeyCategory,
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
		std::optional<::OpenNet::Core::Content::ContentIdentity> expectedSha256,
		std::filesystem::path targetFilePath,
		std::uint64_t expectedSize)
	{
		if (gid.empty() || resourceKeys.empty()) return;
		{
			std::lock_guard lock(m_resourceDiscoveryMutex);
			m_resourceDiscoveryJobs.push_back({
				std::move(gid),
				std::move(resourceKeys),
				std::move(expectedSha256),
				std::move(targetFilePath),
				expectedSize
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

				::OpenNet::Core::Content::ResourceCandidate const*
					selected = nullptr;

				if (job.expectedSha256)
				{
					for (auto const& candidate : lookup->candidates)
					{
						if (job.expectedSize != 0
							&& candidate.size != job.expectedSize)
							continue;

						bool const checksumMatch =
							std::ranges::find(
								candidate.identities,
								*job.expectedSha256)
							!= candidate.identities.end();
						if (checksumMatch)
						{
							selected = &candidate;
							break;
						}
					}
				}

				if (!selected)
				{
					selected = &lookup->candidates.front();
				}

				summary.contentId = selected->contentId;
				summary.size = selected->size;
				summary.observationCount = selected->observationCount;
				summary.checksumValidated =
					job.expectedSha256
					&& (job.expectedSize == 0
						|| selected->size == job.expectedSize)
					&& std::ranges::find(
						selected->identities,
						*job.expectedSha256)
						!= selected->identities.end();

				for (auto const& identity : selected->identities)
				{
					if (identity.algorithm
						== ::OpenNet::Core::Content::ContentIdentityAlgorithm::Bep52FileRootSha256)
					{
						summary.bep52Identity = identity;
						break;
					}
				}

				if (summary.checksumValidated)
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
							"; supplied SHA-256 and known length match candidate.";
					else
						message +=
							"; hint is not authoritative, origin download remains active.";
					m_httpTaskLogs[job.gid].push_back({
						timestamp,
						std::move(message)
					});
				}
			}

			if (summary.checksumValidated
				&& summary.bep52Identity
				&& job.expectedSha256
				&& !job.targetFilePath.empty())
			{
				QueuePeerFallback(job, summary);
			}
		}

		winrt::uninit_apartment();
	}

	void DownloadManager::QueuePeerFallback(
		ResourceDiscoveryJob const& discovery,
		HttpResourceDiscovery const& summary)
	{
		if (!summary.bep52Identity
			|| !discovery.expectedSha256
			|| discovery.targetFilePath.empty())
			return;

		PeerFallbackJob job;
		job.gid = discovery.gid;
		job.sessionId = "http-fallback:" + discovery.gid;
		job.targetFilePath = discovery.targetFilePath;
		job.temporaryFilePath = discovery.targetFilePath;
		job.temporaryFilePath +=
			winrt::to_hstring(
				".opennet-p2p-" + discovery.gid + ".part").c_str();
		job.bep52Identity = *summary.bep52Identity;
		job.expectedSha256 = *discovery.expectedSha256;
		job.resourceKeys = discovery.resourceKeys;
		job.expectedSize = summary.size;

		{
			std::lock_guard lock(m_peerFallbackMutex);
			if (m_stopPeerFallback.load()
				|| m_peerFallbackSuppressedGids.contains(job.gid))
				return;
			if (auto const existing = m_peerFallbacks.find(job.gid);
				existing != m_peerFallbacks.end()
				&& existing->second.phase != PeerFallbackPhase::Failed
				&& !existing->second.cancelRequested)
				return;

			PeerFallbackState state;
			state.phase = PeerFallbackPhase::Pending;
			state.job = job;
			m_peerFallbacks.insert_or_assign(job.gid, state);
			m_peerFallbackJobs.push_back(job);
		}

		{
			std::lock_guard lock(m_mutex);
			if (auto discoveryState =
				m_httpResourceDiscoveries.find(job.gid);
				discoveryState != m_httpResourceDiscoveries.end())
			{
				discoveryState->second.peerFallbackQueued = true;
			}
			m_httpTaskLogs[job.gid].push_back({
				std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now()
						.time_since_epoch()).count(),
				"Trusted OpenNet peer fallback queued in a separate temporary file."
			});
		}
		m_peerFallbackCv.notify_one();
	}

	bool DownloadManager::HasPeerFallbackPending(
		std::string const& gid) const
	{
		std::lock_guard lock(m_peerFallbackMutex);
		auto const it = m_peerFallbacks.find(gid);
		if (it == m_peerFallbacks.end()
			|| it->second.cancelRequested)
			return false;
		return it->second.phase == PeerFallbackPhase::Pending
			|| it->second.phase == PeerFallbackPhase::Downloading
			|| it->second.phase == PeerFallbackPhase::Ready;
	}

	void DownloadManager::CancelPeerFallback(std::string const& gid)
	{
		if (gid.empty()) return;

		std::optional<PeerFallbackJob> immediateCleanup;
		{
			std::lock_guard lock(m_peerFallbackMutex);
			m_peerFallbackSuppressedGids.insert(gid);
			auto const it = m_peerFallbacks.find(gid);
			if (it == m_peerFallbacks.end())
				return;

			it->second.cancelRequested = true;
			std::erase_if(
				m_peerFallbackJobs,
				[&](PeerFallbackJob const& job)
				{
					return job.gid == gid;
				});

			if (it->second.phase == PeerFallbackPhase::Pending
				|| it->second.phase == PeerFallbackPhase::Ready
				|| it->second.phase == PeerFallbackPhase::Failed)
			{
				immediateCleanup = it->second.job;
				m_peerFallbacks.erase(it);
			}
		}

		if (immediateCleanup)
		{
			::OpenNet::Core::P2PManager::Instance()
				.CloseLongSeedSession(immediateCleanup->sessionId);
			std::error_code error;
			std::filesystem::remove(
				immediateCleanup->temporaryFilePath,
				error);
		}
		m_peerFallbackCv.notify_all();
	}

	void DownloadManager::PeerFallbackThreadEntry()
	{
		winrt::init_apartment(winrt::apartment_type::multi_threaded);

		auto fail = [this](
			PeerFallbackJob const& job,
			std::string message)
		{
			::OpenNet::Core::P2PManager::Instance()
				.CloseLongSeedSession(job.sessionId);
			std::error_code error;
			std::filesystem::remove(job.temporaryFilePath, error);

			bool shouldLog = false;
			{
				std::lock_guard lock(m_peerFallbackMutex);
				auto const it = m_peerFallbacks.find(job.gid);
				if (it != m_peerFallbacks.end())
				{
					if (it->second.cancelRequested)
					{
						m_peerFallbacks.erase(it);
					}
					else
					{
						it->second.phase = PeerFallbackPhase::Failed;
						it->second.error = message;
						shouldLog = true;
					}
				}
			}

			if (shouldLog)
			{
				std::lock_guard lock(m_mutex);
				m_httpTaskLogs[job.gid].push_back({
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now()
							.time_since_epoch()).count(),
					"OpenNet peer fallback failed: " + message
				});
			}
		};

		for (;;)
		{
			PeerFallbackJob job;
			{
				std::unique_lock lock(m_peerFallbackMutex);
				m_peerFallbackCv.wait(lock, [this]
				{
					return m_stopPeerFallback.load()
						|| !m_peerFallbackJobs.empty();
				});

				if (m_stopPeerFallback.load()
					&& m_peerFallbackJobs.empty())
					break;

				job = std::move(m_peerFallbackJobs.front());
				m_peerFallbackJobs.pop_front();

				auto const state = m_peerFallbacks.find(job.gid);
				if (state == m_peerFallbacks.end()
					|| state->second.cancelRequested)
					continue;
				state->second.phase =
					PeerFallbackPhase::Downloading;
			}

			{
				std::error_code error;
				std::filesystem::remove(
					job.temporaryFilePath,
					error);
			}

			bool started = false;
			try
			{
				started =
					::OpenNet::Core::P2PManager::Instance()
						.StartLongSeedDownloadAsync(
							job.bep52Identity,
							job.temporaryFilePath,
							20,
							job.sessionId)
						.get();
			}
			catch (std::exception const& exception)
			{
				fail(job, exception.what());
				continue;
			}
			catch (...)
			{
				fail(job, "Unable to start hidden libtorrent download.");
				continue;
			}

			if (!started)
			{
				fail(job, "No ready canonical swarm was available.");
				continue;
			}

			auto const deadline =
				std::chrono::steady_clock::now()
				+ std::chrono::minutes(30);
			bool completed = false;
			bool cancelled = false;
			std::string failure;

			while (std::chrono::steady_clock::now() < deadline)
			{
				{
					std::lock_guard lock(m_peerFallbackMutex);
					auto const state = m_peerFallbacks.find(job.gid);
					if (m_stopPeerFallback.load()
						|| state == m_peerFallbacks.end()
						|| state->second.cancelRequested)
					{
						cancelled = true;
						break;
					}
				}

				bool originCompleted = false;
				{
					std::lock_guard lock(m_mutex);
					if (auto const task =
						m_httpTaskSnapshots.find(job.gid);
						task != m_httpTaskSnapshots.end())
					{
						originCompleted =
							task->second.Status
								== Aria2::DownloadStatus::Complete;
					}
				}
				if (originCompleted)
				{
					cancelled = true;
					break;
				}

				auto status =
					::OpenNet::Core::P2PManager::Instance()
						.GetLongSeedSessionStatus(job.sessionId);

				if (!status.exists || !status.valid)
				{
					failure =
						"Hidden libtorrent session disappeared.";
					break;
				}
				if (status.hasError)
				{
					failure = status.error.empty()
						? "Hidden libtorrent session failed."
						: status.error;
					break;
				}

				{
					std::lock_guard lock(m_peerFallbackMutex);
					auto const state = m_peerFallbacks.find(job.gid);
					if (state != m_peerFallbacks.end())
					{
						state->second.progressPercent =
							status.progressPercent;
						state->second.downloadRate =
							status.downloadRate;
						state->second.completedBytes =
							status.totalWantedDone;
					}
				}

				if (status.finished)
				{
					completed = true;
					break;
				}

				std::this_thread::sleep_for(
					std::chrono::milliseconds(500));
			}

			::OpenNet::Core::P2PManager::Instance()
				.CloseLongSeedSession(job.sessionId);

			if (cancelled)
			{
				std::error_code error;
				std::filesystem::remove(
					job.temporaryFilePath,
					error);
				std::lock_guard lock(m_peerFallbackMutex);
				m_peerFallbacks.erase(job.gid);
				continue;
			}
			if (!completed)
			{
				fail(
					job,
					failure.empty()
						? "Peer fallback timed out."
						: std::move(failure));
				continue;
			}

			try
			{
				auto hashed =
					::OpenNet::Core::Content::ContentHasher::HashFile(
						job.temporaryFilePath);

				bool const sizeMatches =
					(job.expectedSize == 0
						|| hashed.size == job.expectedSize);
				bool const checksumMatches =
					std::ranges::find(
						hashed.identities,
						job.expectedSha256)
					!= hashed.identities.end();

				if (!sizeMatches || !checksumMatches)
				{
					fail(
						job,
						"Downloaded peer content failed the caller-supplied SHA-256/size check.");
					continue;
				}
			}
			catch (std::exception const& exception)
			{
				fail(job, exception.what());
				continue;
			}
			catch (...)
			{
				fail(job, "Unable to verify peer fallback file.");
				continue;
			}

			{
				std::lock_guard lock(m_peerFallbackMutex);
				auto const state = m_peerFallbacks.find(job.gid);
				if (state == m_peerFallbacks.end()
					|| state->second.cancelRequested)
				{
					std::error_code error;
					std::filesystem::remove(
						job.temporaryFilePath,
						error);
					if (state != m_peerFallbacks.end())
						m_peerFallbacks.erase(state);
					continue;
				}
				state->second.phase = PeerFallbackPhase::Ready;
				state->second.progressPercent = 100;
				state->second.completedBytes =
					static_cast<std::int64_t>(job.expectedSize);
			}

			{
				std::lock_guard lock(m_mutex);
				if (auto discovery =
					m_httpResourceDiscoveries.find(job.gid);
					discovery != m_httpResourceDiscoveries.end())
				{
					discovery->second.peerFallbackReady = true;
				}
				m_httpTaskLogs[job.gid].push_back({
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now()
							.time_since_epoch()).count(),
					"OpenNet peer fallback is fully downloaded and locally SHA-256 verified."
				});
			}
		}

		winrt::uninit_apartment();
	}

	bool DownloadManager::TryPromotePeerFallback(
		std::string const& gid,
		Aria2::DownloadInformation const& task,
		std::string const& recordId)
	{
		PeerFallbackJob job;
		{
			std::lock_guard lock(m_peerFallbackMutex);
			auto const it = m_peerFallbacks.find(gid);
			if (it == m_peerFallbacks.end()
				|| it->second.cancelRequested
				|| it->second.phase != PeerFallbackPhase::Ready)
				return false;
			job = it->second.job;
		}

		std::error_code existsError;
		if (!std::filesystem::is_regular_file(
			job.temporaryFilePath,
			existsError)
			|| existsError)
		{
			std::lock_guard lock(m_peerFallbackMutex);
			if (auto const it = m_peerFallbacks.find(gid);
				it != m_peerFallbacks.end())
			{
				it->second.phase = PeerFallbackPhase::Failed;
				it->second.error =
					"Verified peer fallback file disappeared before promotion.";
			}
			return false;
		}

		if (!::MoveFileExW(
			job.temporaryFilePath.c_str(),
			job.targetFilePath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			auto const error = ::GetLastError();
			// aria2 may need one more refresh tick to release the failed
			// output handle. Keep the verified fallback intact and retry.
			if (error == ERROR_SHARING_VIOLATION
				|| error == ERROR_LOCK_VIOLATION
				|| error == ERROR_ACCESS_DENIED)
				return false;

			std::lock_guard lock(m_peerFallbackMutex);
			if (auto const it = m_peerFallbacks.find(gid);
				it != m_peerFallbacks.end())
			{
				it->second.phase = PeerFallbackPhase::Failed;
				it->second.error = std::system_category()
					.message(static_cast<int>(error));
			}
			return false;
		}

		auto aria2ControlPath = job.targetFilePath;
		aria2ControlPath += L".aria2";
		{
			std::error_code error;
			std::filesystem::remove(aria2ControlPath, error);
		}

		// The task is already in aria2's stopped/error list at this point.
		// Removing the result prevents the old failed origin from being
		// restored from the saved aria2 session.
		try
		{
			m_aria2->Remove(gid);
			m_aria2->SaveSession();
		}
		catch (...)
		{
		}

		auto const finalSize = std::filesystem::file_size(
			job.targetFilePath,
			existsError);
		auto const completedSize = existsError
			? job.expectedSize
			: finalSize;

		if (!recordId.empty())
		{
			auto& stateManager = HttpStateManager::Instance();
			stateManager.UpdateRecordOutputPath(
				recordId,
				winrt::to_string(winrt::hstring{
					job.targetFilePath.parent_path().wstring() }),
				winrt::to_string(winrt::hstring{
					job.targetFilePath.filename().wstring() }));
			stateManager.UpdateRecordProgress(
				recordId,
				static_cast<std::int64_t>(completedSize),
				static_cast<std::int64_t>(completedSize));
			stateManager.UpdateRecordStatus(recordId, 3);
		}

		Aria2::DownloadInformation completedTask = task;
		completedTask.Status = Aria2::DownloadStatus::Complete;
		completedTask.TotalLength =
			static_cast<std::size_t>(completedSize);
		completedTask.CompletedLength =
			static_cast<std::size_t>(completedSize);
		completedTask.DownloadSpeed = 0;
		completedTask.ErrorCode = 0;
		completedTask.ErrorMessage.clear();
		if (completedTask.Files.empty())
		{
			Aria2::FileInformation file;
			file.Index = 1;
			file.Path = winrt::to_string(
				winrt::hstring{ job.targetFilePath.wstring() });
			file.Length =
				static_cast<std::size_t>(completedSize);
			file.CompletedLength = file.Length;
			completedTask.Files.push_back(std::move(file));
		}
		else
		{
			completedTask.Files.front().Path = winrt::to_string(
				winrt::hstring{ job.targetFilePath.wstring() });
			completedTask.Files.front().Length =
				static_cast<std::size_t>(completedSize);
			completedTask.Files.front().CompletedLength =
				static_cast<std::size_t>(completedSize);
		}

		HttpFinishedCallback finishedCallback;
		{
			std::lock_guard lock(m_mutex);
			m_httpTaskSnapshots.insert_or_assign(
				gid, completedTask);
			m_lastHttpStatuses.insert_or_assign(
				gid, Aria2::DownloadStatus::Complete);
			m_httpTaskLogs[gid].push_back({
				std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now()
						.time_since_epoch()).count(),
				"HTTP origin failed; verified OpenNet peer fallback was promoted atomically."
			});
			finishedCallback = m_finishedCb;
		}

		{
			std::lock_guard lock(m_peerFallbackMutex);
			m_peerFallbacks.erase(gid);
		}

		::OpenNet::Core::Content::ContentCatalogService::Instance()
			.EnqueueFile(
				job.targetFilePath,
				{
					::OpenNet::Core::Content::ContentSourceKind::Http,
					recordId.empty() ? gid : recordId,
					std::nullopt
				},
				{},
				job.resourceKeys);

		ShowHttpCompletionToast(gid, completedTask);
		if (finishedCallback)
			finishedCallback(
				gid,
				Aria2::ToFriendlyName(completedTask));
		return true;
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

				bool const peerFallbackHoldingError =
					task.Status == Aria2::DownloadStatus::Error
					&& HasPeerFallbackPending(gid);

				if (progressCb)
				{
					HttpTaskProgress progress;
					progress.gid = gid;
					progress.name = Aria2::ToFriendlyName(task);
					progress.status = peerFallbackHoldingError
						? Aria2::DownloadStatus::Waiting
						: task.Status;
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
						if (!peerFallbackHoldingError)
						{
							int const persistedStatus =
								task.Status == Aria2::DownloadStatus::Paused ? 2
								: task.Status == Aria2::DownloadStatus::Complete ? 3
								: task.Status == Aria2::DownloadStatus::Error ? 4
								: 1;
							hsm.UpdateRecordStatus(recordId, persistedStatus);
						}
					}
				}

				std::optional<Aria2::DownloadStatus> previousStatus;
				{
					std::lock_guard lock(m_mutex);
					if (auto const previous = m_lastHttpStatuses.find(gid);
						previous != m_lastHttpStatuses.end())
						previousStatus = previous->second;

					if (!peerFallbackHoldingError)
					{
						m_lastHttpStatuses[gid] = task.Status;
						if (previousStatus
							&& *previousStatus != task.Status)
						{
							auto const text =
								task.Status == Aria2::DownloadStatus::Complete
									? "Download completed."
								: task.Status == Aria2::DownloadStatus::Error
									? "Download failed: " + task.ErrorMessage
								: task.Status == Aria2::DownloadStatus::Paused
									? "Task paused."
								: task.Status == Aria2::DownloadStatus::Active
									? "Task active."
								: "Task state changed.";
							m_httpTaskLogs[gid].push_back({
								std::chrono::duration_cast<std::chrono::seconds>(
									std::chrono::system_clock::now()
										.time_since_epoch()).count(),
								text
							});
						}
					}
				}

				// Fire completion / error callbacks on state transitions.
				if (task.Status == Aria2::DownloadStatus::Complete)
				{
					if (previousStatus && *previousStatus != Aria2::DownloadStatus::Complete)
					{
						CancelPeerFallback(gid);
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

							if (auto persisted =
								settings.GetString(
									ResourceValidatorKeyCategory,
									recordId))
							{
								if (auto validatorKey =
									ParsePersistedResourceKey(*persisted);
									validatorKey
									&& std::ranges::find(
										resourceKeys, *validatorKey)
										== resourceKeys.end())
								{
									resourceKeys.push_back(
										*validatorKey);
								}
							}
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
					if (peerFallbackHoldingError)
					{
						if (TryPromotePeerFallback(
							gid,
							task,
							recordId))
						{
							continue;
						}

						// Keep the persisted task out of Failed while a
						// trusted peer fallback is still downloading or is
						// waiting for aria2 to release the target handle.
						continue;
					}

					if (previousStatus
						&& *previousStatus
							!= Aria2::DownloadStatus::Error)
					{
						// Do not suppress an in-flight ResourceKey lookup here.
						// A very fast origin failure may happen before discovery
						// returns; a later caller-SHA256-validated candidate is
						// still allowed to recover this failed origin task.
						{
							std::lock_guard fallbackLock(
								m_peerFallbackMutex);
							if (auto const failed =
								m_peerFallbacks.find(gid);
								failed != m_peerFallbacks.end()
								&& failed->second.phase
									== PeerFallbackPhase::Failed)
							{
								m_peerFallbacks.erase(failed);
							}
						}
						if (!recordId.empty())
							HttpStateManager::Instance()
								.UpdateRecordStatus(recordId, 4);

						if (errorCb)
							errorCb(
								gid,
								Aria2::ToFriendlyName(task));
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
