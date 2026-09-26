/*
 * PROJECT:   OpenNet
 * FILE:      Core/DownloadManager.cpp
 * PURPOSE:   Unified download manager – aria2 HTTP engine integration
 *
 * LICENSE:   The MIT License
 */
module;

#include "Core/Notification/HttpToastNotification.h"
#include <Windows.h>

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
		constexpr auto ExpectedSha256Category =
			"http_expected_sha256";
		constexpr auto CanonicalWebSeedUrlCategory =
			"http_canonical_webseed_url";

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

		bool IsDirectFileWebSeedUrl(std::string_view const url)
		{
			auto const scheme = url.find("://");
			if (scheme == std::string_view::npos)
				return false;
			auto const pathStart = url.find('/', scheme + 3);
			if (pathStart == std::string_view::npos)
				return false;
			auto const pathEnd = url.find_first_of("?#", pathStart);
			auto const path = url.substr(
				pathStart,
				pathEnd == std::string_view::npos
					? std::string_view::npos
					: pathEnd - pathStart);
			return !path.empty() && !path.ends_with('/');
		}

		std::vector<std::string> BuildHybridWebSeeds(
			Aria2::HttpDownloadOptions const& options)
		{
			std::vector<std::string> result;
			if (!options.ResourceSupportsByteRanges
				|| options.ResourceFinalUrl.empty()
				|| !IsDirectFileWebSeedUrl(options.ResourceFinalUrl))
				return result;

			// Reuse the same public-resource policy as ResourceKey discovery.
			// If the final URL is unsafe for sharing, do not hand it to the
			// canonical web-seed path either.
			if (::OpenNet::Core::Content::ResourceKeyFactory::FromHttpUrl(
				options.ResourceFinalUrl))
			{
				result.push_back(options.ResourceFinalUrl);
			}
			return result;
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

		bool RemoveAria2TaskUnlocked(
			Aria2::LocalAria2Instance& aria2,
			std::string const& gid)
		{
			try
			{
				aria2.Cancel(gid, true);
			}
			catch (...)
			{
			}

			bool stopped = false;
			for (int attempt = 0; attempt < 20; ++attempt)
			{
				try
				{
					auto const status =
						aria2.GetTaskInformation(gid).Status;
					if (status == Aria2::DownloadStatus::Removed
						|| status == Aria2::DownloadStatus::Complete
						|| status == Aria2::DownloadStatus::Error)
					{
						stopped = true;
						break;
					}
				}
				catch (...)
				{
					// tellStatus fails once the result is already gone.
					return true;
				}
				std::this_thread::sleep_for(
					std::chrono::milliseconds(50));
			}
			if (!stopped)
				return false;

			for (int attempt = 0; attempt < 20; ++attempt)
			{
				try
				{
					aria2.Remove(gid);
					aria2.SaveSession();
					return true;
				}
				catch (...)
				{
					try
					{
						(void)aria2.GetTaskInformation(gid);
					}
					catch (...)
					{
						return true;
					}
				}
				std::this_thread::sleep_for(
					std::chrono::milliseconds(50));
			}
			return false;
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

			m_resourceDiscoveryStopSource.request_stop();
			{
				std::lock_guard lock(m_resourceDiscoveryMutex);
				m_stopResourceDiscovery.store(true);
				m_resourceDiscoveryJobs.clear();
			}
			m_resourceDiscoveryCv.notify_all();
			if (m_resourceDiscoveryThread.joinable())
				m_resourceDiscoveryThread.join();

			m_canonicalHybridStopSource.request_stop();
			{
				std::lock_guard lock(m_canonicalHybridMutex);
				m_stopCanonicalHybrid.store(true);
				m_canonicalHybridJobs.clear();
				for (auto& [gid, state] : m_canonicalHybrids)
				{
					(void)gid;
					state.cancelRequested = true;
				}
			}
			m_canonicalHybridCv.notify_all();
			for (auto& worker : m_canonicalHybridWorkers)
			{
				if (worker.joinable()) worker.join();
			}
			m_canonicalHybridWorkers.clear();
			{
				std::lock_guard lock(m_canonicalHybridMutex);
				m_canonicalHybridSuppressedGids.clear();
				m_hybridProbeGids.clear();
				m_userPausedHttpGids.clear();
				m_canonicalHybrids.clear();
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

			// Older prototypes used a second whole-file peer fallback at
			// <target>.opennet-p2p-<gid>.part. The current architecture has no
			// producer or consumer for that format: canonical HTTP/P2P always
			// uses one libtorrent writer at the final target. Remove only the
			// exact legacy temp path derivable from a persisted record/GID.
			for (auto const& rec : records)
			{
				if (rec.lastGid.empty()
					|| rec.savePath.empty()
					|| rec.fileName.empty())
				{
					continue;
				}

				auto legacyFallbackPath =
					std::filesystem::path{
						winrt::to_hstring(rec.savePath).c_str() }
					/ std::filesystem::path{
						winrt::to_hstring(rec.fileName).c_str() };
				legacyFallbackPath +=
					winrt::to_hstring(
						".opennet-p2p-" + rec.lastGid + ".part").c_str();

				std::error_code error;
				auto const existed =
					std::filesystem::exists(
						legacyFallbackPath,
						error);
				if (!error && existed)
				{
					std::filesystem::remove(
						legacyFallbackPath,
						error);
					if (error)
					{
						OutputDebugStringW((
							L"DownloadManager: failed to remove legacy peer fallback temp: "
							+ legacyFallbackPath.wstring()
							+ L"\n").c_str());
					}
				}
			}

			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_aria2 = std::move(aria2);
				for (auto const& rec : records)
				{
					if (!rec.lastGid.empty())
						m_gidToRecordId[rec.lastGid] = rec.recordId;
				}
			}

			// Hidden canonical sessions are process-local. Recover any persisted
			// ownership before refresh callbacks can observe the aria2 shell.
			auto& recoverySettings = AppSettingsDatabase::Instance();
			recoverySettings.Initialize();
			for (auto const& rec : records)
			{
				bool const completedCanonicalRecord =
					rec.status == 3
					&& rec.transferMode == static_cast<int>(
						Aria2::HttpTransferMode::P2PPreferred)
					&& rec.activeEngine == static_cast<int>(
						HttpTransferEngine::CanonicalHybrid);

				if (rec.status == 4
					|| rec.transferMode != static_cast<int>(
						Aria2::HttpTransferMode::P2PPreferred)
					|| rec.activeEngine == static_cast<int>(
						HttpTransferEngine::Aria2)
					|| (rec.status < 3 && rec.lastGid.empty())
					|| (rec.status >= 3 && !completedCanonicalRecord))
				{
					continue;
				}

				// A probe never gave payload ownership to libtorrent, so its
				// paused shell can immediately become the aria2 fallback.
				if (rec.activeEngine == static_cast<int>(
					HttpTransferEngine::CanonicalProbe))
				{
					HttpStateManager::Instance().UpdateRecordActiveEngine(
						rec.recordId,
						static_cast<int>(HttpTransferEngine::Aria2));
					if (!rec.userRequestedPaused)
					{
						try
						{
							std::lock_guard rpcLock(m_aria2->InstanceLock());
							m_aria2->Resume(rec.lastGid);
						}
						catch (...)
						{
						}
					}
					HttpStateManager::Instance().UpdateRecordStatus(
						rec.recordId,
						rec.userRequestedPaused ? 2 : 1);
					continue;
				}

				if (rec.activeEngine != static_cast<int>(
					HttpTransferEngine::CanonicalHybrid)
					|| rec.savePath.empty()
					|| rec.fileName.empty())
					continue;

				auto const targetFilePath =
					std::filesystem::path{
						winrt::to_hstring(rec.savePath).c_str() }
					/ std::filesystem::path{
						winrt::to_hstring(rec.fileName).c_str() };

				// A completed canonical HTTP record may have crashed after
				// EnqueueFile() returned but before the catalog worker committed
				// content_location. Keep replaying that idempotent transition on
				// later starts until the durable catalog really contains it.
				if (completedCanonicalRecord)
				{
					if (::OpenNet::Core::Content::ContentCatalogService::Instance()
						.FindByLocation(targetFilePath))
					{
						continue;
					}
				}

				bool recoveredComplete = false;
				std::uint64_t recoveredSize{};
				std::optional<
					::OpenNet::Core::Content::ContentIdentity>
					recoveredExpectedSha256;
				if (auto persistedSha = recoverySettings.GetString(
					ExpectedSha256Category, rec.recordId))
				{
					if (auto expected = ParseExpectedSha256(
						"sha-256=" + *persistedSha))
					{
						recoveredExpectedSha256 = *expected;
						try
						{
							auto const hashed =
								::OpenNet::Core::Content::ContentHasher::HashFile(
									targetFilePath);
							recoveredSize = hashed.size;
							recoveredComplete =
								(rec.totalSize == 0
									|| hashed.size
										== static_cast<std::uint64_t>(
											rec.totalSize))
								&& std::ranges::find(
									hashed.identities, *expected)
									!= hashed.identities.end();
						}
						catch (...)
						{
						}
					}
				}

				auto enqueueRecoveredCatalog =
					[&](std::optional<
						::OpenNet::Core::Content::ContentIdentity> expected)
					{
						std::vector<
							::OpenNet::Core::Content::ResourceKey>
							resourceKeys;
						if (recoverySettings.GetInt(
								ResourceHintEligibilityCategory,
								rec.recordId,
								0) != 0)
						{
							std::vector<std::string> resourceUris;
							if (!rec.url.empty())
								resourceUris.push_back(rec.url);
							if (auto persistedWebSeed =
								recoverySettings.GetString(
									CanonicalWebSeedUrlCategory,
									rec.recordId);
								persistedWebSeed
								&& !persistedWebSeed->empty())
							{
								resourceUris.push_back(
									*persistedWebSeed);
							}

							resourceKeys =
								BuildResourceKeys(resourceUris);
							if (auto persistedValidator =
								recoverySettings.GetString(
									ResourceValidatorKeyCategory,
									rec.recordId))
							{
								if (auto validatorKey =
									ParsePersistedResourceKey(
										*persistedValidator);
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

						std::vector<
							::OpenNet::Core::Content::ContentIdentity>
							knownIdentities;
						if (expected)
							knownIdentities.push_back(*expected);

						::OpenNet::Core::Content::ContentCatalogService::Instance()
							.EnqueueFile(
								targetFilePath,
								{
									::OpenNet::Core::Content::ContentSourceKind::Http,
									rec.recordId,
									std::nullopt
								},
								std::move(knownIdentities),
								std::move(resourceKeys));
					};

				if (completedCanonicalRecord)
				{
					if (recoveredComplete)
						enqueueRecoveredCatalog(
							recoveredExpectedSha256);
					continue;
				}

				if (recoveredComplete)
				{
					bool shellRemoved = false;
					{
						std::lock_guard rpcLock(m_aria2->InstanceLock());
						shellRemoved =
							RemoveAria2TaskUnlocked(
								*m_aria2, rec.lastGid);
					}
					if (!shellRemoved)
						continue;

					auto const aria2ControlPath = std::filesystem::path{
						targetFilePath.wstring() + L".aria2" };
					std::error_code error;
					std::filesystem::remove(aria2ControlPath, error);
					HttpStateManager::Instance().UpdateRecordProgress(
						rec.recordId,
						static_cast<std::int64_t>(recoveredSize),
						static_cast<std::int64_t>(recoveredSize));
					HttpStateManager::Instance().UpdateRecordStatus(
						rec.recordId, 3);

					// Enqueue is intentionally replayable. If the process dies
					// before ContentCatalog's worker commits, status==Complete
					// records are revisited on the next startup until the
					// location is durably present.
					enqueueRecoveredCatalog(
						recoveredExpectedSha256);
					continue;
				}

				bool payloadCleanupSucceeded = false;
				for (int attempt = 0; attempt < 40; ++attempt)
				{
					std::error_code error;
					std::filesystem::remove(targetFilePath, error);
					std::error_code existsError;
					if (!std::filesystem::exists(
							targetFilePath, existsError)
						&& !existsError)
					{
						payloadCleanupSucceeded = true;
						break;
					}
					std::this_thread::sleep_for(
						std::chrono::milliseconds(50));
				}
				if (!payloadCleanupSucceeded)
				continue;

				HttpStateManager::Instance().UpdateRecordActiveEngine(
					rec.recordId,
					static_cast<int>(HttpTransferEngine::Aria2));
				if (!rec.userRequestedPaused)
				{
					try
					{
						std::lock_guard rpcLock(m_aria2->InstanceLock());
						m_aria2->Resume(rec.lastGid);
					}
					catch (...)
					{
					}
				}
				HttpStateManager::Instance().UpdateRecordStatus(
					rec.recordId,
					rec.userRequestedPaused ? 2 : 1);
			}

			{
				std::lock_guard<std::mutex> lock(m_mutex);
				// Start refresh/discovery only after writer ownership recovery.
				m_stopRefresh.store(false);
				m_refreshThread = std::thread([this]()
				{
					RefreshThreadEntry();
				});
				m_resourceDiscoveryStopSource = std::stop_source{};
				m_stopResourceDiscovery.store(false);
				m_resourceDiscoveryThread = std::thread([this]()
				{
					ResourceDiscoveryThreadEntry();
				});

				m_canonicalHybridStopSource = std::stop_source{};
				m_stopCanonicalHybrid.store(false);
				m_canonicalHybridWorkers.clear();
				for (int index = 0; index < 2; ++index)
				{
					m_canonicalHybridWorkers.emplace_back([this]()
					{
						CanonicalHybridThreadEntry();
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

		m_resourceDiscoveryStopSource.request_stop();
		{
			std::lock_guard lock(m_resourceDiscoveryMutex);
			m_stopResourceDiscovery.store(true);
			m_resourceDiscoveryJobs.clear();
		}
		m_resourceDiscoveryCv.notify_all();
		if (m_resourceDiscoveryThread.joinable())
			m_resourceDiscoveryThread.join();

		m_canonicalHybridStopSource.request_stop();
		{
			std::lock_guard lock(m_canonicalHybridMutex);
			m_stopCanonicalHybrid.store(true);
			m_canonicalHybridJobs.clear();
			for (auto& [gid, state] : m_canonicalHybrids)
			{
				(void)gid;
				state.cancelRequested = true;
			}
		}
		m_canonicalHybridCv.notify_all();
		for (auto& worker : m_canonicalHybridWorkers)
		{
			if (worker.joinable()) worker.join();
		}
		m_canonicalHybridWorkers.clear();
		{
			std::lock_guard lock(m_canonicalHybridMutex);
			m_canonicalHybridSuppressedGids.clear();
			m_hybridProbeGids.clear();
			m_userPausedHttpGids.clear();
			m_canonicalHybrids.clear();
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

		// Keep duplicate detection, aria2 creation and persistence association
		// one transaction-like process operation. The SQLite output_key unique
		// index remains the durable backstop.
		std::lock_guard addLock(m_httpAddMutex);

		bool const resourceHintSafe = IsResourceHintSafe(options);
		bool const p2pPreferred =
			options.TransferMode == Aria2::HttpTransferMode::P2PPreferred;
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
		auto hybridWebSeeds = p2pPreferred && resourceHintSafe
			? BuildHybridWebSeeds(options)
			: std::vector<std::string>{};

		std::filesystem::path canonicalHybridTarget;
		if (expectedSha256
			&& !options.Dir.empty()
			&& !options.OutFileName.empty())
		{
			canonicalHybridTarget =
				std::filesystem::path{
					winrt::to_hstring(options.Dir).c_str() }
				/ std::filesystem::path{
					winrt::to_hstring(options.OutFileName).c_str() };
		}

		bool const hybridProbe =
			p2pPreferred
			&& resourceHintSafe
			&& expectedSha256.has_value()
			&& options.ResourceContentLength != 0
			&& !canonicalHybridTarget.empty()
			&& !resourceKeys.empty()
			&& !hybridWebSeeds.empty();

		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			auto& stateManager = HttpStateManager::Instance();
			stateManager.Initialize();

			auto resolveExisting = [&](std::optional<HttpDownloadRecord> existing)
				-> std::optional<std::string>
			{
				if (!existing)
					return std::nullopt;
				if (existing->lastGid.empty())
				{
					stateManager.UpdateRecordStatus(existing->recordId, 4);
					return std::nullopt;
				}

				try
				{
					auto const information =
						m_aria2->GetTaskInformation(existing->lastGid);
					switch (information.Status)
					{
						case Aria2::DownloadStatus::Removed:
						case Aria2::DownloadStatus::Error:
							stateManager.UpdateRecordStatus(
								existing->recordId, 4);
							return std::nullopt;
						case Aria2::DownloadStatus::Complete:
							stateManager.UpdateRecordProgress(
								existing->recordId,
								static_cast<std::int64_t>(
									information.CompletedLength),
								static_cast<std::int64_t>(
									information.TotalLength));
							stateManager.UpdateRecordStatus(
								existing->recordId, 3);
							return existing->lastGid;
						default:
							return existing->lastGid;
					}
				}
				catch (...)
				{
					// An RPC failure does not prove the previous writer is gone.
					// Conservatively preserve its ownership rather than risk
					// creating a second writer for the same physical path.
					return existing->lastGid;
				}
			};

			if (auto const existing = resolveExisting(
				stateManager.FindActiveByUrl(options.Uris.front())))
				return *existing;

			if (!options.Dir.empty() && !options.OutFileName.empty())
			{
				if (auto const existing = resolveExisting(
					stateManager.FindActiveByOutputPath(
						options.Dir,
						options.OutFileName)))
					return *existing;
			}

			auto& database = AppSettingsDatabase::Instance();
			database.Initialize();
			auto effectiveOptions = options;
			if (hybridProbe)
				effectiveOptions.StartPaused = true;
			if (effectiveOptions.ConnectionsPerServer == 0)
				effectiveOptions.ConnectionsPerServer = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
					database.GetInt(AppSettingsDatabase::CAT_DOWNLOAD,
									"aria2_connections_per_server", 8), 1, 16));
			auto const gid = m_aria2->AddUriWithOptions(effectiveOptions);

			// Persist the download record
			if (!gid.empty())
			{
				if (!effectiveOptions.Description.empty()) database.SetString("http_task_description", gid, effectiveOptions.Description);
				auto recordId = stateManager.AddRecord(
					effectiveOptions.Uris.front(),
					effectiveOptions.Dir,
					effectiveOptions.OutFileName);
				if (recordId.empty())
				{
					try
					{
						m_aria2->Cancel(gid, true);
						m_aria2->Remove(gid);
					}
					catch (...)
					{
					}
					return {};
				}

				// A concurrent external aria2 reconciliation can theoretically
				// publish the same durable record while this RPC is in flight.
				// Never replace another task's GID: discard our newly-created
				// shell and keep the existing owner.
				if (auto const persisted =
					stateManager.FindByRecordId(recordId);
					persisted
					&& !persisted->lastGid.empty()
					&& persisted->lastGid != gid)
				{
					try
					{
						m_aria2->Cancel(gid, true);
						m_aria2->Remove(gid);
					}
					catch (...)
					{
					}
					return persisted->lastGid;
				}

				stateManager.UpdateRecordGid(recordId, gid);
				stateManager.UpdateRecordTransferPolicy(
					recordId,
					static_cast<int>(options.TransferMode),
					options.StartPaused);
				stateManager.UpdateRecordActiveEngine(
					recordId,
					static_cast<int>(
						hybridProbe
							? HttpTransferEngine::CanonicalProbe
							: HttpTransferEngine::Aria2));
				if (options.ResourceContentLength != 0)
				{
					stateManager.UpdateRecordProgress(
						recordId,
						0,
						static_cast<std::int64_t>(
							options.ResourceContentLength));
				}
				if (expectedSha256)
				{
					database.SetString(
						ExpectedSha256Category,
						recordId,
						expectedSha256->ToHex());
				}
				else
				{
					database.Delete(
						ExpectedSha256Category,
						recordId);
				}
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
				if (!hybridWebSeeds.empty())
				{
					// This URL is kept locally only. It is persisted solely
					// after the active Range probe proved BEP 19-compatible
					// byte-range behavior for a public, credential-free URL.
					database.SetString(
						CanonicalWebSeedUrlCategory,
						recordId,
						hybridWebSeeds.front());
				}
				else
				{
					database.Delete(
						CanonicalWebSeedUrlCategory,
						recordId);
				}
				{
					std::lock_guard lock(m_mutex);
					m_gidToRecordId[gid] = recordId;
					m_lastHttpStatuses[gid] = effectiveOptions.StartPaused ? Aria2::DownloadStatus::Paused : Aria2::DownloadStatus::Waiting;
					m_httpTaskLogs[gid].push_back({ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(), effectiveOptions.StartPaused ? "Task added in paused state." : "Download task started." });
					if (!effectiveOptions.Description.empty()) m_httpTaskLogs[gid].push_back({ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(), "Description: " + effectiveOptions.Description });
					m_httpTaskLogs[gid].push_back({
						std::chrono::duration_cast<std::chrono::seconds>(
							std::chrono::system_clock::now().time_since_epoch()).count(),
						p2pPreferred
							? "Transfer mode: P2P preferred; aria2 is fallback only when canonical metadata is unavailable."
							: "Transfer mode: aria2 only."
					});
					if (p2pPreferred && !hybridProbe)
					{
						m_httpTaskLogs[gid].push_back({
							std::chrono::duration_cast<std::chrono::seconds>(
								std::chrono::system_clock::now().time_since_epoch()).count(),
							"P2P acceleration prerequisites are not available; using aria2 directly."
						});
					}
				}
			}

			if (!gid.empty() && hybridProbe)
			{
				{
					std::lock_guard fallbackLock(m_canonicalHybridMutex);
					m_hybridProbeGids.insert(gid);
					if (options.StartPaused)
						m_userPausedHttpGids.insert(gid);
				}
				QueueResourceDiscovery(
					gid,
					std::move(resourceKeys),
					std::move(expectedSha256),
					std::move(canonicalHybridTarget),
					options.ResourceContentLength,
					std::move(hybridWebSeeds));
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

		if (auto record = HttpStateManager::Instance().FindByGid(gid))
		{
			HttpStateManager::Instance().UpdateRecordTransferPolicy(
				record->recordId,
				record->transferMode,
				true);
		}

		bool hybridOwned = false;
		std::string sessionId;
		{
			std::lock_guard lock(m_canonicalHybridMutex);
			if (m_hybridProbeGids.contains(gid))
			{
				hybridOwned = true;
				m_userPausedHttpGids.insert(gid);
			}
			if (auto const state = m_canonicalHybrids.find(gid);
				state != m_canonicalHybrids.end()
				&& !state->second.cancelRequested
				&& state->second.phase != CanonicalHybridPhase::Failed)
			{
				hybridOwned = true;
				m_userPausedHttpGids.insert(gid);
				state->second.userPaused = true;
				if (state->second.phase == CanonicalHybridPhase::Downloading)
					sessionId = state->second.job.sessionId;
			}
		}

		if (!sessionId.empty())
			::OpenNet::Core::P2PManager::Instance()
				.PauseLongSeedSession(sessionId);

		if (!hybridOwned)
			CancelCanonicalHybrid(gid);

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

		if (auto record = HttpStateManager::Instance().FindByGid(gid))
		{
			HttpStateManager::Instance().UpdateRecordTransferPolicy(
				record->recordId,
				record->transferMode,
				false);
		}

		bool hybridOwned = false;
		bool queueWorker = false;
		std::string sessionId;
		{
			std::lock_guard lock(m_canonicalHybridMutex);
			m_userPausedHttpGids.erase(gid);

			if (auto state = m_canonicalHybrids.find(gid);
				state != m_canonicalHybrids.end()
				&& !state->second.cancelRequested)
			{
				if (state->second.phase == CanonicalHybridPhase::Failed)
				{
					std::error_code error;
					std::filesystem::remove(
						state->second.job.targetFilePath,
						error);
					std::error_code existsError;
					bool const fallbackSafe =
						!std::filesystem::exists(
							state->second.job.targetFilePath,
							existsError)
						&& !existsError;
					if (!fallbackSafe)
					{
						// The previous libtorrent writer may still have the
						// destination open. Never resume aria2 onto that path.
						hybridOwned = true;
					}
					else
					{
						m_canonicalHybrids.erase(state);
						m_hybridProbeGids.erase(gid);
					}
				}
				else
				{
					hybridOwned = true;
					state->second.userPaused = false;
					if (state->second.phase == CanonicalHybridPhase::Downloading)
					{
						sessionId = state->second.job.sessionId;
					}
					else if (state->second.phase == CanonicalHybridPhase::Pending
						&& !state->second.workerQueued)
					{
						state->second.workerQueued = true;
						m_canonicalHybridJobs.push_back(state->second.job);
						queueWorker = true;
					}
				}
			}
			else if (m_hybridProbeGids.contains(gid))
			{
				// Discovery is still deciding whether libtorrent can own the
				// target. Keep the aria2 control shell paused until it resolves.
				hybridOwned = true;
			}
		}

		if (queueWorker)
			m_canonicalHybridCv.notify_one();
		if (!sessionId.empty())
			::OpenNet::Core::P2PManager::Instance()
				.ResumeLongSeedSession(sessionId);
		if (hybridOwned)
			return;

		if (auto record =
			HttpStateManager::Instance().FindByGid(gid);
			record
			&& TryQueueResumeResourceDiscovery(gid, *record))
		{
			return;
		}

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
		CancelCanonicalHybrid(gid);
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
		CancelCanonicalHybrid(gid);
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
		CancelCanonicalHybrid(gid);
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
			settings.Delete(
				ExpectedSha256Category,
				resourceRecordId);
			settings.Delete(
				CanonicalWebSeedUrlCategory,
				resourceRecordId);
		}
	}

	void DownloadManager::PauseAllHttp()
	{
		if (!IsAria2Available())
			return;
		std::vector<std::string> gids;
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			gids = m_aria2->GetTaskList(false);
		}
		catch (...)
		{
			return;
		}
		for (auto const& gid : gids)
			PauseHttpDownload(gid);
	}

	void DownloadManager::ResumeAllHttp()
	{
		if (!IsAria2Available())
			return;
		std::vector<std::string> gids;
		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			gids = m_aria2->GetTaskList(false);
		}
		catch (...)
		{
			return;
		}
		for (auto const& gid : gids)
			ResumeHttpDownload(gid);
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
		std::uint64_t expectedSize,
		std::vector<std::string> webSeeds)
	{
		if (gid.empty() || resourceKeys.empty()) return;
		{
			std::lock_guard lock(m_resourceDiscoveryMutex);
			m_resourceDiscoveryJobs.push_back({
				std::move(gid),
				std::move(resourceKeys),
				std::move(expectedSha256),
				std::move(targetFilePath),
				expectedSize,
				std::move(webSeeds)
			});
		}
		m_resourceDiscoveryCv.notify_one();
	}

	void DownloadManager::ResourceDiscoveryThreadEntry()
	{
		winrt::init_apartment(winrt::apartment_type::multi_threaded);
		::OpenNet::Core::Content::ContentDirectoryClient client;
		auto const stopToken =
			m_resourceDiscoveryStopSource.get_token();

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
				if (stopToken.stop_requested())
					break;

				auto lookup = client.LookupResource(
					key,
					8,
					stopToken);
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

			if (stopToken.stop_requested())
				break;

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
				QueueCanonicalHybrid(job, summary);
			}
			else
			{
				bool userPaused = false;
				bool suppressed = false;
				{
					std::lock_guard fallbackLock(m_canonicalHybridMutex);
					m_hybridProbeGids.erase(job.gid);
					userPaused = m_userPausedHttpGids.contains(job.gid);
					suppressed =
						m_canonicalHybridSuppressedGids.contains(job.gid);
				}
				if (auto const recordId = GetRecordIdForGid(job.gid);
					!recordId.empty())
				{
					HttpStateManager::Instance().UpdateRecordActiveEngine(
						recordId,
						static_cast<int>(HttpTransferEngine::Aria2));
				}
				if (!suppressed && !userPaused)
				{
					try
					{
						std::lock_guard rpcLock(m_aria2->InstanceLock());
						m_aria2->Resume(job.gid);
					}
					catch (...)
					{
					}
				}
				std::lock_guard lock(m_mutex);
				m_httpTaskLogs[job.gid].push_back({
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now()
							.time_since_epoch()).count(),
					userPaused
						? "Canonical metadata unavailable; aria2 fallback remains paused by user."
						: "Canonical metadata unavailable; falling back to aria2 origin download."
				});
			}
		}

		winrt::uninit_apartment();
	}

	bool DownloadManager::TryQueueResumeResourceDiscovery(
		std::string const& gid,
		HttpDownloadRecord const& record)
	{
		if (record.status >= 3
			|| record.transferMode
				!= static_cast<int>(
					Aria2::HttpTransferMode::P2PPreferred)
			|| record.savePath.empty()
			|| record.fileName.empty()
			|| m_stopResourceDiscovery.load())
		{
			return false;
		}

		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		if (settings.GetInt(
				ResourceHintEligibilityCategory,
				record.recordId,
				0) == 0)
		{
			return false;
		}

		auto const expectedHex =
			settings.GetString(
				ExpectedSha256Category,
				record.recordId);
		if (!expectedHex || expectedHex->empty())
			return false;
		auto expectedSha256 =
			ParseExpectedSha256(
				"sha-256=" + *expectedHex);
		if (!expectedSha256)
			return false;

		auto const persistedWebSeed =
			settings.GetString(
				CanonicalWebSeedUrlCategory,
				record.recordId);
		if (!persistedWebSeed
			|| persistedWebSeed->empty()
			|| !IsDirectFileWebSeedUrl(*persistedWebSeed)
			|| !::OpenNet::Core::Content::ResourceKeyFactory::FromHttpUrl(
				*persistedWebSeed))
		{
			return false;
		}

		Aria2::DownloadInformation task;
		std::vector<std::string> uris;
		if (!record.url.empty())
			uris.push_back(record.url);
		uris.push_back(*persistedWebSeed);

		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			task = m_aria2->GetTaskInformation(gid);
			if (task.Status != Aria2::DownloadStatus::Paused
				|| task.CompletedLength != 0)
			{
				return false;
			}

			for (auto const& file : task.Files)
			{
				for (auto const& uri : file.Uris)
				{
					if (!uri.Uri.empty())
						uris.push_back(uri.Uri);
				}
			}
		}
		catch (...)
		{
			return false;
		}

		try
		{
			std::lock_guard rpcLock(m_aria2->InstanceLock());
			for (auto const& group : m_aria2->GetTaskServers(gid))
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
			// Server-list enrichment is opportunistic. The persisted verified
			// direct WebSeed and original URL are already sufficient keys.
		}

		auto const expectedSize =
			task.TotalLength != 0
				? static_cast<std::uint64_t>(task.TotalLength)
				: static_cast<std::uint64_t>(
					(std::max)(std::int64_t{}, record.totalSize));
		if (expectedSize == 0)
			return false;

		auto targetFilePath =
			std::filesystem::path{
				winrt::to_hstring(record.savePath).c_str() }
			/ std::filesystem::path{
				winrt::to_hstring(record.fileName).c_str() };

		// Resume is the only safe time to reconsider ownership after an
		// aria2 fallback. Require zero completed payload bytes and prove that
		// the paused aria2 payload can actually be removed before libtorrent
		// is allowed to become the writer. The .aria2 control file is kept.
		std::error_code error;
		if (std::filesystem::exists(targetFilePath, error))
		{
			if (error)
				return false;
			std::filesystem::remove(targetFilePath, error);
			if (error)
				return false;
		}
		error.clear();
		if (std::filesystem::exists(targetFilePath, error)
			|| error)
		{
			return false;
		}

		auto resourceKeys = BuildResourceKeys(uris);
		if (auto persisted =
			settings.GetString(
				ResourceValidatorKeyCategory,
				record.recordId))
		{
			if (auto validatorKey =
				ParsePersistedResourceKey(*persisted);
				validatorKey
				&& std::ranges::find(
					resourceKeys, *validatorKey)
					== resourceKeys.end())
			{
				resourceKeys.push_back(*validatorKey);
			}
		}
		if (resourceKeys.empty())
			return false;

		{
			std::lock_guard fallbackLock(m_canonicalHybridMutex);
			if (auto const existing =
				m_canonicalHybrids.find(gid);
				existing != m_canonicalHybrids.end()
				&& !existing->second.cancelRequested
				&& existing->second.phase
					!= CanonicalHybridPhase::Failed)
			{
				return true;
			}
			if (m_hybridProbeGids.contains(gid))
				return true;

			// Pause intentionally suppresses late fallback work. An explicit
			// Resume starts a new ownership decision, so that suppression no
			// longer applies to this new probe.
			m_canonicalHybridSuppressedGids.erase(gid);
			m_userPausedHttpGids.erase(gid);
			m_hybridProbeGids.insert(gid);
		}

		HttpStateManager::Instance().UpdateRecordActiveEngine(
			record.recordId,
			static_cast<int>(
				HttpTransferEngine::CanonicalProbe));
		{
			std::lock_guard lock(m_mutex);
			m_httpResourceDiscoveries.erase(gid);
			m_httpTaskLogs[gid].push_back({
				std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now()
						.time_since_epoch()).count(),
				"Resume re-checking trusted canonical metadata before returning payload ownership to aria2."
			});
		}

		QueueResourceDiscovery(
			gid,
			std::move(resourceKeys),
			std::move(expectedSha256),
			std::move(targetFilePath),
			expectedSize,
			{ *persistedWebSeed });
		return true;
	}

	void DownloadManager::QueueCanonicalHybrid(
		ResourceDiscoveryJob const& discovery,
		HttpResourceDiscovery const& summary)
	{
		if (!summary.bep52Identity
			|| !discovery.expectedSha256
			|| discovery.targetFilePath.empty())
			return;

		CanonicalHybridJob job;
		job.gid = discovery.gid;
		job.sessionId = "http-hybrid:" + discovery.gid;
		job.targetFilePath = discovery.targetFilePath;
		job.bep52Identity = *summary.bep52Identity;
		job.expectedSha256 = *discovery.expectedSha256;
		job.resourceKeys = discovery.resourceKeys;
		job.expectedSize = summary.size;
		job.webSeeds = discovery.webSeeds;

		{
			std::lock_guard lock(m_canonicalHybridMutex);
			if (m_stopCanonicalHybrid.load()
				|| m_canonicalHybridSuppressedGids.contains(job.gid))
				return;
			if (auto const existing = m_canonicalHybrids.find(job.gid);
				existing != m_canonicalHybrids.end()
				&& existing->second.phase != CanonicalHybridPhase::Failed
				&& !existing->second.cancelRequested)
				return;

			m_hybridProbeGids.erase(job.gid);
			CanonicalHybridState state;
			state.phase = CanonicalHybridPhase::Pending;
			state.job = job;
			state.userPaused = m_userPausedHttpGids.contains(job.gid);
			state.workerQueued = !state.userPaused;
			m_canonicalHybrids.insert_or_assign(job.gid, state);
			if (state.workerQueued)
				m_canonicalHybridJobs.push_back(job);
		}

		{
			std::lock_guard lock(m_mutex);
			if (auto discoveryState =
				m_httpResourceDiscoveries.find(job.gid);
				discoveryState != m_httpResourceDiscoveries.end())
			{
				discoveryState->second.canonicalHybridQueued = true;
			}
			m_httpTaskLogs[job.gid].push_back({
				std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now()
						.time_since_epoch()).count(),
				"Trusted canonical resource matched; libtorrent hybrid transfer queued with HTTP web seed + OpenNet peers."
			});
		}
		{
			std::lock_guard lock(m_canonicalHybridMutex);
			auto const state = m_canonicalHybrids.find(job.gid);
			if (state != m_canonicalHybrids.end()
				&& state->second.workerQueued)
				m_canonicalHybridCv.notify_one();
		}
	}

	bool DownloadManager::HasCanonicalHybridPending(
		std::string const& gid) const
	{
		std::lock_guard lock(m_canonicalHybridMutex);
		auto const it = m_canonicalHybrids.find(gid);
		if (it == m_canonicalHybrids.end()
			|| it->second.cancelRequested)
			return false;
		return it->second.phase == CanonicalHybridPhase::Pending
			|| it->second.phase == CanonicalHybridPhase::Downloading
			|| it->second.phase == CanonicalHybridPhase::Ready;
	}

	void DownloadManager::CancelCanonicalHybrid(std::string const& gid)
	{
		if (gid.empty()) return;

		std::optional<CanonicalHybridJob> immediateCleanup;
		{
			std::lock_guard lock(m_canonicalHybridMutex);
			m_canonicalHybridSuppressedGids.insert(gid);
			m_hybridProbeGids.erase(gid);
			m_userPausedHttpGids.erase(gid);
			auto const it = m_canonicalHybrids.find(gid);
			if (it == m_canonicalHybrids.end())
				return;

			it->second.cancelRequested = true;
			std::erase_if(
				m_canonicalHybridJobs,
				[&](CanonicalHybridJob const& job)
				{
					return job.gid == gid;
				});

			if (it->second.phase == CanonicalHybridPhase::Pending
				|| it->second.phase == CanonicalHybridPhase::Ready
				|| it->second.phase == CanonicalHybridPhase::Failed)
			{
				immediateCleanup = it->second.job;
				m_canonicalHybrids.erase(it);
			}
		}

		if (immediateCleanup)
		{
			::OpenNet::Core::P2PManager::Instance()
				.CloseLongSeedSession(immediateCleanup->sessionId);
		}
		m_canonicalHybridCv.notify_all();
	}

	void DownloadManager::CanonicalHybridThreadEntry()
	{
		winrt::init_apartment(winrt::apartment_type::multi_threaded);
		auto const stopToken =
			m_canonicalHybridStopSource.get_token();

		auto fail = [this](
			CanonicalHybridJob const& job,
			std::string message)
		{
			::OpenNet::Core::P2PManager::Instance()
				.CloseLongSeedSession(job.sessionId);

			std::error_code error;
			// libtorrent was the only payload writer. Close the hidden torrent
			// first, then wait until its incomplete payload can be removed
			// before aria2 is allowed to own the same path.
			bool payloadCleanupSucceeded = false;
			for (int attempt = 0; attempt < 40; ++attempt)
			{
				error.clear();
				std::filesystem::remove(job.targetFilePath, error);
				std::error_code existsError;
				if (!std::filesystem::exists(job.targetFilePath, existsError)
					&& !existsError)
				{
					payloadCleanupSucceeded = true;
					break;
				}
				std::this_thread::sleep_for(
					std::chrono::milliseconds(50));
			}
			if (!payloadCleanupSucceeded)
			{
				message +=
					" Incomplete libtorrent output could not be removed; aria2 remains paused to preserve single-writer ownership.";
			}
			// Do not delete the aria2 .aria2 control file here. aria2 is the
			// fallback control shell and still owns that state.

			bool shouldLog = false;
			bool resumeAria2 = false;
			bool userPaused = false;
			{
				std::lock_guard lock(m_canonicalHybridMutex);
				m_hybridProbeGids.erase(job.gid);
				auto const it = m_canonicalHybrids.find(job.gid);
				if (it != m_canonicalHybrids.end())
				{
					if (it->second.cancelRequested)
					{
						m_canonicalHybrids.erase(it);
					}
					else
					{
						it->second.phase = CanonicalHybridPhase::Failed;
						it->second.workerQueued = false;
						it->second.error = message;
						userPaused = it->second.userPaused
							|| m_userPausedHttpGids.contains(job.gid);
						resumeAria2 =
							payloadCleanupSucceeded
							&& !userPaused
							&& !m_canonicalHybridSuppressedGids.contains(job.gid);
						shouldLog = true;
					}
				}
			}

			if (payloadCleanupSucceeded)
			{
				if (auto const recordId = GetRecordIdForGid(job.gid);
					!recordId.empty())
				{
					HttpStateManager::Instance().UpdateRecordActiveEngine(
						recordId,
						static_cast<int>(HttpTransferEngine::Aria2));
				}
			}

			if (resumeAria2)
			{
				try
				{
					std::lock_guard rpcLock(m_aria2->InstanceLock());
					m_aria2->Resume(job.gid);
				}
				catch (...)
				{
				}
			}

			if (shouldLog)
			{
				std::lock_guard lock(m_mutex);
				m_httpTaskLogs[job.gid].push_back({
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now()
							.time_since_epoch()).count(),
					(userPaused
						? "Canonical HTTP/P2P hybrid failed; aria2 fallback remains paused by user: "
						: "Canonical HTTP/P2P hybrid failed; falling back to aria2 origin download: ")
						+ message
				});
			}
		};

		for (;;)
		{
			CanonicalHybridJob job;
			{
				std::unique_lock lock(m_canonicalHybridMutex);
				m_canonicalHybridCv.wait(lock, [this]
				{
					return m_stopCanonicalHybrid.load()
						|| !m_canonicalHybridJobs.empty();
				});

				if (m_stopCanonicalHybrid.load()
					&& m_canonicalHybridJobs.empty())
					break;

				job = std::move(m_canonicalHybridJobs.front());
				m_canonicalHybridJobs.pop_front();

				auto const state = m_canonicalHybrids.find(job.gid);
				if (state == m_canonicalHybrids.end()
					|| state->second.cancelRequested)
					continue;
				state->second.workerQueued = false;
				if (state->second.userPaused)
					continue;
				state->second.phase =
					CanonicalHybridPhase::Downloading;
			}

			{
				std::error_code error;
				std::filesystem::remove(
					job.targetFilePath,
					error);
				std::error_code existsError;
				auto const stillExists =
					std::filesystem::exists(
						job.targetFilePath,
						existsError);
				if (error
					|| existsError
					|| stillExists)
				{
					fail(
						job,
						"Paused aria2 payload could not be released before libtorrent ownership transfer.");
					continue;
				}
			}

			auto const recordId = GetRecordIdForGid(job.gid);
			if (!recordId.empty())
			{
				HttpStateManager::Instance().UpdateRecordActiveEngine(
					recordId,
					static_cast<int>(
						HttpTransferEngine::CanonicalHybrid));
			}

			if (stopToken.stop_requested())
			{
				fail(job, "Canonical transfer cancelled during shutdown.");
				continue;
			}

			bool started = false;
			try
			{
				started =
					::OpenNet::Core::P2PManager::Instance()
						.StartLongSeedDownloadAsync(
							job.bep52Identity,
							job.targetFilePath,
							20,
							job.sessionId,
							job.webSeeds,
							stopToken)
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

			auto const initialStatus =
				::OpenNet::Core::P2PManager::Instance()
					.GetLongSeedSessionStatus(job.sessionId);
			job.canonicalInfoHashV2 = initialStatus.infoHashV2;
			if (!recordId.empty())
			{
				HttpStateManager::Instance().UpdateRecordActiveEngine(
					recordId,
					static_cast<int>(
						HttpTransferEngine::CanonicalHybrid),
					job.canonicalInfoHashV2);
			}
			{
				std::lock_guard lock(m_canonicalHybridMutex);
				if (auto const state = m_canonicalHybrids.find(job.gid);
					state != m_canonicalHybrids.end())
				{
					state->second.job.canonicalInfoHashV2 =
						job.canonicalInfoHashV2;
				}
			}
			if (!job.canonicalInfoHashV2.empty())
			{
				std::lock_guard lock(m_mutex);
				if (auto discovery =
					m_httpResourceDiscoveries.find(job.gid);
					discovery != m_httpResourceDiscoveries.end())
				{
					discovery->second.canonicalInfoHashV2 =
						job.canonicalInfoHashV2;
				}
				m_httpTaskLogs[job.gid].push_back({
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now()
							.time_since_epoch()).count(),
					"Canonical libtorrent session started; v2 info-hash: "
						+ job.canonicalInfoHashV2
				});
			}

			auto deadline =
				std::chrono::steady_clock::now()
				+ std::chrono::minutes(30);
			bool completed = false;
			bool cancelled = false;
			std::string failure;

			while (std::chrono::steady_clock::now() < deadline)
			{
				bool userPaused = false;
				{
					std::lock_guard lock(m_canonicalHybridMutex);
					auto const state = m_canonicalHybrids.find(job.gid);
					if (m_stopCanonicalHybrid.load()
						|| state == m_canonicalHybrids.end()
						|| state->second.cancelRequested)
					{
						cancelled = true;
						break;
					}
					userPaused = state->second.userPaused;
				}
				if (userPaused)
				{
					// Paused wall-clock time must not consume the transfer timeout.
					deadline += std::chrono::milliseconds(500);
					std::this_thread::sleep_for(
						std::chrono::milliseconds(500));
					continue;
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
					std::lock_guard lock(m_canonicalHybridMutex);
					auto const state = m_canonicalHybrids.find(job.gid);
					if (state != m_canonicalHybrids.end())
					{
						state->second.progressPercent =
							status.progressPercent;
						state->second.downloadRate =
							status.downloadRate;
						state->second.uploadRate =
							status.uploadRate;
						state->second.completedBytes =
							status.totalWantedDone;
						state->second.connectedPeers =
							status.connectedPeers;
						state->second.connectedSeeds =
							status.connectedSeeds;
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
				std::lock_guard lock(m_canonicalHybridMutex);
				m_canonicalHybrids.erase(job.gid);
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
						job.targetFilePath);

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
				std::lock_guard lock(m_canonicalHybridMutex);
				auto const state = m_canonicalHybrids.find(job.gid);
				if (state == m_canonicalHybrids.end()
					|| state->second.cancelRequested)
				{
					if (state != m_canonicalHybrids.end())
						m_canonicalHybrids.erase(state);
					continue;
				}
				state->second.phase = CanonicalHybridPhase::Ready;
				state->second.userPaused = false;
				state->second.workerQueued = false;
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
					discovery->second.canonicalHybridReady = true;
				}
				m_httpTaskLogs[job.gid].push_back({
					std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::system_clock::now()
							.time_since_epoch()).count(),
					"Canonical HTTP/P2P hybrid completed and passed caller SHA-256 verification."
				});
			}
		}

		winrt::uninit_apartment();
	}

	bool DownloadManager::TryFinalizeCanonicalHybrid(
		std::string const& gid,
		Aria2::DownloadInformation const& task,
		std::string const& recordId)
	{
		CanonicalHybridJob job;
		{
			std::lock_guard lock(m_canonicalHybridMutex);
			auto const it = m_canonicalHybrids.find(gid);
			if (it == m_canonicalHybrids.end()
				|| it->second.cancelRequested
				|| it->second.phase != CanonicalHybridPhase::Ready)
				return false;
			job = it->second.job;
		}

		std::error_code existsError;
		if (!std::filesystem::is_regular_file(
			job.targetFilePath,
			existsError)
			|| existsError)
		{
			std::lock_guard lock(m_canonicalHybridMutex);
			if (auto const it = m_canonicalHybrids.find(gid);
				it != m_canonicalHybrids.end())
			{
				it->second.phase = CanonicalHybridPhase::Failed;
				it->second.error =
					"Verified peer fallback file disappeared before promotion.";
			}
			return false;
		}

		// ProcessAria2Tasks already owns InstanceLock here. Do not mark the
		// HTTP task complete until the paused control shell is really gone;
		// otherwise aria2 may be restored as a ghost task later.
		if (!RemoveAria2TaskUnlocked(*m_aria2, gid))
			return false;

		auto const aria2ControlPath = std::filesystem::path{
			job.targetFilePath.wstring() + L".aria2" };
		{
			std::error_code error;
			std::filesystem::remove(aria2ControlPath, error);
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
			stateManager.UpdateRecordActiveEngine(
				recordId,
				static_cast<int>(
					HttpTransferEngine::CanonicalHybrid),
				job.canonicalInfoHashV2);
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
				"Canonical HTTP/P2P hybrid completed through libtorrent with one writer."
			});
			finishedCallback = m_finishedCb;
		}

		{
			std::lock_guard lock(m_canonicalHybridMutex);
			m_canonicalHybrids.erase(gid);
			m_hybridProbeGids.erase(gid);
			m_userPausedHttpGids.erase(gid);
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

						auto currentMayOwnRecord =
							[&](HttpDownloadRecord const& record)
						{
							if (record.lastGid.empty()
								|| record.lastGid == gid)
								return true;

							try
							{
								auto const previous =
									m_aria2->GetTaskInformation(
										record.lastGid);
								if (previous.Status
										== Aria2::DownloadStatus::Removed
									|| previous.Status
										== Aria2::DownloadStatus::Error)
								{
									return true;
								}
								if (previous.Status
									== Aria2::DownloadStatus::Complete)
								{
									stateManager.UpdateRecordProgress(
										record.recordId,
										static_cast<std::int64_t>(
											previous.CompletedLength),
										static_cast<std::int64_t>(
											previous.TotalLength));
									stateManager.UpdateRecordStatus(
										record.recordId, 3);
								}
								return false;
							}
							catch (...)
							{
								// RefreshInformation and tellStatus for the
								// current GID already succeeded. In this
								// reconciliation path, an unresolvable old GID
								// is stale session metadata and may be replaced.
								return true;
							}
						};

						auto record =
							stateManager.FindActiveByUrl(sourceUrl);
						if (record)
						{
							if (!currentMayOwnRecord(*record))
							{
								if (!RemoveAria2TaskUnlocked(
									*m_aria2, gid))
								{
									OutputDebugStringA(
										"DownloadManager: failed to remove duplicate restored aria2 task\n");
								}
								continue;
							}
							recordId = record->recordId;
							stateManager.UpdateRecordGid(
								recordId, gid);
						}
						else
						{
							std::string outputDir = task.Dir;
							std::string outputName;
							if (!task.Files.empty()
								&& !task.Files.front().Path.empty())
							{
								auto const outputPath =
									std::filesystem::path{
										winrt::to_hstring(
											task.Files.front().Path).c_str() };
								outputDir = winrt::to_string(
									winrt::hstring{
										outputPath.parent_path().wstring() });
								outputName = winrt::to_string(
									winrt::hstring{
										outputPath.filename().wstring() });
							}

							recordId = stateManager.AddRecord(
								sourceUrl,
								outputDir,
								outputName);
							if (recordId.empty())
							{
								if (!RemoveAria2TaskUnlocked(
									*m_aria2, gid))
								{
									OutputDebugStringA(
										"DownloadManager: failed to remove unpersistable aria2 task\n");
								}
								continue;
							}

							auto persisted =
								stateManager.FindByRecordId(recordId);
							if (persisted
								&& !currentMayOwnRecord(*persisted))
							{
								if (!RemoveAria2TaskUnlocked(
									*m_aria2, gid))
								{
									OutputDebugStringA(
										"DownloadManager: failed to remove output-conflicting restored aria2 task\n");
								}
								continue;
							}
							stateManager.UpdateRecordGid(
								recordId, gid);
						}
						if (!recordId.empty())
						{
							std::lock_guard lock(m_mutex);
							m_gidToRecordId[gid] = recordId;
						}
					}
				}

				// aria2 may learn the final filename only after redirects or
				// Content-Disposition. Claim that physical path as soon as it is
				// observable. The SQLite unique output_key makes this atomic with
				// every other active HTTP record.
				if (!recordId.empty()
					&& !task.Files.empty()
					&& !task.Files.front().Path.empty())
				{
					auto const resolvedPath = std::filesystem::path{
						winrt::to_hstring(
							task.Files.front().Path).c_str() };
					auto const resolvedDir = winrt::to_string(
						winrt::hstring{
							resolvedPath.parent_path().wstring() });
					auto const resolvedName = winrt::to_string(
						winrt::hstring{
							resolvedPath.filename().wstring() });

					auto const persisted =
						HttpStateManager::Instance()
							.FindByRecordId(recordId);
					bool const changed =
						persisted
						&& (persisted->savePath != resolvedDir
							|| persisted->fileName != resolvedName);
					if (changed
						&& !HttpStateManager::Instance()
							.UpdateRecordOutputPath(
								recordId,
								resolvedDir,
								resolvedName))
					{
						CancelCanonicalHybrid(gid);
						auto const removed =
							RemoveAria2TaskUnlocked(*m_aria2, gid);
						HttpStateManager::Instance()
							.UpdateRecordStatus(recordId, 4);
						{
							std::lock_guard lock(m_mutex);
							m_httpTaskLogs[gid].push_back({
								std::chrono::duration_cast<
									std::chrono::seconds>(
										std::chrono::system_clock::now()
											.time_since_epoch()).count(),
								removed
									? "Download stopped: the resolved output path is already owned by another active HTTP task."
									: "Output collision detected, but aria2 did not confirm task removal; ownership remains failed until cleanup succeeds."
							});
						}
						if (errorCb)
							errorCb(
								gid,
								"Resolved output path conflicts with another active HTTP task.");
						continue;
					}
				}

				if (task.Status == Aria2::DownloadStatus::Paused
					&& HasCanonicalHybridPending(gid)
					&& TryFinalizeCanonicalHybrid(gid, task, recordId))
				{
					continue;
				}

				bool const canonicalHybridHoldingError =
					task.Status == Aria2::DownloadStatus::Error
					&& HasCanonicalHybridPending(gid);

				std::optional<CanonicalHybridState> hybridState;
				{
					std::lock_guard fallbackLock(m_canonicalHybridMutex);
					if (auto const state = m_canonicalHybrids.find(gid);
						state != m_canonicalHybrids.end()
						&& !state->second.cancelRequested
						&& state->second.phase != CanonicalHybridPhase::Failed)
					{
						hybridState = state->second;
					}
				}

				if (progressCb)
				{
					HttpTaskProgress progress;
					progress.gid = gid;
					progress.name = Aria2::ToFriendlyName(task);
					progress.status = canonicalHybridHoldingError
						? Aria2::DownloadStatus::Waiting
						: task.Status;
					progress.totalLength = task.TotalLength;
					progress.completedLength = task.CompletedLength;
					progress.downloadSpeed = task.DownloadSpeed;
					progress.uploadSpeed = task.UploadSpeed;
					progress.progressPercent = (task.TotalLength > 0)
						? static_cast<int>((task.CompletedLength * 100) / task.TotalLength)
						: 0;
					{
						std::lock_guard fallbackLock(m_canonicalHybridMutex);
						if (m_hybridProbeGids.contains(gid))
							progress.engine =
								HttpTransferEngine::CanonicalProbe;
					}
					if (hybridState)
					{
						progress.status = hybridState->userPaused
							? Aria2::DownloadStatus::Paused
							: Aria2::DownloadStatus::Active;
						progress.totalLength =
							hybridState->job.expectedSize;
						progress.completedLength =
							static_cast<std::uint64_t>((std::max)(
								std::int64_t{}, hybridState->completedBytes));
						progress.downloadSpeed =
							static_cast<std::uint64_t>((std::max)(
								std::int64_t{}, hybridState->downloadRate));
						progress.uploadSpeed =
							static_cast<std::uint64_t>((std::max)(
								std::int64_t{}, hybridState->uploadRate));
						progress.connectedPeers =
							hybridState->connectedPeers;
						progress.connectedSeeds =
							hybridState->connectedSeeds;
						progress.progressPercent =
							hybridState->progressPercent;
						progress.engine =
							hybridState->phase == CanonicalHybridPhase::Pending
								? HttpTransferEngine::CanonicalProbe
								: HttpTransferEngine::CanonicalHybrid;
					}

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
						if (hybridState)
						{
							hsm.UpdateRecordProgress(
								recordId,
								(std::max)(std::int64_t{}, hybridState->completedBytes),
								static_cast<std::int64_t>(
									hybridState->job.expectedSize));
							hsm.UpdateRecordStatus(
								recordId,
								hybridState->userPaused ? 2 : 1);
						}
						else
						{
							hsm.UpdateRecordProgress(
								recordId,
								task.CompletedLength,
								task.TotalLength);
						}
						if (!canonicalHybridHoldingError && !hybridState)
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

					if (!canonicalHybridHoldingError)
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
						CancelCanonicalHybrid(gid);
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
					if (canonicalHybridHoldingError)
					{
						if (TryFinalizeCanonicalHybrid(
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
								m_canonicalHybridMutex);
							if (auto const failed =
								m_canonicalHybrids.find(gid);
								failed != m_canonicalHybrids.end()
								&& failed->second.phase
									== CanonicalHybridPhase::Failed)
							{
								m_canonicalHybrids.erase(failed);
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
