module;
#include "WindowsPlatform.h"
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#include "LibtorrentIncludeGuard.h"
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/bdecode.hpp>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/close_reason.hpp>
#include <libtorrent/error_code.hpp>
#include "Core/ClientFilter/ClientFilterManager.h"
#include "Core/IPFilter/IPFilterManager.h"
#include <libtorrent/session_stats.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/pread_disk_io.hpp>
#include <libtorrent/time.hpp>
#include <libtorrent/address.hpp>
#include <libtorrent/pex_flags.hpp>
#include <boost/asio/ip/address.hpp>
#include "TorrentSettingsAdapter.h"
#include "LibtorrentIncludeRestore.h"

module OpenNet.Core.torrentCore.LibtorrentHandle;

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Content.ContentCatalogService;
import OpenNet.Core.Content.ContentIdentity;
import OpenNet.Core.IO.FileSystem;
import OpenNet.Core.Torrent.TrackerManager;
import OpenNet.Core.torrentCore.TorrentStateManager;
import OpenNet.Core.TorrentSettings;
import winrt_base;

namespace lt = libtorrent;
using namespace std::chrono_literals;

static_assert(LIBTORRENT_VERSION_MAJOR == 2 && LIBTORRENT_VERSION_MINOR == 1 && LIBTORRENT_VERSION_TINY >= 1);
static_assert(TORRENT_USE_RTC == 1);

namespace
{
	constexpr auto TorrentTaskSettingsCategory = "torrent_task_settings";
	constexpr auto TorrentTlsCategory = "torrent_tls_protected";

	void CatalogCompletedTorrent(
		lt::torrent_handle const& handle,
		std::string const& taskId)
	{
		try
		{
			if (!handle.is_valid() || taskId.empty()) return;
			auto const info = handle.torrent_file();
			if (!info || !info->is_valid() || info->priv()) return;

			auto const& files = info->layout();
			auto const priorities = handle.get_file_priorities();
			auto const renamedFiles = handle.get_renamed_files();
			lt::filenames const currentNames(files, renamedFiles);
			auto const status = handle.status();
			auto const saveRoot = std::filesystem::path{
				winrt::to_hstring(status.save_path).c_str() };

			for (auto const index : files.file_range())
			{
				auto const ordinal = static_cast<int>(index);
				if (files.pad_file_at(index)) continue;
				if (ordinal < static_cast<int>(priorities.size())
					&& static_cast<std::uint8_t>(priorities[ordinal]) == 0)
					continue;

				auto const relativePath = currentNames.file_path(index);
				auto const path = saveRoot
					/ std::filesystem::path{ winrt::to_hstring(relativePath).c_str() };

				std::vector<::OpenNet::Core::Content::ContentIdentity> identities;
				auto const root = files.root(index);
				if (!root.is_all_zeros())
				{
					auto const* first = reinterpret_cast<std::uint8_t const*>(root.data());
					identities.push_back({
						::OpenNet::Core::Content::ContentIdentityAlgorithm::Bep52FileRootSha256,
						std::vector<std::uint8_t>(first, first + root.size())
					});
				}

				::OpenNet::Core::Content::ContentCatalogService::Instance().EnqueueFile(
					path,
					{
						::OpenNet::Core::Content::ContentSourceKind::Torrent,
						taskId,
						ordinal
					},
					std::move(identities));
			}
		}
		catch (std::exception const& exception)
		{
			OutputDebugStringA((
				"LibtorrentHandle: failed to catalog completed torrent: "
				+ std::string(exception.what()) + "\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA(
				"LibtorrentHandle: failed to catalog completed torrent\n");
		}
	}

	bool ValidateTlsBuffers(std::string const& certificate, std::string const& key,
		std::string const& dh)
	{
		constexpr std::size_t maximum = 1024 * 1024;
		if (certificate.empty() || key.empty() || certificate.size() > maximum
			|| key.size() > maximum || dh.size() > maximum) return false;
		auto bio = [](std::string const& data)
		{
			return std::unique_ptr<BIO, decltype(&BIO_free)>(
				BIO_new_mem_buf(data.data(), static_cast<int>(data.size())), BIO_free);
		};
		auto certBio = bio(certificate);
		auto keyBio = bio(key);
		if (!certBio || !keyBio) return false;
		std::unique_ptr<X509, decltype(&X509_free)> cert(
			PEM_read_bio_X509(certBio.get(), nullptr, nullptr, nullptr), X509_free);
		auto noPassword = [](char*, int, int, void*) -> int { return 0; };
		std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> privateKey(
			PEM_read_bio_PrivateKey(keyBio.get(), nullptr, noPassword, nullptr), EVP_PKEY_free);
		if (!cert || !privateKey || X509_check_private_key(cert.get(), privateKey.get()) != 1)
			return false;
		if (!dh.empty())
		{
			auto dhBio = bio(dh);
			if (!dhBio) return false;
			std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> parameters(
				PEM_read_bio_Parameters(dhBio.get(), nullptr), EVP_PKEY_free);
			if (!parameters) return false;
		}
		return true;
	}

	std::string ProtectTls(std::vector<char>& plaintext)
	{
		DATA_BLOB input{ static_cast<DWORD>(plaintext.size()),
			reinterpret_cast<BYTE*>(plaintext.data()) };
		DATA_BLOB output{};
		auto const ok = CryptProtectData(&input, L"OpenNet torrent TLS", nullptr,
			nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
		SecureZeroMemory(plaintext.data(), plaintext.size());
		if (!ok) throw std::runtime_error("Unable to protect TLS credentials");
		std::unique_ptr<BYTE, decltype(&LocalFree)> memory(output.pbData, LocalFree);
		constexpr char hex[] = "0123456789abcdef";
		std::string encoded;
		encoded.reserve(output.cbData * 2);
		for (DWORD i = 0; i < output.cbData; ++i)
		{
			encoded += hex[output.pbData[i] >> 4];
			encoded += hex[output.pbData[i] & 15];
		}
		return encoded;
	}
	constexpr std::array DhtFallbackRouters{
		std::pair{ "dht.libtorrent.org", 25401 },
		std::pair{ "dht.transmissionbt.com", 6881 },
		std::pair{ "router.bt.ouinet.work", 6881 },
		std::pair{ "router.bittorrent.com", 6881 } };

	void AddFallbackDhtRouters(lt::session& session)
	{
		for (auto const& [host, port] : DhtFallbackRouters)
		{
			session.add_dht_node({ host, port });
		}
	}

	std::string TaskSettingKey(std::string const& taskId, std::string_view const name)
	{
		return taskId + "." + std::string{ name };
	}
	void ApplyTorrentSettings(
		OpenNet::Core::TorrentSettings const& s,
		lt::settings_pack& pack)
	{
		OpenNet::Core::Torrent::Detail::ApplyNetworkSettings(s, pack);
		pack.set_int(lt::settings_pack::connections_limit, s.connectionsLimit);
		pack.set_bool(lt::settings_pack::enable_upnp, s.enableUpnp);
		pack.set_bool(lt::settings_pack::enable_natpmp, s.enableNatpmp);
		pack.set_str(lt::settings_pack::natpmp_gateway, s.natPmpGateway);
		pack.set_int(lt::settings_pack::natpmp_lease_duration, s.natPmpLeaseDuration);
		pack.set_int(lt::settings_pack::upnp_lease_duration, std::max(0, s.upnpLeaseDuration));

		pack.set_int(lt::settings_pack::active_downloads,
					 s.queueingEnabled ? s.activeDownloads : -1);
		pack.set_int(lt::settings_pack::active_seeds,
					 s.queueingEnabled ? s.activeSeeds : -1);
		pack.set_int(lt::settings_pack::active_limit,
					 s.queueingEnabled ? s.activeLimit : -1);
		pack.set_bool(lt::settings_pack::dont_count_slow_torrents,
					  s.dontCountSlowTorrents);
		pack.set_int(lt::settings_pack::inactive_down_rate,
					 s.slowTorrentDownloadRateThreshold);
		pack.set_int(lt::settings_pack::inactive_up_rate,
					 s.slowTorrentUploadRateThreshold);
		pack.set_int(lt::settings_pack::auto_manage_startup,
					 s.slowTorrentInactiveTimer);
		pack.set_int(lt::settings_pack::download_rate_limit, s.downloadRateLimit);
		pack.set_int(lt::settings_pack::upload_rate_limit, s.uploadRateLimit);

		pack.set_int(lt::settings_pack::peer_timeout, s.peerTimeout);
		pack.set_int(lt::settings_pack::handshake_timeout, s.handshakeTimeout);
		pack.set_bool(lt::settings_pack::close_redundant_connections, s.closeRedundantConnections);
		pack.set_int(lt::settings_pack::max_peerlist_size, s.maxPeerListSize);
		pack.set_int(lt::settings_pack::connection_speed, s.connectionSpeed);
		pack.set_bool(lt::settings_pack::seeding_outgoing_connections, s.seedingOutgoingConnections);
		pack.set_int(lt::settings_pack::send_socket_buffer_size, s.socketSendBufferSize);
		pack.set_int(lt::settings_pack::recv_socket_buffer_size, s.socketReceiveBufferSize);
		pack.set_int(lt::settings_pack::listen_queue_size, s.socketBacklogSize);
		pack.set_int(lt::settings_pack::mixed_mode_algorithm, s.mixedModeAlgorithm);
		pack.set_int(lt::settings_pack::resolver_cache_timeout, s.hostnameCacheTtl);
		pack.set_bool(lt::settings_pack::validate_https_trackers, s.validateHttpsTrackers);
		pack.set_bool(lt::settings_pack::ssrf_mitigation, s.ssrfMitigation);
		pack.set_bool(lt::settings_pack::no_connect_privileged_ports, s.blockPeersOnPrivilegedPorts);
		pack.set_int(lt::settings_pack::peer_turnover, s.peerTurnover);
		pack.set_int(lt::settings_pack::peer_turnover_cutoff, s.peerTurnoverCutoff);
		pack.set_int(lt::settings_pack::peer_turnover_interval, s.peerTurnoverInterval);
		pack.set_int(lt::settings_pack::max_out_request_queue, s.requestQueueSize);
		pack.set_int(lt::settings_pack::choking_algorithm, s.uploadSlotsBehavior);
		pack.set_int(lt::settings_pack::seed_choking_algorithm, s.uploadChokingAlgorithm);
		pack.set_int(lt::settings_pack::unchoke_slots_limit, s.unchokeSlotsLimit);
		pack.set_int(lt::settings_pack::alert_queue_size, s.alertQueueSize);

		pack.set_int(lt::settings_pack::aio_threads, s.aioThreads);
		pack.set_int(lt::settings_pack::hashing_threads, s.hashingThreads);
		pack.set_int(lt::settings_pack::file_pool_size, s.filePoolSize);
		pack.set_int(lt::settings_pack::checking_mem_usage,
					 std::max(1, s.checkingMemUsage) * 64);
		pack.set_int(lt::settings_pack::max_queued_disk_bytes, s.diskQueueSize);
		pack.set_bool(lt::settings_pack::piece_extent_affinity, s.pieceExtentAffinity);
		pack.set_int(lt::settings_pack::suggest_mode, s.uploadSuggestions
					 ? lt::settings_pack::suggest_read_cache
					 : lt::settings_pack::no_piece_suggestions);
		pack.set_int(lt::settings_pack::send_buffer_watermark, s.sendBufferWatermark * 1024);
		pack.set_int(lt::settings_pack::send_buffer_low_watermark, s.sendBufferLowWatermark * 1024);
		pack.set_int(lt::settings_pack::send_buffer_watermark_factor, s.sendBufferWatermarkFactor);

		pack.set_int(lt::settings_pack::alert_mask,
					 lt::alert_category::status |
					 lt::alert_category::error |
					 lt::alert_category::storage |
					 lt::alert_category::peer |
					 lt::alert_category::connect |
					 lt::alert_category::tracker |
					 lt::alert_category::stats |
					 lt::alert_category::performance_warning |
					 lt::alert_category::file_progress |
					 lt::alert_category::dht |
					 lt::alert_category::ip_block |
					 lt::alert_category::port_mapping);
		pack.set_int(lt::settings_pack::share_ratio_limit,
					 s.seedingRatioLimit > 0 ? static_cast<int>(s.seedingRatioLimit * 100) : 0);
		pack.set_int(lt::settings_pack::seed_time_limit,
					 s.seedingTimeLimit > 0 ? s.seedingTimeLimit * 60 : 0);
	}

	std::string SafePartFileDirectory(std::string const& value)
	{
		if (value.empty()) return {};
		std::filesystem::path const path{ value };
		if (path.is_absolute() || path.has_root_path()) return {};
		for (auto const& component : path)
		{
			if (component == "..") return {};
		}
		return path.generic_string();
	}

	void ApplyPerTorrentSettings(lt::add_torrent_params& params, OpenNet::Core::TorrentSettings const& settings)
	{
		params.storage_mode = settings.preallocateStorage ? lt::storage_mode_allocate : lt::storage_mode_sparse;
		params.part_file_dir = SafePartFileDirectory(settings.partFileDirectory);
		if (settings.disableV1HashesForHybrid)
			params.flags |= lt::torrent_flags::disable_v1_hashes;
		else
			params.flags &= ~lt::torrent_flags::disable_v1_hashes;
	}

	std::string HexDigest(lt::sha256_hash const& digest)
	{
		static constexpr char digits[] = "0123456789abcdef";
		std::string result;
		result.reserve(static_cast<std::size_t>(digest.size()) * 2);
		for (auto const byte : digest)
		{
			result.push_back(digits[byte >> 4]);
			result.push_back(digits[byte & 0x0f]);
		}
		return result;
	}

	void FillTaskInfoHashes(OpenNet::Core::Torrent::TaskMetadata& metadata, lt::info_hash_t const& hashes)
	{
		if (hashes.has_v1())
		{
			std::ostringstream stream;
			stream << hashes.v1;
			metadata.infoHashV1 = stream.str();
		}
		if (hashes.has_v2())
		{
			std::ostringstream stream;
			stream << hashes.v2;
			metadata.infoHashV2 = stream.str();
		}
	}

	OpenNet::Core::Torrent::TaskSettingsMetadata PersistedTaskSettings(std::string const& taskId, OpenNet::Core::Torrent::LibtorrentHandle::TorrentTaskSettings const& settings)
	{
		return {
			taskId,
			settings.downloadLimit,
			settings.uploadLimit,
			settings.minimumUploadRate,
			settings.maxConnections,
			settings.maxUploads,
			settings.enableDht,
			settings.enableLsd,
			settings.enablePex,
			settings.applyIpFilter,
			settings.sequentialDownload,
			settings.superSeeding,
			settings.forceStart,
			settings.uploadMode,
			settings.shareMode,
			settings.shareRatioLimit,
			settings.seedingTimeLimit,
			settings.inactiveSeedingTimeLimit,
			settings.shareLimitMatchAll,
			settings.shareLimitAction,
			settings.completionAction };
	}

	OpenNet::Core::Torrent::LibtorrentHandle::TorrentTaskSettings RuntimeTaskSettings(OpenNet::Core::Torrent::TaskSettingsMetadata const& settings)
	{
		return {
			settings.downloadLimit,
			settings.uploadLimit,
			settings.minimumUploadRate,
			settings.maxConnections,
			settings.maxUploads,
			settings.enableDht,
			settings.enableLsd,
			settings.enablePex,
			settings.applyIpFilter,
			settings.sequentialDownload,
			settings.superSeeding,
			settings.forceStart,
			settings.uploadMode,
			settings.shareMode,
			settings.shareRatioLimit,
			settings.seedingTimeLimit,
			settings.inactiveSeedingTimeLimit,
			settings.shareLimitMatchAll,
			settings.shareLimitAction,
			settings.completionAction };
	}
}

namespace OpenNet::Core::Torrent
{
	struct LibtorrentHandle::Impl
	{
		std::unique_ptr<lt::session> m_session;
		std::optional<lt::session_proxy> m_sessionProxy;
		std::atomic<bool> m_running{ false };
		std::thread m_thread;

		std::mutex m_cbMutex;
		ProgressCallback m_progressCb;
		FinishedCallback m_finishedCb;
		ErrorCallback m_errorCb;

		std::atomic<bool> m_stopRequested{ false };
		std::atomic<int> m_pendingResumeDataCount{ 0 };
		std::unordered_map<std::string, lt::torrent_handle> m_taskIdToHandle;
		std::unordered_map<lt::torrent_handle, std::string,
			std::hash<lt::torrent_handle>> m_handleToTaskId;
		mutable std::mutex m_torrentMapMutex;

		struct LongSeedSession
		{
			lt::torrent_handle handle;
			std::string infoHashV2;
		};
		std::unordered_map<std::string, LongSeedSession> m_longSeedSessions;
		mutable std::mutex m_longSeedMutex;
		std::mutex m_addRemoveMutex;
		std::mutex m_taskCommandMutex;
		std::mutex m_detailRequestMutex;
		std::unordered_map<lt::torrent_handle,
			std::array<std::chrono::steady_clock::time_point, 5>,
			std::hash<lt::torrent_handle>> m_detailRequests;
		enum class RecheckCompletionAction
		{
			Pause,
			Resume,
		};
		std::unordered_map<std::string, RecheckCompletionAction> m_recheckCompletionActions;
		std::mutex m_completionMoveMutex;
		std::unordered_map<std::string, bool> m_completionMoves;
		std::mutex m_pendingDeleteMutex;
		std::unordered_map<std::string, std::string> m_pendingDeleteByHash;
		struct PersistedProgress
		{
			std::int64_t downloadedSize{};
			std::chrono::steady_clock::time_point timestamp{};
		};
		std::unordered_map<std::string, PersistedProgress> m_persistedProgress;
		std::mutex m_progressPersistenceMutex;
		TorrentStateManager* m_stateManager{ nullptr };
		mutable std::mutex m_peerEventMutex;
		std::unordered_map<std::string, std::deque<PeerConnectionEvent>> m_peerEvents;
		mutable std::mutex m_trackerLogMutex;
		std::unordered_map<std::string,
			std::unordered_map<std::string, std::deque<TrackerLogEntry>>>
			m_trackerLogs;
		mutable std::mutex m_filePrioritiesMutex;
		std::unordered_map<
			lt::torrent_handle,
			std::vector<lt::download_priority_t>,
			std::hash<lt::torrent_handle>> m_filePrioritiesCache;
		mutable std::mutex m_peerSnapshotMutex;
		std::unordered_map<lt::torrent_handle, std::vector<TorrentPeerInfo>,
			std::hash<lt::torrent_handle>> m_peerSnapshots;
		mutable std::mutex m_fileProgressMutex;
		std::unordered_map<lt::torrent_handle, std::vector<std::int64_t>,
			std::hash<lt::torrent_handle>> m_fileProgressCache;
		mutable std::mutex m_pieceAvailabilityMutex;
		std::unordered_map<lt::torrent_handle, std::vector<int>,
			std::hash<lt::torrent_handle>> m_pieceAvailabilityCache;
		mutable std::mutex m_trackerSnapshotMutex;
		std::unordered_map<lt::torrent_handle, std::vector<TorrentTrackerInfo>,
			std::hash<lt::torrent_handle>> m_trackerSnapshots;
		struct RateConstraints
		{
			int downloadLimit{};
			int uploadLimit{};
			int minimumUploadRate{};
		};
		mutable std::mutex m_rateConstraintsMutex;
		std::unordered_map<lt::torrent_handle, RateConstraints, std::hash<lt::torrent_handle>> m_rateConstraints;
		mutable std::mutex m_taskSettingsCacheMutex;
		// Read-heavy and normally small. Contiguous storage is a better fit than
		// allocating one node per task; mutations already occur under the mutex.
		std::flat_map<std::string, TaskSettingsMetadata> m_taskSettingsCache;
		struct CachedTorrentMetadata
		{
			std::string infoHash;
			std::string infoHashV1;
			std::string infoHashV2;
			std::string apiHash;
			std::string comment;
			std::string creator;
			std::int64_t creationTimestamp{};
			int pieceSize{};
			int piecesNum{};
			bool isPrivate{};
			bool isPieceAligned{};
			bool metadataLoaded{};
			std::vector<std::string> pieceHashes;
		};
		mutable std::mutex m_torrentMetadataMutex;
		std::unordered_map<lt::torrent_handle, CachedTorrentMetadata, std::hash<lt::torrent_handle>> m_torrentMetadataCache;

		std::atomic<int> m_cachedDhtNodeCount{ 0 };
		std::atomic<std::int64_t> m_internalBanCount{ 0 };
		mutable std::mutex m_portMappingMutex;
		PortMappingStatus m_portMappingStatus;
		mutable std::mutex m_listenStateMutex;
		std::string m_lastListenError;
		std::string m_ipv4ListenError;
		std::string m_ipv6ListenError;
		int m_ipv4ListenPort{};
		int m_ipv6ListenPort{};
		int m_ipv4TcpListenPort{};
		int m_ipv4UtpListenPort{};
		int m_ipv6TcpListenPort{};
		int m_ipv6UtpListenPort{};

		mutable std::mutex m_sessionStatsMutex;
		std::int64_t m_sessionTotalDownload{};
		std::int64_t m_sessionTotalUpload{};
		std::int64_t m_sessionDiskBlocksInUse{};
		std::int64_t m_sessionDhtBytesReceived{};
		std::int64_t m_sessionDhtBytesSent{};
		std::int64_t m_cachedDownloadRate{};
		std::int64_t m_cachedUploadRate{};
		std::int64_t m_cachedLongTermSeedingUploadRate{};
		struct CachedTorrentRate
		{
			std::int64_t downloadRate{};
			std::int64_t uploadRate{};
			std::int64_t totalDone{};
			std::int64_t totalUpload{};
			int numPeers{};
			int numSeeds{};
			lt::torrent_status::state_t state{};
			bool isPaused{};
			bool hasError{};
			bool isFinished{};
		};
		std::unordered_map<lt::torrent_handle, CachedTorrentRate,
			std::hash<lt::torrent_handle>> m_cachedTorrentRates;
		std::unordered_map<std::string, std::int64_t> m_sessionMetricValues;
		int m_sessionStatsMetricIdxRecvBytes{ -1 };
		int m_sessionStatsMetricIdxSentBytes{ -1 };
		int m_sessionStatsMetricIdxDhtNodes{ -1 };
		int m_sessionStatsMetricIdxDiskBlocksInUse{ -1 };
		int m_sessionStatsMetricIdxDhtBytesReceived{ -1 };
		int m_sessionStatsMetricIdxDhtBytesSent{ -1 };
		bool m_sessionStatsMetricsResolved{ false };

		std::chrono::steady_clock::time_point m_lastTorrentUpdateRequest{ std::chrono::steady_clock::now() };
		std::chrono::steady_clock::time_point m_lastSharePolicyCheck{ std::chrono::steady_clock::now() };
		struct UploadActivity
		{
			std::int64_t uploaded{};
			std::chrono::steady_clock::time_point lastChange{};
		};
		std::mutex m_uploadActivityMutex;
		std::unordered_map<lt::torrent_handle, UploadActivity,
			std::hash<lt::torrent_handle>> m_uploadActivity;
		struct SharePolicySnapshot
		{
			lt::torrent_handle handle;
			lt::torrent_flags_t flags;
			std::int64_t all_time_download{};
			std::int64_t all_time_upload{};
			std::chrono::seconds seeding_duration{};
			std::chrono::steady_clock::time_point observedAt{};
			bool is_finished{};
			bool is_seeding{};
		};
		std::mutex m_sharePolicySnapshotMutex;
		std::unordered_map<lt::torrent_handle, SharePolicySnapshot,
			std::hash<lt::torrent_handle>> m_sharePolicySnapshots;
		std::chrono::steady_clock::time_point m_lastStatsRequest{
			std::chrono::steady_clock::now() };
		std::chrono::steady_clock::time_point m_lastDhtStateSave{
			std::chrono::steady_clock::now() };
		std::chrono::steady_clock::time_point m_lastResumeCheckpoint{
			std::chrono::steady_clock::now() };
		std::mutex m_resumeRetryMutex;
		std::unordered_set<std::string> m_resumeRetryTasks;
		std::chrono::steady_clock::time_point m_lastDhtBootstrapRetry{
			std::chrono::steady_clock::now() };
		int m_dhtBootstrapAttempts{};
		std::chrono::steady_clock::time_point m_lastClientFilterCheck{
			std::chrono::steady_clock::now() };
		std::chrono::steady_clock::time_point m_lastIpFilterMaintenance{
			std::chrono::steady_clock::now() };
	};

#define m_session m_impl->m_session
#define m_sessionProxy m_impl->m_sessionProxy
#define m_running m_impl->m_running
#define m_thread m_impl->m_thread
#define m_cbMutex m_impl->m_cbMutex
#define m_progressCb m_impl->m_progressCb
#define m_finishedCb m_impl->m_finishedCb
#define m_errorCb m_impl->m_errorCb
#define m_stopRequested m_impl->m_stopRequested
#define m_pendingResumeDataCount m_impl->m_pendingResumeDataCount
#define m_taskIdToHandle m_impl->m_taskIdToHandle
#define m_handleToTaskId m_impl->m_handleToTaskId
#define m_torrentMapMutex m_impl->m_torrentMapMutex
#define m_longSeedSessions m_impl->m_longSeedSessions
#define m_longSeedMutex m_impl->m_longSeedMutex
#define m_addRemoveMutex m_impl->m_addRemoveMutex
#define m_taskCommandMutex m_impl->m_taskCommandMutex
#define m_detailRequestMutex m_impl->m_detailRequestMutex
#define m_detailRequests m_impl->m_detailRequests
#define m_recheckCompletionActions m_impl->m_recheckCompletionActions
#define m_completionMoveMutex m_impl->m_completionMoveMutex
#define m_completionMoves m_impl->m_completionMoves
#define m_pendingDeleteMutex m_impl->m_pendingDeleteMutex
#define m_pendingDeleteByHash m_impl->m_pendingDeleteByHash
#define m_persistedProgress m_impl->m_persistedProgress
#define m_progressPersistenceMutex m_impl->m_progressPersistenceMutex
#define m_stateManager m_impl->m_stateManager
#define m_peerEventMutex m_impl->m_peerEventMutex
#define m_peerEvents m_impl->m_peerEvents
#define m_trackerLogMutex m_impl->m_trackerLogMutex
#define m_trackerLogs m_impl->m_trackerLogs
#define m_filePrioritiesMutex m_impl->m_filePrioritiesMutex
#define m_filePrioritiesCache m_impl->m_filePrioritiesCache
#define m_peerSnapshotMutex m_impl->m_peerSnapshotMutex
#define m_peerSnapshots m_impl->m_peerSnapshots
#define m_fileProgressMutex m_impl->m_fileProgressMutex
#define m_fileProgressCache m_impl->m_fileProgressCache
#define m_pieceAvailabilityMutex m_impl->m_pieceAvailabilityMutex
#define m_pieceAvailabilityCache m_impl->m_pieceAvailabilityCache
#define m_trackerSnapshotMutex m_impl->m_trackerSnapshotMutex
#define m_trackerSnapshots m_impl->m_trackerSnapshots
#define m_rateConstraintsMutex m_impl->m_rateConstraintsMutex
#define m_rateConstraints m_impl->m_rateConstraints
#define m_taskSettingsCacheMutex m_impl->m_taskSettingsCacheMutex
#define m_taskSettingsCache m_impl->m_taskSettingsCache
#define m_torrentMetadataMutex m_impl->m_torrentMetadataMutex
#define m_torrentMetadataCache m_impl->m_torrentMetadataCache
#define m_cachedDhtNodeCount m_impl->m_cachedDhtNodeCount
#define m_internalBanCount m_impl->m_internalBanCount
#define m_portMappingMutex m_impl->m_portMappingMutex
#define m_portMappingStatus m_impl->m_portMappingStatus
#define m_listenStateMutex m_impl->m_listenStateMutex
#define m_lastListenError m_impl->m_lastListenError
#define m_ipv4ListenError m_impl->m_ipv4ListenError
#define m_ipv6ListenError m_impl->m_ipv6ListenError
#define m_ipv4ListenPort m_impl->m_ipv4ListenPort
#define m_ipv6ListenPort m_impl->m_ipv6ListenPort
#define m_ipv4TcpListenPort m_impl->m_ipv4TcpListenPort
#define m_ipv4UtpListenPort m_impl->m_ipv4UtpListenPort
#define m_ipv6TcpListenPort m_impl->m_ipv6TcpListenPort
#define m_ipv6UtpListenPort m_impl->m_ipv6UtpListenPort
#define m_sessionStatsMutex m_impl->m_sessionStatsMutex
#define m_sessionTotalDownload m_impl->m_sessionTotalDownload
#define m_sessionTotalUpload m_impl->m_sessionTotalUpload
#define m_sessionDiskBlocksInUse m_impl->m_sessionDiskBlocksInUse
#define m_sessionDhtBytesReceived m_impl->m_sessionDhtBytesReceived
#define m_sessionDhtBytesSent m_impl->m_sessionDhtBytesSent
#define m_cachedDownloadRate m_impl->m_cachedDownloadRate
#define m_cachedUploadRate m_impl->m_cachedUploadRate
#define m_cachedLongTermSeedingUploadRate m_impl->m_cachedLongTermSeedingUploadRate
#define m_cachedTorrentRates m_impl->m_cachedTorrentRates
#define m_sessionMetricValues m_impl->m_sessionMetricValues
#define m_sessionStatsMetricIdxRecvBytes m_impl->m_sessionStatsMetricIdxRecvBytes
#define m_sessionStatsMetricIdxSentBytes m_impl->m_sessionStatsMetricIdxSentBytes
#define m_sessionStatsMetricIdxDhtNodes m_impl->m_sessionStatsMetricIdxDhtNodes
#define m_sessionStatsMetricIdxDiskBlocksInUse m_impl->m_sessionStatsMetricIdxDiskBlocksInUse
#define m_sessionStatsMetricIdxDhtBytesReceived m_impl->m_sessionStatsMetricIdxDhtBytesReceived
#define m_sessionStatsMetricIdxDhtBytesSent m_impl->m_sessionStatsMetricIdxDhtBytesSent
#define m_sessionStatsMetricsResolved m_impl->m_sessionStatsMetricsResolved
#define m_lastTorrentUpdateRequest m_impl->m_lastTorrentUpdateRequest
#define m_lastSharePolicyCheck m_impl->m_lastSharePolicyCheck
#define m_uploadActivityMutex m_impl->m_uploadActivityMutex
#define m_uploadActivity m_impl->m_uploadActivity
#define m_sharePolicySnapshotMutex m_impl->m_sharePolicySnapshotMutex
#define m_sharePolicySnapshots m_impl->m_sharePolicySnapshots
#define m_lastStatsRequest m_impl->m_lastStatsRequest
#define m_lastDhtStateSave m_impl->m_lastDhtStateSave
#define m_lastResumeCheckpoint m_impl->m_lastResumeCheckpoint
#define m_resumeRetryMutex m_impl->m_resumeRetryMutex
#define m_resumeRetryTasks m_impl->m_resumeRetryTasks
#define m_lastDhtBootstrapRetry m_impl->m_lastDhtBootstrapRetry
#define m_dhtBootstrapAttempts m_impl->m_dhtBootstrapAttempts
#define m_lastClientFilterCheck m_impl->m_lastClientFilterCheck
#define m_lastIpFilterMaintenance m_impl->m_lastIpFilterMaintenance
#define RecheckCompletionAction Impl::RecheckCompletionAction
#define PersistedProgress Impl::PersistedProgress
#define CachedTorrentRate Impl::CachedTorrentRate

	namespace
	{
		std::optional<lt::tcp::endpoint> TcpEndpoint(
			lt::peer_endpoint_t const& endpoint)
		{
			if (auto const value = std::get_if<
				lt::aux::noexcept_movable<lt::tcp::endpoint>>(&endpoint))
			{
				return static_cast<lt::tcp::endpoint const&>(*value);
			}
			return std::nullopt;
		}

		LibtorrentHandle::TorrentPeerInfo ConvertPeerInfo(
			lt::peer_info const& peer)
		{
			LibtorrentHandle::TorrentPeerInfo value;
			value.isI2p = bool(peer.flags & lt::peer_info::i2p_socket);
			value.isUtp = bool(peer.flags & lt::peer_info::utp_socket);
			value.isTls = bool(peer.flags & lt::peer_info::ssl_socket);
#if TORRENT_USE_I2P
			if (value.isI2p)
			{
				value.ip = HexDigest(peer.i2p_destination());
				value.port = 0;
			}
			else
#endif
			{
				auto const endpoint = peer.remote_endpoint();
				value.ip = endpoint.address().to_string();
				value.port = endpoint.port();
			}
			value.client = peer.client;
			value.downloadRateKB = static_cast<int>(peer.down_speed / 1000);
			value.uploadRateKB = static_cast<int>(peer.up_speed / 1000);
			value.totalDownloaded = peer.total_download;
			value.totalUploaded = peer.total_upload;
			value.progress = peer.progress;
			value.flags = static_cast<std::uint32_t>(peer.flags);
			value.connectionType = static_cast<int>(
				static_cast<std::uint8_t>(peer.connection_type));
			value.source = static_cast<int>(
				static_cast<std::uint8_t>(peer.source));
			if (peer.source & lt::peer_info::tracker) value.sourceDescription += "Tracker ";
			if (peer.source & lt::peer_info::dht) value.sourceDescription += "DHT ";
			if (peer.source & lt::peer_info::pex) value.sourceDescription += "PEX ";
			if (peer.source & lt::peer_info::lsd) value.sourceDescription += "LSD ";
			if (peer.source & lt::peer_info::resume_data) value.sourceDescription += "Resume ";
			if (peer.source & lt::peer_info::incoming) value.sourceDescription += "Incoming ";
			if (!value.sourceDescription.empty()) value.sourceDescription.pop_back();
			value.isIncoming = bool(peer.source & lt::peer_info::incoming);
			value.isConnecting = bool(peer.flags
				& (lt::peer_info::connecting | lt::peer_info::handshake));
			value.isInteresting = bool(peer.flags & lt::peer_info::interesting);
			value.isChoked = bool(peer.flags & lt::peer_info::choked);
			value.isRemoteInterested = bool(peer.flags & lt::peer_info::remote_interested);
			value.isRemoteChoked = bool(peer.flags & lt::peer_info::remote_choked);
			value.isSeed = bool(peer.flags & lt::peer_info::seed);
			value.isOptimisticUnchoke = bool(peer.flags & lt::peer_info::optimistic_unchoke);
			value.isSnubbed = bool(peer.flags & lt::peer_info::snubbed);
			value.isRc4Encrypted = bool(peer.flags & lt::peer_info::rc4_encrypted);
			value.isPlaintextEncrypted = bool(peer.flags & lt::peer_info::plaintext_encrypted);
			value.downloadingPiece = static_cast<int>(peer.downloading_piece_index);
			return value;
		}

		std::optional<int> NextTrackerAnnounceSeconds(
			lt::torrent_handle const& handle,
			std::string_view const url)
		{
			try
			{
				auto const now = lt::time_point_cast<lt::seconds32>(
					lt::clock_type::now());
				std::optional<int> result;
				for (auto const& tracker : handle.trackers())
				{
					if (tracker.url != url) continue;
					for (auto const& endpoint : tracker.endpoints)
					{
						for (auto const& hash : endpoint.info_hashes)
						{
							if (hash.next_announce == (lt::time_point32::min)()
								|| hash.next_announce == (lt::time_point32::max)())
								continue;
							auto const remaining = static_cast<int>(std::max<
																	std::int64_t>(0, lt::total_seconds(
																		hash.next_announce - now)));
							result = result
								? std::min(*result, remaining)
								: remaining;
						}
					}
				}
				return result;
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		std::string FormatTrackerInterval(int seconds)
		{
			auto const hours = seconds / 3600;
			seconds %= 3600;
			auto const minutes = seconds / 60;
			seconds %= 60;
			return std::format("{}:{:02}:{:02}", hours, minutes, seconds);
		}

		std::vector<std::string> TrackersForNewTask(
			std::vector<std::string> const& taskTrackers)
		{
			std::vector<std::string> result = taskTrackers;
			auto& manager = TrackerManager::Instance();
			if (manager.AutoAddToNewTorrents())
			{
				for (auto const& tracker : manager.GetEnabledTrackers())
				{
					auto url = winrt::to_string(winrt::hstring{ tracker.url });
					if (!url.empty())
					{
						result.push_back(std::move(url));
					}
				}
			}

			std::vector<std::string> unique;
			std::unordered_set<std::string> seen;
			for (auto const& url : result)
			{
				if (!url.empty() && seen.insert(url).second)
				{
					unique.push_back(url);
				}
			}
			return unique;
		}

		bool IsLibtorrentError(
			lt::error_code const& error,
			lt::errors::error_code_enum value)
		{
			return error
				&& error.category() == lt::libtorrent_category()
				&& error.value() == static_cast<int>(value);
		}

		std::string PeerDisconnectReason(
			lt::peer_disconnected_alert const& alert)
		{
			if (alert.reason == lt::close_reason_t::upload_to_upload
				|| IsLibtorrentError(
					alert.error, lt::errors::upload_upload_connection)
				|| IsLibtorrentError(alert.error, lt::errors::torrent_finished))
			{
				return "both_finished";
			}
			if (alert.reason == lt::close_reason_t::blocked
				|| IsLibtorrentError(
					alert.error, lt::errors::banned_by_ip_filter))
			{
				return "ip_filter";
			}
			if (IsLibtorrentError(alert.error, lt::errors::peer_banned)
				|| IsLibtorrentError(
					alert.error, lt::errors::too_many_corrupt_pieces))
			{
				return "anti_leech";
			}
			if (alert.error)
				return alert.error.message();

			switch (alert.reason)
			{
				case lt::close_reason_t::duplicate_peer_id:
					return "duplicate_peer_id";
				case lt::close_reason_t::torrent_removed:
					return "torrent_removed";
				case lt::close_reason_t::no_memory:
					return "no_memory";
				case lt::close_reason_t::port_blocked:
					return "port_blocked";
				case lt::close_reason_t::not_interested_upload_only:
					return "not_interested_upload_only";
				case lt::close_reason_t::timeout:
					return "timeout";
				case lt::close_reason_t::timed_out_interest:
					return "timed_out_interest";
				case lt::close_reason_t::timed_out_activity:
					return "timed_out_activity";
				case lt::close_reason_t::timed_out_handshake:
					return "timed_out_handshake";
				case lt::close_reason_t::timed_out_request:
					return "timed_out_request";
				case lt::close_reason_t::protocol_blocked:
					return "protocol_blocked";
				case lt::close_reason_t::peer_churn:
					return "peer_churn";
				case lt::close_reason_t::too_many_connections:
					return "too_many_connections";
				case lt::close_reason_t::too_many_files:
					return "too_many_files";
				default:
					return "connection_closed";
			}
		}

		void ApplyTrackers(
			lt::torrent_handle const& handle,
			std::vector<std::string> const& taskTrackers)
		{
			if (!handle.is_valid())
			{
				return;
			}

			std::unordered_set<std::string> existing;
			for (auto const& tracker : handle.trackers())
			{
				existing.insert(tracker.url);
			}
			for (auto const& url : TrackersForNewTask(taskTrackers))
			{
				if (existing.insert(url).second)
				{
					handle.add_tracker(lt::announce_entry(url));
				}
			}
		}

		std::wstring SafeTorrentFileStem(lt::torrent_info const& info)
		{
			std::wstring stem = winrt::to_hstring(info.name()).c_str();
			if (stem.empty())
			{
				std::ostringstream hashText;
				hashText << info.info_hashes();
				stem = winrt::to_hstring(hashText.str()).c_str();
			}
			for (auto& ch : stem)
			{
				if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"'
					|| ch == L'/' || ch == L'\\' || ch == L'|'
					|| ch == L'?' || ch == L'*' || ch < L' ')
				{
					ch = L'_';
				}
			}
			while (!stem.empty() && (stem.back() == L'.' || stem.back() == L' '))
			{
				stem.pop_back();
			}
			return stem.empty() ? L"torrent" : stem;
		}

		bool HasTorrentContentDirectory(lt::torrent_info const& info)
		{
			auto const& files = info.layout();
			for (auto const index : files.file_range())
			{
				auto const path = files.file_path(index);
				if (path.find('/') != std::string::npos
					|| path.find('\\') != std::string::npos)
				{
					return true;
				}
			}
			return false;
		}

		void WriteTorrentFile(
			lt::torrent_handle const& handle,
			std::string const& taskId,
			std::string const& downloadPath,
			bool copyToDownloadDirectory)
		{
			if (!handle.is_valid())
			{
				return;
			}
			auto info = handle.torrent_file();
			if (!info || !info->is_valid())
			{
				return;
			}

			try
			{
				auto params = handle.get_resume_data(
					lt::torrent_handle::save_info_dict);
				auto bytes = lt::write_torrent_file_buf(
					params, lt::write_flags::allow_missing_piece_layer);
				auto appDataDirectory = std::filesystem::path(
					winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW())
					/ L"Torrents";
				std::filesystem::create_directories(appDataDirectory);
				auto const userFileName = SafeTorrentFileStem(*info) + L".torrent";
				auto const cacheFileName = winrt::to_hstring(taskId) + L".torrent";
				auto appDataFile = appDataDirectory / cacheFileName.c_str();
				auto temporaryFile = appDataFile;
				temporaryFile += L".tmp";

				std::ofstream output(temporaryFile, std::ios::binary | std::ios::trunc);
				output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
				output.flush();
				if (!output.good())
					throw std::runtime_error("failed to write torrent metadata cache");
				output.close();
				if (!::MoveFileExW(
					temporaryFile.c_str(), appDataFile.c_str(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				{
					std::error_code cleanupError;
					std::filesystem::remove(temporaryFile, cleanupError);
					throw std::system_error(
						static_cast<int>(::GetLastError()),
						std::system_category(),
						"failed to replace torrent metadata cache");
				}

				if (copyToDownloadDirectory && !downloadPath.empty())
				{
					auto targetDirectory = std::filesystem::path(
						winrt::to_hstring(downloadPath).c_str());
					auto& settingsDb =
						::OpenNet::Core::AppSettingsDatabase::Instance();
					settingsDb.Initialize();
					auto const location = settingsDb.GetInt(
						::OpenNet::Core::AppSettingsDatabase::CAT_TORRENT,
						"torrentCopyLocation").value_or(0);
					if (location == 1 && HasTorrentContentDirectory(*info))
					{
						targetDirectory /= SafeTorrentFileStem(*info);
					}
					std::filesystem::create_directories(targetDirectory);
					auto const targetFile = targetDirectory / userFileName;
					auto const allowedOverwrite = settingsDb.GetStringW(
						::OpenNet::Core::AppSettingsDatabase::CAT_TORRENT,
						"torrentCopyOverwritePath");
					auto const mayOverwrite = allowedOverwrite
						&& std::filesystem::path(*allowedOverwrite).lexically_normal()
						== targetFile.lexically_normal();
					// Consume the one-shot authorization before touching the target.
					// A failed copy must prompt again instead of authorizing a later task.
					if (mayOverwrite)
					{
						settingsDb.Delete(
							::OpenNet::Core::AppSettingsDatabase::CAT_TORRENT,
							"torrentCopyOverwritePath");
					}
					std::filesystem::copy_file(
						appDataFile,
						targetFile,
						mayOverwrite
						? std::filesystem::copy_options::overwrite_existing
						: std::filesystem::copy_options::none);
				}
			}
			catch (std::exception const& ex)
			{
				OutputDebugStringA((
					"LibtorrentHandle: Failed to persist .torrent metadata: "
					+ std::string(ex.what()) + "\n").c_str());
			}
		}
	}

	LibtorrentHandle::LibtorrentHandle()
		: m_impl(std::make_unique<Impl>())
	{
	}

	void LibtorrentHandle::SetStateManager(
		TorrentStateManager* stateManager) noexcept
	{
		m_stateManager = stateManager;
	}

	bool LibtorrentHandle::IsRunning() const noexcept
	{
		return m_running.load();
	}
	LibtorrentHandle::~LibtorrentHandle()
	{
		// If still running, save resume data and stop gracefully
		if (m_running.load() && m_session)
		{
			SaveAllResumeData();
		}
		Stop();
	}

	bool LibtorrentHandle::Initialize()
	{
		if (m_session)
			return true; // 已初始化
		try
		{
			lt::settings_pack currentSettings;
			ConfigureDefaultSettings(currentSettings);

			// Restore session_params before constructing the session. In
			// particular, this is how libtorrent restores the saved DHT node
			// IDs and routing-table bootstrap nodes.
			if (m_stateManager)
			{
				if (auto savedData = m_stateManager->LoadSessionStateData())
				{
					lt::span<char const> buffer(
						reinterpret_cast<char const*>(savedData->data()),
						savedData->size());
					auto savedParams = lt::read_session_params(buffer);
					// Session-state blobs from older launches may contain stale
					// listen/DHT settings. The dedicated TorrentSettings store
					// remains authoritative.
					savedParams.settings = std::move(currentSettings);
					savedParams.disk_io_constructor = lt::pread_disk_io_constructor;
					m_session = std::make_unique<lt::session>(
						std::move(savedParams));
				}
			}
			if (!m_session)
			{
				lt::session_params params;
				params.settings = std::move(currentSettings);
				params.disk_io_constructor = lt::pread_disk_io_constructor;
				m_session = std::make_unique<lt::session>(std::move(params));
			}
			if (m_session->get_settings().get_bool(lt::settings_pack::enable_dht))
			{
				// Keep several independent bootstrap routes. The settings entry is
				// still user-configurable; these routers are startup fallbacks for a
				// missing, stale, or partially unreachable saved routing table.
				AddFallbackDhtRouters(*m_session);
				m_dhtBootstrapAttempts = 1;
			}
			OutputDebugStringA(std::format(
				"libtorrent header={}, runtime={}, ABI={}, Torrent_Use_ASSERTS={}, sizeof(add_torrent_params)={}, iterator_debug={}\n",
				LIBTORRENT_VERSION,
				lt::version(),
				TORRENT_ABI_VERSION,
				TORRENT_USE_ASSERTS,
				sizeof(lt::add_torrent_params),
				_ITERATOR_DEBUG_LEVEL).c_str());
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA((std::string("LibtorrentHandle: Session initialization failed: ") + ex.what() + "\n").c_str());
			std::lock_guard lk(m_cbMutex);
			if (m_errorCb)
				m_errorCb(std::string("Session init error: ") + ex.what());
			return false;
		}
		return true;
	}

	template<typename TSettingsPack>
	void LibtorrentHandle::ConfigureDefaultSettings(TSettingsPack& pack)
	{
		// Load persistent settings and apply to pack
		auto& settingsMgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
		settingsMgr.Load();
		auto settings = settingsMgr.Get();

		// Use the shared builder that maps TorrentSettings -> settings_pack
		ApplyTorrentSettings(settings, pack);

		if (settings.enableDht)
		{
			// A saved routing table may be absent or stale. Explicit bootstrap
			// routers make a fresh application start converge instead of
			// leaving the displayed DHT node count at zero indefinitely.
			pack.set_str(
				lt::settings_pack::dht_bootstrap_nodes,
				settings.dhtBootstrapNodes.empty()
				? "dht.libtorrent.org:25401,"
				"dht.transmissionbt.com:6881,"
				"router.bt.ouinet.work:6881"
				: settings.dhtBootstrapNodes);
		}
	}

	bool LibtorrentHandle::Start()
	{
		if (!Initialize())
			return false;
		if (m_running.load())
			return true;
		m_stopRequested = false;
		m_running.store(true);
		try
		{
			// Do not report the core as running unless the alert worker was
			// actually created. A failed std::thread construction previously left
			// IsRunning() true with no alert processing, which looked like a live
			// session with permanently stale DHT/listen state.
			m_thread = std::thread(&LibtorrentHandle::AlertLoop, this);
			// Prime the UI-visible counters immediately; the alert loop keeps
			// refreshing them at its normal two-second cadence.
			m_session->post_session_stats();
			m_session->post_dht_stats();
		}
		catch (std::exception const& ex)
		{
			m_stopRequested.store(true);
			m_running.store(false);
			OutputDebugStringA((
				"LibtorrentHandle: Failed to start alert loop: "
				+ std::string(ex.what()) + "\n").c_str());
			return false;
		}
		catch (...)
		{
			m_stopRequested.store(true);
			m_running.store(false);
			OutputDebugStringA("LibtorrentHandle: Failed to start alert loop\n");
			return false;
		}
		return true;
	}

	void LibtorrentHandle::Stop()
	{
		OutputDebugStringA("LibtorrentHandle: Stopping...\n");

		// Wait for all pending save_resume_data alerts to be processed
		// before signalling the alert loop to stop. This ensures resume
		// data is persisted before the session is torn down.
		if (m_session && m_pendingResumeDataCount.load() > 0)
		{
			OutputDebugStringA("LibtorrentHandle: Waiting for pending resume data saves...\n");
			auto start = std::chrono::steady_clock::now();
			constexpr auto kTimeout = std::chrono::seconds(10);

			while (m_pendingResumeDataCount.load() > 0)
			{
				auto elapsed = std::chrono::steady_clock::now() - start;
				if (elapsed > kTimeout)
				{
					OutputDebugStringA("LibtorrentHandle: Timeout waiting for resume data alerts\n");
					break;
				}
				std::this_thread::sleep_for(50ms);
			}
			if (m_pendingResumeDataCount.load() == 0)
				OutputDebugStringA("LibtorrentHandle: All resume data alerts received\n");
			{
				std::lock_guard retryLock(m_resumeRetryMutex);
				if (!m_resumeRetryTasks.empty())
					OutputDebugStringA("LibtorrentHandle: Some resume data writes remain unsaved\n");
			}
		}

		// Signal the alert loop to stop as soon as possible.
		m_stopRequested = true;
		// Wake the alert loop so it can observe the stop request promptly.
		// post_torrent_updates() only posts an alert when active torrents exist,
		// so we also call post_session_stats() which unconditionally posts a
		// session_stats_alert, guaranteeing wait_for_alert() returns immediately.
		if (m_session)
		{
			try
			{
				m_session->post_torrent_updates();
				m_session->post_session_stats();
			}
			catch (...)
			{
			}
		}

		if (m_running.exchange(false))
		{
			if (m_thread.joinable())
			{
				OutputDebugStringA("LibtorrentHandle: Waiting for alert thread to finish...\n");
				// AlertLoop is cooperatively stopped above and wait_for_alert() has
				// been explicitly woken. std::thread::join() has no timeout; keeping
				// a fake timeout loop around it only hid that fact.
				m_thread.join();
				OutputDebugStringA("LibtorrentHandle: Alert thread joined successfully\n");
			}
		}

		// Save session state and then clear session. Do this after the
		// alert thread has been joined so we don't contend for libtorrent
		// internal locks between threads.
		if (m_session && m_stateManager)
		{
			try
			{
				OutputDebugStringA("LibtorrentHandle: Saving session state\n");
				auto const state = lt::write_session_params_buf(
					m_session->session_state(), lt::session::save_dht_state);
				m_stateManager->SaveSessionState(
					std::vector<std::uint8_t>(state.begin(), state.end()));
			}
			catch (const std::exception& ex)
			{
				OutputDebugStringW((L"LibtorrentHandle: Error saving session: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
			}
		}

		{
			std::lock_guard lock(m_longSeedMutex);
			m_longSeedSessions.clear();
		}

		// Clear session using abort() + session_proxy for non-blocking shutdown.
		// session::abort() starts an asynchronous shutdown (notifying trackers etc.)
		// and returns a session_proxy. Destroying the session after abort() is
		// immediate. The proxy destructor will synchronize the background threads.
		if (m_session)
		{
			try
			{
				OutputDebugStringA("LibtorrentHandle: Aborting session (non-blocking)\n");
				m_sessionProxy = m_session->abort();
				m_session.reset();
			}
			catch (const std::exception& ex)
			{
				OutputDebugStringW((L"LibtorrentHandle: Error aborting session: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
			}
			catch (...)
			{
				OutputDebugStringA("LibtorrentHandle: Unknown error aborting session\n");
			}
		}

		OutputDebugStringA("LibtorrentHandle: Stop completed\n");
	}

	template<typename THashes>
	std::string LibtorrentHandle::FindExistingTaskId(
		THashes const& hashes) const
	{
		{
			std::lock_guard mapLock(m_torrentMapMutex);
			for (auto const& [taskId, handle] : m_taskIdToHandle)
			{
				if (!handle.is_valid()) continue;
				try
				{
					auto const existing = handle.info_hashes();
					if ((hashes.has_v1() && existing.has_v1()
						 && hashes.v1 == existing.v1)
						|| (hashes.has_v2() && existing.has_v2()
							&& hashes.v2 == existing.v2))
					{
						return taskId;
					}
				}
				catch (...)
				{
				}
			}
		}

		if (m_stateManager)
		{
			TaskMetadata identity;
			FillTaskInfoHashes(identity, hashes);
			if (auto const existing = m_stateManager->FindTaskIdByInfoHashes(
				identity.infoHashV1, identity.infoHashV2))
			{
				return *existing;
			}
		}
		return {};
	}

	template<typename THashes>
	std::string LibtorrentHandle::TakePendingDeleteTask(
		THashes const& hashes)
	{
		TaskMetadata identity;
		FillTaskInfoHashes(identity, hashes);
		std::lock_guard lock(m_pendingDeleteMutex);
		std::string taskId;
		for (auto const& hash : { identity.infoHashV1, identity.infoHashV2 })
		{
			if (hash.empty()) continue;
			if (auto const pending = m_pendingDeleteByHash.find(hash);
				pending != m_pendingDeleteByHash.end())
			{
				taskId = pending->second;
				break;
			}
		}
		if (!taskId.empty())
		{
			std::erase_if(m_pendingDeleteByHash, [&](auto const& item)
			{
				return item.second == taskId;
			});
		}
		return taskId;
	}

	LibtorrentHandle::AddTorrentResult LibtorrentHandle::AddMagnet(
		std::string const& magnetUri,
		std::string const& savePath,
		std::vector<int> const& filePriorities,
		std::vector<std::string> const& extraTrackers,
		bool startImmediately)
	{
		if (!Initialize())
			return { AddTorrentOutcome::Failed, {}, "Failed to initialize the torrent session" };
		try
		{
			lt::add_torrent_params atp = lt::parse_magnet_uri(magnetUri);
			std::scoped_lock addLock(m_addRemoveMutex);
			if (auto const existingTaskId = FindExistingTaskId(atp.info_hashes);
				!existingTaskId.empty())
			{
				return { AddTorrentOutcome::AlreadyExists, existingTaskId, {} };
			}
			atp.flags |= lt::torrent_flags::duplicate_is_error;
			atp.save_path = savePath; // 目标目录
			// Remove seed_mode flag for downloads
			atp.flags &= ~lt::torrent_flags::seed_mode;
			if (!startImmediately)
			{
				atp.flags &= ~lt::torrent_flags::auto_managed;
				atp.flags |= lt::torrent_flags::paused;
			}

			auto torrentSettings = ::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			ApplyPerTorrentSettings(atp, torrentSettings);

			if (!filePriorities.empty())
			{
				atp.file_priorities.clear();
				atp.file_priorities.reserve(filePriorities.size());
				for (int p : filePriorities)
				{
					atp.file_priorities.push_back(static_cast<lt::download_priority_t>(
						static_cast<std::uint8_t>(std::clamp(p, 0, 7))));
				}
			}

			// Generate the stable application task ID. Persist only after
			// libtorrent accepted the torrent, otherwise a failed/duplicate
			// add would leave a blank ghost task in the database.
			std::string taskId = TorrentStateManager::GenerateTaskId();

			auto const requestedFlags = atp.flags;
			atp.flags |= lt::torrent_flags::paused;
			atp.flags &= ~lt::torrent_flags::auto_managed;
			lt::error_code addError;
			lt::torrent_handle handle = m_session->add_torrent(atp, addError);
			if (addError || !handle.is_valid())
			{
				if (auto const existingTaskId = FindExistingTaskId(atp.info_hashes);
					!existingTaskId.empty())
				{
					return { AddTorrentOutcome::AlreadyExists, existingTaskId, {} };
				}
				return {
					AddTorrentOutcome::Failed,
					{},
					addError ? addError.message() : "libtorrent returned an invalid handle" };
			}

			if (m_stateManager)
			{
				TaskMetadata metadata;
				metadata.taskId = taskId;
				metadata.magnetUri = magnetUri;
				metadata.savePath = savePath;
				metadata.name = "";
				metadata.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now().time_since_epoch()).count();
				metadata.status = startImmediately ? 1 : 2;
				metadata.queuePosition = static_cast<int>(handle.queue_position());
				FillTaskInfoHashes(metadata, atp.info_hashes);
				if (!m_stateManager->SaveTaskMetadata(metadata))
				{
					m_session->remove_torrent(handle);
					if (auto const existingTaskId = FindExistingTaskId(atp.info_hashes);
						!existingTaskId.empty())
					{
						return { AddTorrentOutcome::AlreadyExists, existingTaskId, {} };
					}
					return { AddTorrentOutcome::Failed, {}, "Failed to persist torrent identity" };
				}
			}

			try { ApplyTrackers(handle, extraTrackers); }
			catch (std::exception const& error)
			{
				OutputDebugStringA(("Unable to apply extra trackers: " + std::string(error.what()) + "\n").c_str());
			}
			if (!atp.file_priorities.empty())
			{
				std::lock_guard lock(m_filePrioritiesMutex);
				m_filePrioritiesCache.insert_or_assign(handle, atp.file_priorities);
			}
			{
				std::lock_guard lock(m_rateConstraintsMutex);
				m_rateConstraints.insert_or_assign(handle, Impl::RateConstraints{ std::max(0, atp.download_limit), std::max(0, atp.upload_limit), 0 });
			}

			// Store mapping
			{
				std::lock_guard lk(m_torrentMapMutex);
				m_taskIdToHandle[taskId] = handle;
				m_handleToTaskId[handle] = taskId;
			}

			handle.set_flags(requestedFlags,
				lt::torrent_flags::paused | lt::torrent_flags::auto_managed);
			if (startImmediately) handle.resume();
			RequestResumeDataForTorrent(handle);
			return { AddTorrentOutcome::Added, taskId, {} };
		}
		catch (std::exception const& ex)
		{
			std::lock_guard lk(m_cbMutex);
			if (m_errorCb)
				m_errorCb(std::string("AddMagnet error: ") + ex.what());
			return { AddTorrentOutcome::Failed, {}, ex.what() };
		}
	}

	LibtorrentHandle::AddTorrentResult LibtorrentHandle::AddTorrentFile(
		std::string const& torrentFilePath,
		std::string const& savePath,
		std::vector<int> const& filePriorities,
		std::vector<std::string> const& extraTrackers,
		bool startImmediately,
		bool seedMode)
	{
		if (!Initialize())
			return { AddTorrentOutcome::Failed, {}, "Failed to initialize the torrent session" };
		try
		{
			// load_torrent_file() is the 2.1 API and also preserves the
			// top-level metadata (trackers, web seeds, comment and creator).
			auto torrentSettings = ::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			lt::load_torrent_limits limits;
			limits.max_directory_depth = std::clamp(torrentSettings.maxTorrentDirectoryDepth, 1, 1000);
			lt::add_torrent_params atp = lt::load_torrent_file(torrentFilePath, limits);
			if (!atp.ti)
				throw std::runtime_error("torrent file has no info dictionary");
			auto const torrentInfo = atp.ti;
			std::scoped_lock addLock(m_addRemoveMutex);
			if (auto const existingTaskId = FindExistingTaskId(torrentInfo->info_hashes());
				!existingTaskId.empty())
			{
				return { AddTorrentOutcome::AlreadyExists, existingTaskId, {} };
			}
			atp.flags |= lt::torrent_flags::duplicate_is_error;
			atp.save_path = savePath;
			if (seedMode)
				atp.flags |= lt::torrent_flags::seed_mode;
			else
				atp.flags &= ~lt::torrent_flags::seed_mode;
			if (!startImmediately)
			{
				atp.flags &= ~lt::torrent_flags::auto_managed;
				atp.flags |= lt::torrent_flags::paused;
			}

			ApplyPerTorrentSettings(atp, torrentSettings);

			if (!filePriorities.empty())
			{
				atp.file_priorities.clear();
				atp.file_priorities.reserve(filePriorities.size());
				for (int p : filePriorities)
				{
					atp.file_priorities.push_back(static_cast<lt::download_priority_t>(
						static_cast<std::uint8_t>(std::clamp(p, 0, 7))));
				}
			}

			// Generate the stable application task ID. Persist only after
			// libtorrent accepted the torrent.
			std::string taskId = TorrentStateManager::GenerateTaskId();

			auto const requestedFlags = atp.flags;
			atp.flags |= lt::torrent_flags::paused;
			atp.flags &= ~lt::torrent_flags::auto_managed;
			lt::error_code addError;
			lt::torrent_handle handle = m_session->add_torrent(atp, addError);
			if (addError || !handle.is_valid())
			{
				if (auto const existingTaskId = FindExistingTaskId(torrentInfo->info_hashes());
					!existingTaskId.empty())
				{
					return { AddTorrentOutcome::AlreadyExists, existingTaskId, {} };
				}
				return {
					AddTorrentOutcome::Failed,
					{},
					addError ? addError.message() : "libtorrent returned an invalid handle" };
			}

			if (m_stateManager)
			{
				TaskMetadata metadata;
				metadata.taskId = taskId;
				metadata.magnetUri = "";
				metadata.savePath = savePath;
				metadata.name = torrentInfo->name();
				metadata.totalSize = torrentInfo->total_size();
				metadata.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now().time_since_epoch()).count();
				metadata.status = startImmediately ? 1 : 2;
				metadata.queuePosition = static_cast<int>(handle.queue_position());
				FillTaskInfoHashes(metadata, torrentInfo->info_hashes());
				if (!m_stateManager->SaveTaskMetadata(metadata))
				{
					m_session->remove_torrent(handle);
					if (auto const existingTaskId = FindExistingTaskId(torrentInfo->info_hashes());
						!existingTaskId.empty())
					{
						return { AddTorrentOutcome::AlreadyExists, existingTaskId, {} };
					}
					return { AddTorrentOutcome::Failed, {}, "Failed to persist torrent identity" };
				}
			}

			try { ApplyTrackers(handle, extraTrackers); }
			catch (std::exception const& error)
			{
				OutputDebugStringA(("Unable to apply extra trackers: " + std::string(error.what()) + "\n").c_str());
			}
			if (!atp.file_priorities.empty())
			{
				std::lock_guard lock(m_filePrioritiesMutex);
				m_filePrioritiesCache.insert_or_assign(handle, atp.file_priorities);
			}
			{
				std::lock_guard lock(m_rateConstraintsMutex);
				m_rateConstraints.insert_or_assign(handle, Impl::RateConstraints{ std::max(0, atp.download_limit), std::max(0, atp.upload_limit), 0 });
			}

			// Store mapping
			{
				std::lock_guard lk(m_torrentMapMutex);
				m_taskIdToHandle[taskId] = handle;
				m_handleToTaskId[handle] = taskId;
			}

			auto& settingsDb = ::OpenNet::Core::AppSettingsDatabase::Instance();
			settingsDb.Initialize();
			WriteTorrentFile(
				handle,
				taskId,
				savePath,
				settingsDb.GetBool(
					::OpenNet::Core::AppSettingsDatabase::CAT_TORRENT,
					"saveTorrentCopyToDownloadDirectory").value_or(false));

			handle.set_flags(requestedFlags,
				lt::torrent_flags::paused | lt::torrent_flags::auto_managed);
			if (startImmediately) handle.resume();
			RequestResumeDataForTorrent(handle);
			return { AddTorrentOutcome::Added, taskId, {} };
		}
		catch (std::exception const& ex)
		{
			std::lock_guard lk(m_cbMutex);
			if (m_errorCb)
				m_errorCb(std::string("AddTorrentFile error: ") + ex.what());
			return { AddTorrentOutcome::Failed, {}, ex.what() };
		}
	}

	LibtorrentHandle::LongSeedSessionResult LibtorrentHandle::OpenLongSeedSession(
		std::string const& sessionId,
		std::vector<std::uint8_t> const& metainfo,
		std::filesystem::path const& localFilePath)
	{
		if (sessionId.empty())
			return { false, {}, "Long-seed session ID is empty" };
		if (metainfo.empty())
			return { false, {}, "Canonical torrent metadata is empty" };
		if (!std::filesystem::is_regular_file(localFilePath))
			return { false, {}, "Long-seed source file is unavailable" };
		if (!Initialize())
			return { false, {}, "Failed to initialize the torrent session" };

		try
		{
			{
				std::lock_guard lock(m_longSeedMutex);
				auto const existing = m_longSeedSessions.find(sessionId);
				if (existing != m_longSeedSessions.end()
					&& existing->second.handle.is_valid())
				{
					return { true, existing->second.infoHashV2, {} };
				}
			}

			lt::load_torrent_limits limits;
			limits.max_directory_depth = 16;
			auto params = lt::load_torrent_buffer(
				lt::span<char const>(
					reinterpret_cast<char const*>(metainfo.data()),
					metainfo.size()),
				limits);
			if (!params.ti || !params.ti->info_hashes().has_v2())
				return { false, {}, "Canonical torrent is not BitTorrent v2 metadata" };
			if (params.ti->num_files() != 1)
				return { false, {}, "Canonical torrent must contain exactly one file" };
			if (params.ti->total_size()
				!= static_cast<std::int64_t>(std::filesystem::file_size(localFilePath)))
				return { false, {}, "Canonical torrent size does not match local file" };

			auto const parent = localFilePath.parent_path();
			auto const filename = localFilePath.filename();
			if (parent.empty() || filename.empty())
				return { false, {}, "Long-seed source path must have a parent and filename" };

			params.save_path = winrt::to_string(
				winrt::hstring{ parent.wstring() });
			params.renamed_files.insert_or_assign(
				lt::file_index_t{0},
				winrt::to_string(winrt::hstring{ filename.wstring() }));

			params.flags |= lt::torrent_flags::seed_mode
				| lt::torrent_flags::duplicate_is_error
				| lt::torrent_flags::disable_dht
				| lt::torrent_flags::disable_lsd
				| lt::torrent_flags::disable_pex;
			params.flags &= ~lt::torrent_flags::auto_managed;
			params.flags &= ~lt::torrent_flags::paused;

			std::scoped_lock addLock(m_addRemoveMutex);
			lt::error_code error;
			auto handle = m_session->add_torrent(params, error);
			if (error || !handle.is_valid())
			{
				return {
					false,
					{},
					error ? error.message()
						: "libtorrent returned an invalid long-seed handle"
				};
			}

			auto const infoHash = HexDigest(params.ti->info_hashes().v2);
			{
				std::lock_guard lock(m_longSeedMutex);
				auto [it, inserted] = m_longSeedSessions.emplace(
					sessionId,
					Impl::LongSeedSession{ handle, infoHash });
				if (!inserted)
				{
					m_session->remove_torrent(handle);
					if (it->second.handle.is_valid())
						return { true, it->second.infoHashV2, {} };
					it->second = { handle, infoHash };
				}
			}

			handle.resume();
			return { true, infoHash, {} };
		}
		catch (std::exception const& exception)
		{
			return { false, {}, exception.what() };
		}
		catch (...)
		{
			return { false, {}, "Unknown long-seed session error" };
		}
	}

	bool LibtorrentHandle::ConnectLongSeedPeer(
		std::string const& sessionId,
		std::string const& address,
		std::uint16_t port,
		bool preferUtp)
	{
		if (address.empty() || port == 0) return false;

		lt::torrent_handle handle;
		{
			std::lock_guard lock(m_longSeedMutex);
			auto const it = m_longSeedSessions.find(sessionId);
			if (it == m_longSeedSessions.end()
				|| !it->second.handle.is_valid())
				return false;
			handle = it->second.handle;
		}

		try
		{
			lt::error_code error;
			auto const ip = lt::make_address(address, error);
			if (error) return false;

			lt::pex_flags_t flags{};
			if (preferUtp)
				flags |= lt::pex_utp;
			handle.connect_peer(lt::tcp::endpoint{ ip, port }, {}, flags);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	LibtorrentHandle::LongSeedSessionStatus
		LibtorrentHandle::GetLongSeedSessionStatus(
			std::string const& sessionId) const
	{
		LongSeedSessionStatus result;
		lt::torrent_handle handle;
		{
			std::lock_guard lock(m_longSeedMutex);
			auto const it = m_longSeedSessions.find(sessionId);
			if (it == m_longSeedSessions.end())
				return result;
			result.exists = true;
			handle = it->second.handle;
		}

		if (!handle.is_valid())
			return result;

		result.valid = true;
		try
		{
			auto const status = handle.status();
			result.seeding = status.is_seeding;
			result.finished = status.is_finished || status.is_seeding;
			result.progressPercent = std::clamp(
				status.progress_ppm / 10000,
				0,
				100);
			result.downloadRate = status.download_rate;
			result.uploadRate = status.upload_rate;
			result.totalWanted = status.total_wanted;
			result.totalWantedDone = status.total_wanted_done;
			result.connectedPeers = status.num_peers;
			result.connectedSeeds = status.num_seeds;
			if (status.errc)
			{
				result.hasError = true;
				result.error = status.errc.message();
			}
		}
		catch (std::exception const& exception)
		{
			result.hasError = true;
			result.error = exception.what();
		}
		catch (...)
		{
			result.hasError = true;
			result.error = "Unknown long-seed session status error";
		}
		return result;
	}

	void LibtorrentHandle::CloseLongSeedSession(std::string const& sessionId)
	{
		lt::torrent_handle handle;
		{
			std::lock_guard lock(m_longSeedMutex);
			auto const it = m_longSeedSessions.find(sessionId);
			if (it == m_longSeedSessions.end()) return;
			handle = it->second.handle;
			m_longSeedSessions.erase(it);
		}

		if (m_session && handle.is_valid())
		{
			try
			{
				m_session->remove_torrent(handle);
			}
			catch (...)
			{
			}
		}
	}

	void LibtorrentHandle::CloseAllLongSeedSessions()
	{
		std::vector<lt::torrent_handle> handles;
		{
			std::lock_guard lock(m_longSeedMutex);
			handles.reserve(m_longSeedSessions.size());
			for (auto const& [id, state] : m_longSeedSessions)
			{
				(void)id;
				if (state.handle.is_valid())
					handles.push_back(state.handle);
			}
			m_longSeedSessions.clear();
		}

		if (!m_session) return;
		for (auto const& handle : handles)
		{
			try
			{
				m_session->remove_torrent(handle);
			}
			catch (...)
			{
			}
		}
	}

	LibtorrentHandle::LongSeedSessionResult
		LibtorrentHandle::OpenLongSeedDownloadSession(
			std::string const& sessionId,
			std::vector<std::uint8_t> const& metainfo,
			std::filesystem::path const& targetFilePath,
			std::vector<std::string> const& urlSeeds)
	{
		if (sessionId.empty())
			return { false, {}, "Long-seed download session ID is empty" };
		if (metainfo.empty())
			return { false, {}, "Canonical torrent metadata is empty" };
		if (targetFilePath.empty()
			|| targetFilePath.parent_path().empty()
			|| targetFilePath.filename().empty())
			return { false, {}, "Long-seed target path is invalid" };
		if (!Initialize())
			return { false, {}, "Failed to initialize the torrent session" };

		try
		{
			{
				std::lock_guard lock(m_longSeedMutex);
				auto const existing = m_longSeedSessions.find(sessionId);
				if (existing != m_longSeedSessions.end()
					&& existing->second.handle.is_valid())
					return { true, existing->second.infoHashV2, {} };
			}

			lt::load_torrent_limits limits;
			limits.max_directory_depth = 16;
			auto params = lt::load_torrent_buffer(
				lt::span<char const>(
					reinterpret_cast<char const*>(metainfo.data()),
					metainfo.size()),
				limits);
			if (!params.ti || !params.ti->info_hashes().has_v2())
				return { false, {}, "Canonical torrent is not BitTorrent v2 metadata" };
			if (params.ti->num_files() != 1)
				return { false, {}, "Canonical torrent must contain exactly one file" };

			std::filesystem::create_directories(
				targetFilePath.parent_path());
			params.save_path = winrt::to_string(
				winrt::hstring{ targetFilePath.parent_path().wstring() });
			params.renamed_files.insert_or_assign(
				lt::file_index_t{0},
				winrt::to_string(
					winrt::hstring{ targetFilePath.filename().wstring() }));
			for (auto const& urlSeed : urlSeeds)
			{
				if (!urlSeed.empty())
					params.url_seeds.push_back(urlSeed);
			}

			params.flags |= lt::torrent_flags::duplicate_is_error
				| lt::torrent_flags::disable_dht
				| lt::torrent_flags::disable_lsd
				| lt::torrent_flags::disable_pex;
			params.flags &= ~lt::torrent_flags::seed_mode;
			params.flags &= ~lt::torrent_flags::auto_managed;
			params.flags &= ~lt::torrent_flags::paused;

			std::scoped_lock addLock(m_addRemoveMutex);
			lt::error_code error;
			auto handle = m_session->add_torrent(params, error);
			if (error || !handle.is_valid())
			{
				return {
					false,
					{},
					error ? error.message()
						: "libtorrent returned an invalid long-seed download handle"
				};
			}

			auto const infoHash = HexDigest(params.ti->info_hashes().v2);
			{
				std::lock_guard lock(m_longSeedMutex);
				auto [it, inserted] = m_longSeedSessions.emplace(
					sessionId,
					Impl::LongSeedSession{ handle, infoHash });
				if (!inserted)
				{
					m_session->remove_torrent(handle);
					return it->second.handle.is_valid()
						? LongSeedSessionResult{
							true, it->second.infoHashV2, {} }
						: LongSeedSessionResult{
							false, {}, "Long-seed session ID collision" };
				}
			}

			handle.resume();
			return { true, infoHash, {} };
		}
		catch (std::exception const& exception)
		{
			return { false, {}, exception.what() };
		}
		catch (...)
		{
			return { false, {}, "Unknown long-seed download session error" };
		}
	}

	std::string LibtorrentHandle::AddTorrentFromResumeData(std::string const& taskId)
	{
		if (!Initialize() || !m_stateManager) return {};
		try
		{
			std::scoped_lock addLock(m_addRemoveMutex);
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				auto const existing = m_taskIdToHandle.find(taskId);
				if (existing != m_taskIdToHandle.end() && existing->second.is_valid())
					return taskId;
			}
			auto const metadata = m_stateManager->LoadTaskMetadata(taskId);
			if (!metadata) return {};
			auto stored = m_stateManager->LoadTaskSettings(taskId);
			lt::add_torrent_params params;
			bool restored{};
			if (!metadata->resumeData.empty())
			{
				lt::error_code error;
				params = lt::read_resume_data(
					lt::span<char const>(reinterpret_cast<char const*>(metadata->resumeData.data()),
						metadata->resumeData.size()), error);
				restored = !error;
			}
			if (!restored)
			{
				// Cache names are application IDs, never arbitrary imported paths.
				bool const safeId = !taskId.empty() && std::ranges::all_of(taskId, [](unsigned char c)
				{
					return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z')
						|| (c >= 'A' && c <= 'Z') || c == '-' || c == '_';
				});
				if (safeId)
				{
					auto const cache = std::filesystem::path(
						winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW())
						/ L"Torrents" / (winrt::to_hstring(taskId + ".torrent").c_str());
					try
					{
						lt::load_torrent_limits limits;
						limits.max_directory_depth = std::clamp(
							::OpenNet::Core::TorrentSettingsManager::Instance().Get().maxTorrentDirectoryDepth, 1, 1000);
						params = lt::load_torrent_file(winrt::to_string(cache.wstring()), limits);
						restored = bool(params.ti);
					}
					catch (...) {}
				}
				if (!restored && !metadata->magnetUri.empty())
				{
					params = lt::parse_magnet_uri(metadata->magnetUri);
					restored = true;
				}
				if (!restored) return {};
				params.flags &= ~lt::torrent_flags::seed_mode;
			}
			// Preserve restored storage mode and part-file location. New global
			// storage defaults must not silently migrate an existing task.
			params.save_path = metadata->savePath;
			params.flags |= lt::torrent_flags::duplicate_is_error;
			bool const shouldRun = metadata->status == 1
				|| (metadata->status == 3 && stored && stored->completionAction == 1);
			if (!shouldRun)
			{
				params.flags &= ~lt::torrent_flags::auto_managed;
				params.flags |= lt::torrent_flags::paused;
				if (stored) stored->forceStart = false;
			}
			else
			{
				params.flags &= ~lt::torrent_flags::paused;
				if (stored && stored->forceStart) params.flags &= ~lt::torrent_flags::auto_managed;
				else params.flags |= lt::torrent_flags::auto_managed;
			}
			auto const hashes = params.ti ? params.ti->info_hashes() : params.info_hashes;
			TaskMetadata identity;
			FillTaskInfoHashes(identity, hashes);
			if ((!metadata->infoHashV1.empty() && !identity.infoHashV1.empty()
					&& metadata->infoHashV1 != identity.infoHashV1)
				|| (!metadata->infoHashV2.empty() && !identity.infoHashV2.empty()
					&& metadata->infoHashV2 != identity.infoHashV2))
				throw std::runtime_error("Restored torrent identity does not match its task");
			auto handle = m_session->add_torrent(params);
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				m_taskIdToHandle.emplace(taskId, handle);
				m_handleToTaskId.emplace(handle, taskId);
			}
			if (stored && !SetTorrentTaskSettings(taskId, RuntimeTaskSettings(*stored)))
			{
				handle.unset_flags(lt::torrent_flags::auto_managed);
				handle.pause();
				throw std::runtime_error("Unable to restore persisted torrent settings");
			}
			RestoreSslCertificate(taskId);
			RequestResumeDataForTorrent(handle);
			return taskId;
		}
		catch (std::exception const& error)
		{
			ErrorCallback callback;
			{
				std::lock_guard callbackLock(m_cbMutex);
				callback = m_errorCb;
			}
			if (callback) callback("Restore " + taskId + ": " + error.what());
			return {};
		}
	}

	void LibtorrentHandle::PauseTorrent(std::string const& taskId)
	{
		lt::torrent_handle handle;
		{
			std::lock_guard lk(m_torrentMapMutex);
			auto it = m_taskIdToHandle.find(taskId);
			if (it == m_taskIdToHandle.end() || !it->second.is_valid())
				return;
			handle = it->second;
		}

		// libtorrent auto-manages torrents by default and may resume an
		// auto-managed torrent after pause(). Make this an explicit user-
		// managed pause and persist the state immediately.
		handle.unset_flags(lt::torrent_flags::auto_managed);
		handle.pause();
		if (m_stateManager)
		{
			m_stateManager->UpdateTaskStatus(taskId, 2); // Paused
		}
		RequestResumeDataForTorrent(handle);
	}

	void LibtorrentHandle::ResumeTorrent(std::string const& taskId)
	{
		lt::torrent_handle handle;
		{
			std::lock_guard lk(m_torrentMapMutex);
			auto it = m_taskIdToHandle.find(taskId);
			if (it == m_taskIdToHandle.end() || !it->second.is_valid())
				return;
			handle = it->second;
		}

		auto& settings = ::OpenNet::Core::TorrentSettingsManager::Instance();
		settings.Load();
		if (settings.Get().recheckBeforeResume && handle.torrent_file())
		{
			{
				std::lock_guard lock(m_torrentMapMutex);
				m_recheckCompletionActions.insert_or_assign(
					taskId, RecheckCompletionAction::Resume);
			}
			// A paused torrent must run while libtorrent hashes its files. Keep it
			// user-managed until torrent_checked_alert restores the requested state.
			handle.unset_flags(lt::torrent_flags::auto_managed);
			handle.force_recheck();
			handle.resume();
			return;
		}

		handle.set_flags(lt::torrent_flags::auto_managed);
		handle.resume();
		if (m_stateManager)
		{
			m_stateManager->UpdateTaskStatus(taskId, 1); // Downloading
		}
		RequestResumeDataForTorrent(handle);
	}

	void LibtorrentHandle::RemoveTorrent(std::string const& taskId, bool deleteFiles)
	{
		std::scoped_lock addRemoveLock(m_addRemoveMutex);
		lt::torrent_handle handle;
		{
			std::lock_guard lk(m_torrentMapMutex);
			auto it = m_taskIdToHandle.find(taskId);
			if (it == m_taskIdToHandle.end())
				return;
			handle = it->second;
			m_handleToTaskId.erase(handle);
			m_taskIdToHandle.erase(it);
		}
		if (deleteFiles && m_stateManager && handle.is_valid())
		{
			try
			{
				TaskMetadata identity;
				FillTaskInfoHashes(identity, handle.info_hashes());
				std::lock_guard pendingLock(m_pendingDeleteMutex);
				if (!identity.infoHashV1.empty())
					m_pendingDeleteByHash.insert_or_assign(
						identity.infoHashV1, taskId);
				if (!identity.infoHashV2.empty())
					m_pendingDeleteByHash.insert_or_assign(
						identity.infoHashV2, taskId);
			}
			catch (...)
			{
			}
		}
		{
			std::lock_guard lock(m_progressPersistenceMutex);
			m_persistedProgress.erase(taskId);
		}
		{
			std::lock_guard lock(m_sessionStatsMutex);
			m_cachedTorrentRates.erase(handle);
		}
		{
			std::lock_guard lock(m_peerEventMutex);
			m_peerEvents.erase(taskId);
		}
		{
			std::lock_guard lock(m_trackerLogMutex);
			m_trackerLogs.erase(taskId);
		}
		{
			std::lock_guard lock(m_filePrioritiesMutex);
			m_filePrioritiesCache.erase(handle);
		}
		{
			std::lock_guard lock(m_peerSnapshotMutex);
			m_peerSnapshots.erase(handle);
		}
		{
			std::lock_guard lock(m_detailRequestMutex);
			m_detailRequests.erase(handle);
		}
		{
			std::lock_guard lock(m_fileProgressMutex);
			m_fileProgressCache.erase(handle);
		}
		{
			std::lock_guard lock(m_pieceAvailabilityMutex);
			m_pieceAvailabilityCache.erase(handle);
		}
		{
			std::lock_guard lock(m_trackerSnapshotMutex);
			m_trackerSnapshots.erase(handle);
		}
		{
			std::lock_guard lock(m_rateConstraintsMutex);
			m_rateConstraints.erase(handle);
		}
		{
			std::lock_guard lock(m_taskSettingsCacheMutex);
			m_taskSettingsCache.erase(taskId);
		}
		{
			std::lock_guard lock(m_torrentMetadataMutex);
			m_torrentMetadataCache.erase(handle);
		}
		{
			std::lock_guard lock(m_uploadActivityMutex);
			m_uploadActivity.erase(handle);
		}
		{
			std::lock_guard lock(m_sharePolicySnapshotMutex);
			m_sharePolicySnapshots.erase(handle);
		}
		{
			std::lock_guard lock(m_torrentMapMutex);
			m_recheckCompletionActions.erase(taskId);
		}

		if (m_session && handle.is_valid())
		{
			lt::remove_flags_t flags = {};
			if (deleteFiles)
			{
				flags = lt::session::delete_files;
			}
			m_session->remove_torrent(handle, flags);
		}

		if (m_stateManager && !deleteFiles)
		{
			m_stateManager->DeleteTask(taskId);
		}
	}

	void LibtorrentHandle::SaveAllResumeData()
	{
		if (!m_session)
			return;

		std::lock_guard lk(m_torrentMapMutex);
		for (auto const& [taskId, handle] : m_taskIdToHandle)
		{
			if (handle.is_valid())
			{
				RequestResumeDataForTorrent(handle);
			}
		}
	}

	void LibtorrentHandle::SetProgressCallback(ProgressCallback cb)
	{
		std::lock_guard lk(m_cbMutex);
		m_progressCb = std::move(cb);
	}
	void LibtorrentHandle::SetFinishedCallback(FinishedCallback cb)
	{
		std::lock_guard lk(m_cbMutex);
		m_finishedCb = std::move(cb);
	}
	void LibtorrentHandle::SetErrorCallback(ErrorCallback cb)
	{
		std::lock_guard lk(m_cbMutex);
		m_errorCb = std::move(cb);
	}

	std::string LibtorrentHandle::GetTaskIdByName(std::string const& name) const
	{
		std::lock_guard lk(m_torrentMapMutex);
		for (auto const& [taskId, handle] : m_taskIdToHandle)
		{
			if (handle.is_valid())
			{
				try
				{
					auto status = handle.status();
					if (status.name == name)
					{
						return taskId;
					}
				}
				catch (...)
				{
				}
			}
		}
		return "";
	}

	void LibtorrentHandle::AlertLoop()
	{
		OutputDebugStringA("LibtorrentHandle: AlertLoop started\n");

		int emptyAlertCount = 0;
		const int maxEmptyCount = 3; // After 3 empty cycles, increase sleep time
		std::chrono::milliseconds sleepTime(50);
		std::vector<lt::alert*> alerts;

		while (!m_stopRequested.load())
		{
			if (!m_session)
			{
				OutputDebugStringA("LibtorrentHandle: No session in AlertLoop, breaking\n");
				break;
			}

			try
			{
				alerts.clear();
				// Wait with appropriate timeout based on activity
				m_session->wait_for_alert(sleepTime);
				m_session->pop_alerts(&alerts);

				if (!alerts.empty())
				{
					DispatchAlerts(alerts);
					// Reset counters and sleep time when we have activity
					emptyAlertCount = 0;
					sleepTime = std::chrono::milliseconds(50);
				}
				else
				{
					// Gradually increase sleep time if no activity
					++emptyAlertCount;
					if (emptyAlertCount >= maxEmptyCount)
					{
						sleepTime = std::chrono::milliseconds(200);
					}
				}

				// Only request updates if we have torrents and are still running
				if (m_session && !m_stopRequested.load())
				{
					bool hasTorrents = false;
					{
						std::lock_guard lk(m_torrentMapMutex);
						hasTorrents = !m_taskIdToHandle.empty();
					}
					const auto now = std::chrono::steady_clock::now();
					// post_torrent_updates() itself produces an alert. Posting
					// it on every loop iteration keeps wait_for_alert() awake
					// and turns this worker into a busy loop. One update per
					// second is responsive enough for both native and Web UI
					// consumers while allowing the thread to sleep.
					if (hasTorrents
						&& now - m_lastTorrentUpdateRequest
						>= std::chrono::seconds(1))
					{
						m_session->post_torrent_updates();
						m_lastTorrentUpdateRequest = now;
					}
					// Time-gated stats requests: only every 2 seconds to avoid
					// self-excitation (these calls generate alerts that reset
					// emptyAlertCount, preventing adaptive backoff from engaging)
					if (now - m_lastStatsRequest >= std::chrono::seconds(2))
					{
						m_session->post_session_stats();
						m_session->post_dht_stats();
						m_lastStatsRequest = now;
					}
					if (m_stateManager && m_cachedDhtNodeCount.load() > 0 && now - m_lastDhtStateSave >= std::chrono::minutes(5))
					{
						try
						{
							auto const state = lt::write_session_params_buf(
								m_session->session_state(), lt::session::save_dht_state);
							m_stateManager->SaveSessionState(
								std::vector<std::uint8_t>(state.begin(), state.end()));
							m_lastDhtStateSave = now;
						}
						catch (...)
						{
						}
					}
					if (m_stateManager && hasTorrents
						&& now - m_lastResumeCheckpoint >= std::chrono::seconds(60))
					{
						std::vector<std::pair<std::string, lt::torrent_handle>> checkpointTasks;
						{
							std::lock_guard mapLock(m_torrentMapMutex);
							checkpointTasks.reserve(m_taskIdToHandle.size());
							for (auto const& task : m_taskIdToHandle)
								checkpointTasks.push_back(task);
						}
						for (auto const& [taskId, handle] : checkpointTasks)
						{
							bool retry{};
							{
								std::lock_guard retryLock(m_resumeRetryMutex);
								retry = m_resumeRetryTasks.contains(taskId);
							}
							RequestResumeDataForTorrent(handle, !retry);
						}
						m_lastResumeCheckpoint = now;
					}
					if (m_cachedDhtNodeCount.load() == 0 && m_dhtBootstrapAttempts < 6 && now - m_lastDhtBootstrapRetry >= std::chrono::seconds(20))
					{
						AddFallbackDhtRouters(*m_session);
						++m_dhtBootstrapAttempts;
						m_lastDhtBootstrapRetry = now;
					}
					if (now - m_lastClientFilterCheck >= std::chrono::seconds(3))
					{
						EnforceClientFilters();
						m_lastClientFilterCheck = now;
					}
					if (now - m_lastIpFilterMaintenance
						>= std::chrono::seconds(5))
					{
						::OpenNet::Core::IPFilterManager::Instance()
							.MaintainTemporaryBans();
						m_lastIpFilterMaintenance = now;
					}
				}
			}
			catch (const std::exception& ex)
			{
				OutputDebugStringW((L"LibtorrentHandle: AlertLoop error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
				std::this_thread::sleep_for(100ms);
				sleepTime = std::chrono::milliseconds(50);
				emptyAlertCount = 0;
			}
			catch (...)
			{
				OutputDebugStringA("LibtorrentHandle: AlertLoop unknown error\n");
				std::this_thread::sleep_for(100ms);
				sleepTime = std::chrono::milliseconds(50);
				emptyAlertCount = 0;
			}
		}

		OutputDebugStringA("LibtorrentHandle: AlertLoop exiting\n");
	}

	void LibtorrentHandle::EnforceClientFilters()
	{
		auto& filter = ::OpenNet::Core::ClientFilterManager::Instance();
		if (!filter.IsEnabled())
			return;

		std::vector<std::pair<std::string, lt::torrent_handle>> torrents;
		{
			std::lock_guard lock(m_torrentMapMutex);
			torrents.reserve(m_taskIdToHandle.size());
			for (auto const& [taskId, handle] : m_taskIdToHandle)
			{
				if (handle.is_valid())
					torrents.emplace_back(taskId, handle);
			}
		}

		std::vector<::OpenNet::Core::ClientPeerObservation> observations;
		for (auto const& [taskId, handle] : torrents)
		{
			try
			{
				if (ShouldRequestDetails(handle, 0)) handle.post_peer_info();
				std::vector<TorrentPeerInfo> peers;
				{
					std::lock_guard snapshotLock(m_peerSnapshotMutex);
					if (auto const snapshot = m_peerSnapshots.find(handle);
						snapshot != m_peerSnapshots.end())
						peers = snapshot->second;
				}
				observations.reserve(observations.size() + peers.size());
				for (auto const& peer : peers)
				{
					if (peer.client.empty() || peer.ip.empty() || peer.isI2p)
						continue;
					observations.push_back({
						peer.client,
						peer.ip,
						taskId,
									   });
				}
			}
			catch (...)
			{
				// A torrent may disappear while the snapshot is being read.
			}
		}
		filter.EvaluatePeers(observations);
	}

	template<typename THandle, typename TEndpoint>
	void LibtorrentHandle::RecordPeerEvent(
		THandle const& handle,
		TEndpoint const& endpoint,
		std::string reason,
		bool isBan)
	{
		if (endpoint.address().is_unspecified())
			return;

		std::string taskId;
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const found = m_handleToTaskId.find(handle);
			if (found == m_handleToTaskId.end())
				return;
			taskId = found->second;
		}

		PeerConnectionEvent event;
		event.ip = endpoint.address().to_string();
		event.port = endpoint.port();
		event.reason = std::move(reason);
		event.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		event.isBan = isBan;

		std::lock_guard lock(m_peerEventMutex);
		auto& events = m_peerEvents[taskId];
		auto const existing = std::find_if(
			events.begin(), events.end(),
			[&](auto const& current)
		{
			return current.ip == event.ip && current.port == event.port;
		});
		if (existing != events.end())
		{
			// A ban alert is commonly followed by a generic disconnect alert.
			// Keep the stronger terminal state instead of downgrading it.
			if (existing->isBan && !event.isBan)
				return;
			events.erase(existing);
		}
		events.push_front(std::move(event));
		while (events.size() > 256)
			events.pop_back();
	}

	template<typename THandle, typename TEndpoint>
	void LibtorrentHandle::ClearPeerEvent(THandle const& handle, TEndpoint const& endpoint)
	{
		std::string taskId;
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const found = m_handleToTaskId.find(handle);
			if (found == m_handleToTaskId.end())
				return;
			taskId = found->second;
		}

		auto const ip = endpoint.address().to_string();
		auto const port = endpoint.port();
		std::lock_guard lock(m_peerEventMutex);
		auto const found = m_peerEvents.find(taskId);
		if (found == m_peerEvents.end())
			return;
		std::erase_if(
			found->second,
			[&](auto const& event)
		{
			return event.ip == ip && event.port == port;
		});
	}

	template<typename THandle>
	void LibtorrentHandle::RecordTrackerLog(
		THandle const& handle,
		std::string const& trackerUrl,
		std::string content,
		bool const isError)
	{
		if (trackerUrl.empty() || content.empty())
			return;

		std::string taskId;
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const found = m_handleToTaskId.find(handle);
			if (found == m_handleToTaskId.end())
				return;
			taskId = found->second;
		}

		TrackerLogEntry entry;
		entry.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		entry.content = std::move(content);
		entry.isError = isError;

		std::lock_guard lock(m_trackerLogMutex);
		auto& entries = m_trackerLogs[taskId][trackerUrl];
		entries.push_back(std::move(entry));
		while (entries.size() > 500)
			entries.pop_front();
	}

	template<typename THandle, typename TTrackers>
	std::vector<LibtorrentHandle::TorrentTrackerInfo>
		LibtorrentHandle::ConvertTrackers(
			THandle const& handle,
			TTrackers const& trackers) const
	{
		std::vector<TorrentTrackerInfo> result;
		result.reserve(trackers.size());
		for (auto const& tracker : trackers)
		{
			TorrentTrackerInfo value;
			value.url = tracker.url;
			value.tier = tracker.tier;
			value.status = "not contacted";
			bool contacted{};
			bool updating{};
			bool failed{};
			auto const now = lt::time_point_cast<lt::seconds32>(
				lt::clock_type::now());
			for (auto const& endpoint : tracker.endpoints)
			{
				for (auto const& hash : endpoint.info_hashes)
				{
					value.retries = (std::max)(
						value.retries, static_cast<int>(hash.fails));
					updating = updating || hash.updating;
					failed = failed || hash.fails > 0 || bool(hash.last_error);
					contacted = contacted || hash.start_sent || hash.complete_sent
						|| hash.updating || hash.fails > 0 || bool(hash.last_error)
						|| !hash.message.empty() || hash.scrape_complete >= 0
						|| hash.scrape_incomplete >= 0;
					if (hash.scrape_complete >= 0)
						value.seeders = (std::max)(value.seeders, hash.scrape_complete);
					if (hash.scrape_incomplete >= 0)
						value.leechers = (std::max)(value.leechers, hash.scrape_incomplete);
					if (hash.scrape_downloaded >= 0)
						value.downloaded = (std::max)(value.downloaded, hash.scrape_downloaded);
					if (hash.next_announce != (lt::time_point32::min)()
						&& hash.next_announce != (lt::time_point32::max)())
					{
						auto const remaining = static_cast<int>((std::max)(
							std::int64_t{ 0 },
							lt::total_seconds(hash.next_announce - now)));
						value.nextAnnounceSeconds = value.nextAnnounceSeconds < 0
							? remaining
							: (std::min)(value.nextAnnounceSeconds, remaining);
					}
					if (value.message.empty())
					{
						if (hash.last_error) value.message = hash.last_error.message();
						else if (!hash.message.empty()) value.message = hash.message;
					}
				}
			}
			if (value.seeders >= 0 && value.leechers >= 0)
				value.numPeers = value.seeders + value.leechers;
			if (updating) value.status = "updating";
			else if (failed) value.status = "error";
			else if (contacted) value.status = "working";
			(void)handle;
			result.push_back(std::move(value));
		}
		return result;
	}

	template<typename TStatuses>
	void LibtorrentHandle::EvaluateSharePolicies(TStatuses const& statuses)
	{
		auto const now = std::chrono::steady_clock::now();
		{
			std::lock_guard activityLock(m_uploadActivityMutex);
			for (auto const& status : statuses)
			{
				auto [activity, inserted] = m_uploadActivity.try_emplace(
					status.handle,
					Impl::UploadActivity{ status.all_time_upload, now });
				if (!inserted && (activity->second.uploaded != status.all_time_upload
					|| !status.is_finished || bool(status.flags & lt::torrent_flags::paused)))
				{
					activity->second.uploaded = status.all_time_upload;
					activity->second.lastChange = now;
				}
			}
		}
		std::vector<Impl::SharePolicySnapshot> snapshots;
		{
			std::lock_guard snapshotLock(m_sharePolicySnapshotMutex);
			for (auto const& status : statuses)
			{
				m_sharePolicySnapshots.insert_or_assign(
					status.handle,
					Impl::SharePolicySnapshot{
						status.handle,
						status.flags,
						status.all_time_download,
						status.all_time_upload,
						status.finished_duration,
						now,
						status.is_finished,
						status.is_seeding });
			}
			snapshots.reserve(m_sharePolicySnapshots.size());
			for (auto const& [handle, snapshot] : m_sharePolicySnapshots)
			{
				(void)handle;
				snapshots.push_back(snapshot);
			}
		}

		if (!m_stateManager
			|| now - m_lastSharePolicyCheck < std::chrono::seconds(10))
			return;
		m_lastSharePolicyCheck = now;

		struct PendingAction
		{
			std::string taskId;
			lt::torrent_handle handle;
			int action{};
		};
		std::vector<PendingAction> actions;
		auto const global =
			::OpenNet::Core::TorrentSettingsManager::Instance().Get();

		for (auto const& status : snapshots)
		{
			if (!(status.is_finished || status.is_seeding)
				|| bool(status.flags & lt::torrent_flags::paused))
				continue;

			std::string taskId;
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				auto const task = m_handleToTaskId.find(status.handle);
				if (task != m_handleToTaskId.end()) taskId = task->second;
			}
			if (taskId.empty()) continue;
			std::optional<TaskSettingsMetadata> stored;
			{
				std::lock_guard cacheLock(m_taskSettingsCacheMutex);
				if (auto const cached = m_taskSettingsCache.find(taskId);
					cached != m_taskSettingsCache.end())
					stored = cached->second;
			}
			if (stored && stored->forceStart) continue;

			auto const taskRatio = stored ? stored->shareRatioLimit : -2.0;
			auto const taskTime = stored ? stored->seedingTimeLimit : -2;
			auto const taskInactive = stored
				? stored->inactiveSeedingTimeLimit : -2;
			auto const ratioLimit = taskRatio <= -2.0
				? global.seedingRatioLimit : taskRatio;
			auto const timeLimit = taskTime <= -2
				? global.seedingTimeLimit : taskTime;
			auto const inactiveLimit = taskInactive <= -2
				? global.inactiveSeedingTimeLimit : taskInactive;

			int configuredConditions{};
			int reachedConditions{};
			if (ratioLimit >= 0.0 && taskRatio != -1.0
				&& (taskRatio > -2.0 || global.seedingRatioLimit > 0.0))
			{
				++configuredConditions;
				if (status.all_time_download > 0
					&& static_cast<double>(status.all_time_upload)
					/ static_cast<double>(status.all_time_download)
					>= ratioLimit)
					++reachedConditions;
			}
			if (timeLimit >= 0 && taskTime != -1
				&& (taskTime > -2 || global.seedingTimeLimit > 0))
			{
				++configuredConditions;
				auto const observedSeedingSeconds =
					status.seeding_duration.count()
					+ (status.is_finished
					   && !bool(status.flags & lt::torrent_flags::paused)
					   ? std::chrono::duration_cast<std::chrono::seconds>(
						   now - status.observedAt).count()
					   : 0);
				if (observedSeedingSeconds
					>= static_cast<std::int64_t>(timeLimit) * 60)
					++reachedConditions;
			}
			if (inactiveLimit >= 0 && taskInactive != -1
				&& (taskInactive > -2 || global.inactiveSeedingTimeLimit > 0))
			{
				++configuredConditions;
				std::chrono::steady_clock::time_point lastUpload = now;
				{
					std::lock_guard activityLock(m_uploadActivityMutex);
					if (auto const activity = m_uploadActivity.find(status.handle);
						activity != m_uploadActivity.end())
						lastUpload = activity->second.lastChange;
				}
				if (now - lastUpload >= std::chrono::minutes(inactiveLimit))
					++reachedConditions;
			}
			if (configuredConditions == 0) continue;

			bool const hasTaskPolicy = stored
				&& (stored->shareRatioLimit > -2.0
					|| stored->seedingTimeLimit > -2
					|| stored->inactiveSeedingTimeLimit > -2);
			bool const matchAll = hasTaskPolicy
				? stored->shareLimitMatchAll
				: global.shareLimitMatchAll;
			bool const matched = matchAll
				? reachedConditions == configuredConditions
				: reachedConditions > 0;
			if (matched)
				actions.push_back({
					taskId, status.handle,
					stored && stored->shareLimitAction >= 0 ? stored->shareLimitAction
						: global.shareLimitAction });
		}

		for (auto const& action : actions)
		{
			if (action.action == 1 || action.action == 2)
			{
				RemoveTorrent(action.taskId, action.action == 2);
			}
			else if (action.action == 3)
			{
				auto settings = GetTorrentTaskSettings(action.taskId);
				settings.superSeeding = true;
				settings.shareRatioLimit = -1.0;
				settings.seedingTimeLimit = -1;
				settings.inactiveSeedingTimeLimit = -1;
				SetTorrentTaskSettings(action.taskId, settings);
			}
			else
			{
				action.handle.unset_flags(lt::torrent_flags::auto_managed);
				action.handle.pause();
				// Persist the stop independently from content completion.
				m_stateManager->UpdateTaskStatus(action.taskId, 2);
				RequestResumeDataForTorrent(action.handle);
			}
		}
	}

	template<typename TAlerts>
	void LibtorrentHandle::DispatchAlerts(TAlerts const& alerts)
	{
		for (lt::alert* alert : alerts)
		{
			if (auto connected = lt::alert_cast<lt::peer_connect_alert>(alert))
			{
				if (auto const endpoint = TcpEndpoint(connected->ep))
					ClearPeerEvent(connected->handle, *endpoint);
			}
			else if (auto ban = lt::alert_cast<lt::peer_ban_alert>(alert))
			{
				if (auto const endpoint = TcpEndpoint(ban->ep))
					RecordPeerEvent(
						ban->handle, *endpoint, "anti_leech", true);
			}
			else if (auto internalBan = lt::alert_cast<lt::ip_ban_alert>(alert))
			{
				m_internalBanCount.fetch_add(1, std::memory_order_relaxed);
				OutputDebugStringA(("libtorrent internally banned " + internalBan->banned_address.to_string() + "\n").c_str());
			}
			else if (auto blocked = lt::alert_cast<lt::peer_blocked_alert>(alert))
			{
				if (auto const endpoint = TcpEndpoint(blocked->ep))
					RecordPeerEvent(
						blocked->handle, *endpoint, blocked->message(), true);
			}
			else if (auto i2pError = lt::alert_cast<lt::i2p_alert>(alert))
			{
				ErrorCallback callback;
				{
					std::lock_guard callbackLock(m_cbMutex);
					callback = m_errorCb;
				}
				if (callback) callback("I2P error: " + i2pError->message());
			}
			else if (auto performance = lt::alert_cast<lt::performance_alert>(alert))
			{
				OutputDebugStringA(("libtorrent performance warning: "
									+ performance->message() + "\n").c_str());
			}
			else if (auto dropped = lt::alert_cast<lt::alerts_dropped_alert>(alert))
			{
				OutputDebugStringA(("libtorrent alerts dropped: "
									+ dropped->message() + "\n").c_str());
				// Refresh snapshots that may have missed an incremental event.
				if (m_session)
				{
					m_session->post_torrent_updates();
					m_session->post_session_stats();
				}
			}
			else if (auto priorities = lt::alert_cast<lt::file_priorities_alert>(alert))
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				if (!m_handleToTaskId.contains(priorities->handle)) continue;
				std::lock_guard lock(m_filePrioritiesMutex);
				m_filePrioritiesCache.insert_or_assign(priorities->handle, priorities->priorities);
			}
			else if (auto peerSnapshot = lt::alert_cast<lt::peer_info_alert>(alert))
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				if (!m_handleToTaskId.contains(peerSnapshot->handle)) continue;
				std::vector<TorrentPeerInfo> peers;
				peers.reserve(peerSnapshot->peer_info.size());
				std::ranges::transform(
					peerSnapshot->peer_info,
					std::back_inserter(peers),
					ConvertPeerInfo);
				std::lock_guard lock(m_peerSnapshotMutex);
				m_peerSnapshots.insert_or_assign(
					peerSnapshot->handle, std::move(peers));
			}
			else if (auto progress = lt::alert_cast<lt::file_progress_alert>(alert))
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				if (!m_handleToTaskId.contains(progress->handle)) continue;
				std::vector<std::int64_t> values(
					progress->files.begin(), progress->files.end());
				std::lock_guard lock(m_fileProgressMutex);
				m_fileProgressCache.insert_or_assign(
					progress->handle, std::move(values));
			}
			else if (auto availability = lt::alert_cast<lt::piece_availability_alert>(alert))
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				if (!m_handleToTaskId.contains(availability->handle)) continue;
				std::lock_guard lock(m_pieceAvailabilityMutex);
				m_pieceAvailabilityCache.insert_or_assign(
					availability->handle,
					std::move(availability->piece_availability));
			}
			else if (auto trackers = lt::alert_cast<lt::tracker_list_alert>(alert))
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				if (!m_handleToTaskId.contains(trackers->handle)) continue;
				auto snapshot = ConvertTrackers(
					trackers->handle, trackers->trackers);
				std::lock_guard lock(m_trackerSnapshotMutex);
				m_trackerSnapshots.insert_or_assign(
					trackers->handle, std::move(snapshot));
			}
			else if (auto disconnected = lt::alert_cast<lt::peer_disconnected_alert>(alert))
			{
				auto reason = PeerDisconnectReason(*disconnected);
				auto const isBan = reason == "ip_filter" || reason == "anti_leech";
				if (auto const endpoint = TcpEndpoint(disconnected->ep))
					RecordPeerEvent(
						disconnected->handle,
						*endpoint,
						std::move(reason),
						isBan);
			}
			else if (auto peerError = lt::alert_cast<lt::peer_error_alert>(alert))
			{
				auto reason = peerError->error
					? peerError->error.message()
					: std::string{ "peer_error" };
				auto const isBan =
					IsLibtorrentError(
						peerError->error, lt::errors::banned_by_ip_filter)
					|| IsLibtorrentError(
						peerError->error, lt::errors::peer_banned)
					|| IsLibtorrentError(
						peerError->error, lt::errors::too_many_corrupt_pieces);
				if (isBan)
				{
					reason = IsLibtorrentError(
						peerError->error, lt::errors::banned_by_ip_filter)
						? "ip_filter"
						: "anti_leech";
				}
				if (auto const endpoint = TcpEndpoint(peerError->ep))
					RecordPeerEvent(
						peerError->handle,
						*endpoint,
						std::move(reason),
						isBan);
			}
			else if (auto trackerError = lt::alert_cast<lt::tracker_error_alert>(alert))
			{
				auto content = std::string{ "Tracker connection error: " };
				if (trackerError->error)
					content += trackerError->error.message();
				if (auto const reason = trackerError->failure_reason();
					reason && *reason)
				{
					if (trackerError->error)
						content += " - ";
					content += reason;
				}
				RecordTrackerLog(
					trackerError->handle,
					trackerError->tracker_url(),
					std::move(content), true);
			}
			else if (auto warning = lt::alert_cast<lt::tracker_warning_alert>(alert))
			{
				RecordTrackerLog(
					warning->handle,
					warning->tracker_url(),
					std::string{ "Tracker warning: " }
				+ warning->warning_message(), true);
			}
			else if (auto scrape = lt::alert_cast<lt::scrape_reply_alert>(alert))
			{
				RecordTrackerLog(
					scrape->handle,
					scrape->tracker_url(),
					"Tracker returned info: complete = "
					+ std::to_string(scrape->complete)
					+ ", incomplete = "
					+ std::to_string(scrape->incomplete));
			}
			else if (auto scrapeFailed = lt::alert_cast<lt::scrape_failed_alert>(alert))
			{
				auto content = std::string{ "Tracker scrape error: " };
				if (scrapeFailed->error)
					content += scrapeFailed->error.message();
				else if (auto const message = scrapeFailed->error_message())
					content += message;
				RecordTrackerLog(
					scrapeFailed->handle,
					scrapeFailed->tracker_url(),
					std::move(content), true);
			}
			else if (auto reply = lt::alert_cast<lt::tracker_reply_alert>(alert))
			{
				auto const trackerUrl = std::string{ reply->tracker_url() };
				RecordTrackerLog(
					reply->handle,
					trackerUrl,
					"Logged in; Tracker returned "
					+ std::to_string(reply->num_peers) + " peers");
				if (auto const remaining = NextTrackerAnnounceSeconds(reply->handle, trackerUrl))
				{
					RecordTrackerLog(
						reply->handle, trackerUrl,
						"Schedule next announce in "
						+ FormatTrackerInterval(*remaining));
				}
			}
			else if (auto announce = lt::alert_cast<lt::tracker_announce_alert>(alert))
			{
				RecordTrackerLog(
					announce->handle,
					announce->tracker_url(),
					std::string{ "Announce " } + announce->tracker_url());
				RecordTrackerLog(
					announce->handle,
					announce->tracker_url(),
					"Start connecting...");
			}
			else if (auto st = lt::alert_cast<lt::state_update_alert>(alert))
			{
				{
					std::lock_guard statsLock(m_sessionStatsMutex);
					for (auto const& status : st->status)
					{
						m_cachedTorrentRates.insert_or_assign(
							status.handle,
							CachedTorrentRate{
								status.download_rate,
								status.upload_rate,
								status.total_done,
								status.total_upload,
								status.num_peers,
								status.num_seeds,
								status.state,
								(status.flags & lt::torrent_flags::paused)
									!= lt::torrent_flags_t{},
								static_cast<bool>(status.errc),
								status.is_finished || status.is_seeding });
					}
					m_cachedDownloadRate = 0;
					m_cachedUploadRate = 0;
					m_cachedLongTermSeedingUploadRate = 0;
					for (auto const& [handle, rate] : m_cachedTorrentRates)
					{
						(void)handle;
						m_cachedDownloadRate += rate.downloadRate;
						m_cachedUploadRate += rate.uploadRate;
						if (rate.isFinished)
							m_cachedLongTermSeedingUploadRate += rate.uploadRate;
					}
				}

				EvaluateSharePolicies(st->status);

				ProgressCallback progressCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					progressCbCopy = m_progressCb;
				}

				if (progressCbCopy)
				{
					for (auto const& s : st->status)
					{
						std::string taskId;
						{
							std::lock_guard mapLk(m_torrentMapMutex);
							auto const it = m_handleToTaskId.find(s.handle);
							if (it != m_handleToTaskId.end())
								taskId = it->second;
						}

						// Ignore handles that do not belong to an OpenNet task.
						// This prevents anonymous session activity from creating
						// UI rows that cannot be controlled or persisted.
						if (taskId.empty())
							continue;

						ProgressEvent evt;
						evt.taskId = taskId;
						evt.progressPercent = static_cast<int>(s.progress_ppm / 10000); // 1e6 -> %
						evt.isPaused = (s.flags & lt::torrent_flags::paused) != lt::torrent_flags_t{};
						evt.isChecking = s.state == lt::torrent_status::checking_files || s.state == lt::torrent_status::checking_resume_data;
						evt.downloadRateKB = evt.isPaused
							? 0 : static_cast<int>(s.download_rate / 1000);
						evt.uploadRateKB = evt.isPaused
							? 0 : static_cast<int>(s.upload_rate / 1000);
						evt.totalSize = s.total_wanted;
						evt.downloadedSize = s.total_wanted_done;
						evt.sessionDownloaded = s.total_download;
						evt.sessionUploaded = s.total_upload;
						evt.allTimeDownloaded = s.all_time_download;
						evt.allTimeUploaded = s.all_time_upload;
						evt.completedTimestamp = static_cast<std::int64_t>(s.completed_time);
						evt.connectedPeers = s.num_peers;
						evt.connectedSeeds = s.num_seeds;
						evt.knownPeers = (std::max)(
							s.num_incomplete, (std::max)(0, s.list_peers - s.list_seeds));
						evt.knownSeeds = (std::max)(s.num_complete, s.list_seeds);
						evt.name = s.name;
						evt.isFinished = s.is_finished;
						evt.isSeeding = s.is_seeding;
						{
							std::lock_guard lock(m_rateConstraintsMutex);
							auto const constraints = m_rateConstraints.find(s.handle);
							if (constraints != m_rateConstraints.end())
							{
								evt.downloadLimit = constraints->second.downloadLimit;
								evt.uploadLimit = constraints->second.uploadLimit;
								evt.minimumUploadRate = constraints->second.minimumUploadRate;
							}
						}
						progressCbCopy(evt);

						// Persist progress without turning every one-second status
						// snapshot into one SQLite transaction per torrent. UI
						// callbacks remain real-time; completion is always durable.
						if (m_stateManager)
						{
							auto const now = std::chrono::steady_clock::now();
							constexpr auto interval = std::chrono::seconds(15);
							constexpr std::int64_t byteThreshold = 64ll * 1024 * 1024;
							bool shouldPersist{};
							{
								std::lock_guard lock(m_progressPersistenceMutex);
								auto const persisted = m_persistedProgress.find(taskId);
								shouldPersist = (s.is_finished
												 && (persisted == m_persistedProgress.end()
													 || persisted->second.downloadedSize != s.total_done))
									|| persisted == m_persistedProgress.end()
									|| now - persisted->second.timestamp >= interval
									|| std::abs(s.total_done
												- persisted->second.downloadedSize) >= byteThreshold;
							}
							if (shouldPersist && m_stateManager->UpdateTaskProgress(taskId, s.total_done, s.all_time_upload, static_cast<std::int64_t>(s.completed_time)))
							{
								std::lock_guard lock(m_progressPersistenceMutex);
								m_persistedProgress.insert_or_assign(
									taskId, PersistedProgress{ s.total_done, now });
							}
						}
					}
				}
			}
			else if (auto checked = lt::alert_cast<lt::torrent_checked_alert>(alert))
			{
				std::string taskId;
				std::optional<RecheckCompletionAction> completionAction;
				{
					std::lock_guard mapLock(m_torrentMapMutex);
					auto const task = m_handleToTaskId.find(checked->handle);
					if (task != m_handleToTaskId.end())
					{
						taskId = task->second;
						auto const action = m_recheckCompletionActions.find(taskId);
						if (action != m_recheckCompletionActions.end())
						{
							completionAction = action->second;
							m_recheckCompletionActions.erase(action);
						}
					}
				}

				if (completionAction == RecheckCompletionAction::Pause)
				{
					checked->handle.unset_flags(lt::torrent_flags::auto_managed);
					checked->handle.pause();
					if (m_stateManager && !taskId.empty())
						m_stateManager->UpdateTaskStatus(taskId, 2);
				}
				else if (completionAction == RecheckCompletionAction::Resume)
				{
					checked->handle.set_flags(lt::torrent_flags::auto_managed);
					checked->handle.resume();
					if (m_stateManager && !taskId.empty())
						m_stateManager->UpdateTaskStatus(taskId, 1);
				}

				// A manual recheck does not install a completion action for torrents
				// that were already running.  The freshly verified piece map still has
				// to be persisted, otherwise the next application launch restores the
				// stale resume data and presents completed pieces as unchecked again.
				RequestResumeDataForTorrent(checked->handle);
			}
			else if (auto renamed = lt::alert_cast<lt::file_renamed_alert>(alert))
			{
				// get_renamed_files() is now the authoritative current-name source.
				// Persist the mapping only after libtorrent confirms the disk rename.
				RequestResumeDataForTorrent(renamed->handle);
			}
			else if (auto renameFailed = lt::alert_cast<lt::file_rename_failed_alert>(alert))
			{
				ErrorCallback callback;
				{
					std::lock_guard callbackLock(m_cbMutex);
					callback = m_errorCb;
				}
				if (callback)
					callback("File rename failed: " + renameFailed->message());
			}
			else if (auto deleted = lt::alert_cast<lt::torrent_deleted_alert>(alert))
			{
				auto const taskId = TakePendingDeleteTask(deleted->info_hashes);
				if (m_stateManager && !taskId.empty())
					m_stateManager->DeleteTask(taskId);
			}
			else if (auto deleteFailed = lt::alert_cast<lt::torrent_delete_failed_alert>(alert))
			{
				auto const taskId = TakePendingDeleteTask(deleteFailed->info_hashes);
				if (m_stateManager && !taskId.empty())
				{
					if (auto metadata = m_stateManager->LoadTaskMetadata(taskId))
					{
						metadata->status = 4;
						metadata->errorMessage = deleteFailed->message();
						m_stateManager->SaveTaskMetadata(*metadata);
					}
				}
				ErrorCallback callback;
				{
					std::lock_guard callbackLock(m_cbMutex);
					callback = m_errorCb;
				}
				if (callback)
					callback("Torrent content deletion failed: "
							 + deleteFailed->message());
			}
			else if (auto moved = lt::alert_cast<lt::storage_moved_alert>(alert))
			{
				std::string taskId;
				{
					std::lock_guard mapLock(m_torrentMapMutex);
					auto const task = m_handleToTaskId.find(moved->handle);
					if (task != m_handleToTaskId.end()) taskId = task->second;
				}
				bool completedMove{};
				{
					std::lock_guard moveLock(m_completionMoveMutex);
					auto const pending = m_completionMoves.find(taskId);
					if (pending != m_completionMoves.end())
					{
						completedMove = true;
						if (pending->second)
						{
							moved->handle.set_flags(lt::torrent_flags::auto_managed);
							moved->handle.resume();
						}
						m_completionMoves.erase(pending);
					}
				}
				if (m_stateManager && !taskId.empty())
					m_stateManager->UpdateTaskSavePath(
						taskId, moved->storage_path());
				RequestResumeDataForTorrent(moved->handle);
				if (completedMove)
				{
					CatalogCompletedTorrent(moved->handle, taskId);
					FinishedCallback callback;
					{
						std::lock_guard callbackLock(m_cbMutex);
						callback = m_finishedCb;
					}
					if (callback)
					{
						try
						{
							callback(taskId, moved->handle.status().name);
						}
						catch (...)
						{
						}
					}
				}
			}
			else if (auto moveFailed = lt::alert_cast<lt::storage_moved_failed_alert>(alert))
			{
				std::string taskId;
				{
					std::lock_guard mapLock(m_torrentMapMutex);
					auto const task = m_handleToTaskId.find(moveFailed->handle);
					if (task != m_handleToTaskId.end()) taskId = task->second;
				}
				bool completedMove{};
				{
					std::lock_guard moveLock(m_completionMoveMutex);
					auto const pending = m_completionMoves.find(taskId);
					if (pending != m_completionMoves.end())
					{
						completedMove = true;
						if (pending->second)
						{
							moveFailed->handle.set_flags(lt::torrent_flags::auto_managed);
							moveFailed->handle.resume();
						}
						m_completionMoves.erase(pending);
					}
				}
				ErrorCallback errorCallback;
				FinishedCallback finishedCallback;
				{
					std::lock_guard callbackLock(m_cbMutex);
					errorCallback = m_errorCb;
					finishedCallback = m_finishedCb;
				}
				if (errorCallback)
					errorCallback("Storage move failed: " + moveFailed->message());
				if (completedMove)
					CatalogCompletedTorrent(moveFailed->handle, taskId);
				if (completedMove && finishedCallback)
				{
					try
					{
						finishedCallback(taskId, moveFailed->handle.status().name);
					}
					catch (...)
					{
					}
				}
			}
			else if (auto needCertificate = lt::alert_cast<lt::torrent_need_cert_alert>(alert))
			{
				std::string taskId;
				{
					std::lock_guard mapLock(m_torrentMapMutex);
					auto const task = m_handleToTaskId.find(needCertificate->handle);
					if (task != m_handleToTaskId.end()) taskId = task->second;
				}
				if (!taskId.empty() && RestoreSslCertificate(taskId)) continue;
				ErrorCallback callback;
				{
					std::lock_guard callbackLock(m_cbMutex);
					callback = m_errorCb;
				}
				if (callback)
					callback("TLS torrent requires a client certificate: "
							 + needCertificate->message());
			}
			else if (auto tf = lt::alert_cast<lt::torrent_finished_alert>(alert))
			{
				std::string taskId;
				{
					std::lock_guard mapLk(m_torrentMapMutex);
					auto const it = m_handleToTaskId.find(tf->handle);
					if (it != m_handleToTaskId.end())
						taskId = it->second;
				}
				if (taskId.empty()) continue;
				auto& settingsManager =
					::OpenNet::Core::TorrentSettingsManager::Instance();
				settingsManager.Load();
				auto const settings = settingsManager.Get();
				auto taskSettings = GetTorrentTaskSettings(taskId);
				if (taskSettings.completionAction < 0)
				{
					auto const previous = m_stateManager
						? m_stateManager->LoadTaskMetadata(taskId) : std::nullopt;
					// Old completed records keep their historical stopped behavior,
					// including when a restore emits another finished alert.
					taskSettings.completionAction = previous && previous->status == 3
						? 0 : (settings.continueSeedingAfterCompletion ? 1 : 0);
					if (!SetTorrentTaskSettings(taskId, taskSettings))
					{
						PauseTorrent(taskId);
						continue;
					}
				}
				bool const continueSeeding = taskSettings.completionAction == 1;
				bool const moveOnCompletion = settings.moveCompletedEnabled
					&& !settings.moveCompletedPath.empty();
				if (!continueSeeding || moveOnCompletion)
				{
					// Moving is performed from a stable paused state. A task whose
					// completion action is ContinueSeeding is resumed by the move alert.
					tf->handle.unset_flags(lt::torrent_flags::auto_managed);
					tf->handle.pause();
				}


				if (m_stateManager && !taskId.empty())
				{
					m_stateManager->UpdateTaskStatus(taskId, 3); // Completed
				}

				if (moveOnCompletion && !taskId.empty())
				{
					{
						std::lock_guard moveLock(m_completionMoveMutex);
						m_completionMoves.insert_or_assign(
							taskId, continueSeeding);
					}
					tf->handle.move_storage(winrt::to_string(
						winrt::hstring{ settings.moveCompletedPath }));
				}
				else
				{
					// Save after changing flags so the completed/paused state is
					// what is restored on the next launch.
					RequestResumeDataForTorrent(tf->handle);
					CatalogCompletedTorrent(tf->handle, taskId);
				}

				FinishedCallback finishedCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					finishedCbCopy = m_finishedCb;
				}

				if (finishedCbCopy && !moveOnCompletion)
				{
					try
					{
						auto status = tf->handle.status();
						finishedCbCopy(taskId, status.name);
					}
					catch (...)
					{
					}
				}
			}
			else if (auto srd = lt::alert_cast<lt::save_resume_data_alert>(alert))
			{
				HandleSaveResumeDataAlert(srd);
			}
			else if (auto srdf = lt::alert_cast<lt::save_resume_data_failed_alert>(alert))
			{
				HandleSaveResumeDataFailedAlert(srdf);
			}
			else if (auto mapping = lt::alert_cast<lt::portmap_alert>(alert))
			{
				std::lock_guard lock(m_portMappingMutex);
				std::string mechanism =
					mapping->map_transport == lt::portmap_transport::upnp
					? "UPnP" : "NAT-PMP/PCP";
				if (mapping->map_protocol == lt::portmap_protocol::tcp)
				{
					m_portMappingStatus.tcpExternalPort = mapping->external_port;
					m_portMappingStatus.tcpMechanism = mechanism;
				}
				else if (mapping->map_protocol == lt::portmap_protocol::udp)
				{
					m_portMappingStatus.udpExternalPort = mapping->external_port;
					m_portMappingStatus.udpMechanism = mechanism;
				}
				m_portMappingStatus.lastError.clear();
			}
			else if (auto mappingError = lt::alert_cast<lt::portmap_error_alert>(alert))
			{
				std::lock_guard lock(m_portMappingMutex);
				m_portMappingStatus.lastError = mappingError->message();
			}
			else if (auto externalIp = lt::alert_cast<lt::external_ip_alert>(alert))
			{
				std::lock_guard lock(m_portMappingMutex);
				m_portMappingStatus.externalAddress =
					externalIp->external_address.to_string();
			}
			else if (auto listenFailed = lt::alert_cast<lt::listen_failed_alert>(alert))
			{
				std::lock_guard lock(m_listenStateMutex);
				m_lastListenError = listenFailed->message();
				if (listenFailed->address.is_v4())
				{
					m_ipv4ListenError = listenFailed->message();
				}
				else if (listenFailed->address.is_v6())
				{
					m_ipv6ListenError = listenFailed->message();
				}
			}
			else if (auto listenSucceeded = lt::alert_cast<lt::listen_succeeded_alert>(alert))
			{
				std::lock_guard lock(m_listenStateMutex);
				m_lastListenError.clear();
				if (listenSucceeded->address.is_v4())
				{
					if (listenSucceeded->socket_type == lt::socket_type_t::tcp)
						m_ipv4TcpListenPort = listenSucceeded->port;
					else if (listenSucceeded->socket_type == lt::socket_type_t::utp)
						m_ipv4UtpListenPort = listenSucceeded->port;
					m_ipv4ListenPort = m_ipv4TcpListenPort > 0
						? m_ipv4TcpListenPort
						: m_ipv4UtpListenPort;
					m_ipv4ListenError.clear();
				}
				else if (listenSucceeded->address.is_v6())
				{
					if (listenSucceeded->socket_type == lt::socket_type_t::tcp)
						m_ipv6TcpListenPort = listenSucceeded->port;
					else if (listenSucceeded->socket_type == lt::socket_type_t::utp)
						m_ipv6UtpListenPort = listenSucceeded->port;
					m_ipv6ListenPort = m_ipv6TcpListenPort > 0
						? m_ipv6TcpListenPort
						: m_ipv6UtpListenPort;
					m_ipv6ListenError.clear();
				}
			}
			else if (auto se = lt::alert_cast<lt::session_error_alert>(alert))
			{
				ErrorCallback errorCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					errorCbCopy = m_errorCb;
				}

				if (errorCbCopy)
				{
					errorCbCopy(se->message());
				}
			}
			else if (auto te = lt::alert_cast<lt::torrent_error_alert>(alert))
			{
				// Update status in database
				if (m_stateManager)
				{
					std::lock_guard mapLk(m_torrentMapMutex);
					auto it = m_handleToTaskId.find(te->handle);
					if (it != m_handleToTaskId.end())
					{
						m_stateManager->UpdateTaskStatus(it->second, 4); // Failed
					}
				}

				ErrorCallback errorCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					errorCbCopy = m_errorCb;
				}

				if (errorCbCopy)
				{
					errorCbCopy(te->message());
				}
			}
			else if (auto fe = lt::alert_cast<lt::file_error_alert>(alert))
			{
				ErrorCallback errorCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					errorCbCopy = m_errorCb;
				}

				if (errorCbCopy)
				{
					errorCbCopy(fe->message());
				}
			}
			else if (auto ma = lt::alert_cast<lt::metadata_received_alert>(alert))
			{
				// Update the task and persist a reusable .torrent file as soon
				// as a magnet has received its metadata.
				if (m_stateManager && ma->handle.is_valid())
				{
					try
					{
						auto status = ma->handle.status();
						std::string taskId;
						{
							std::lock_guard mapLk(m_torrentMapMutex);
							auto it = m_handleToTaskId.find(ma->handle);
							if (it != m_handleToTaskId.end())
							{
								taskId = it->second;
							}
						}
						if (!taskId.empty())
						{
							auto metaOpt = m_stateManager->LoadTaskMetadata(taskId);
							if (metaOpt.has_value())
							{
								TaskMetadata meta = metaOpt.value();
								meta.name = status.name;
								meta.totalSize = status.total_wanted;
								FillTaskInfoHashes(meta, ma->handle.info_hashes());
								m_stateManager->SaveTaskMetadata(meta);

								auto& settingsDb =
									::OpenNet::Core::AppSettingsDatabase::Instance();
								settingsDb.Initialize();
								WriteTorrentFile(
									ma->handle,
									taskId,
									meta.savePath,
									settingsDb.GetBool(
										::OpenNet::Core::AppSettingsDatabase::CAT_TORRENT,
										"saveTorrentCopyToDownloadDirectory")
									.value_or(false));
							}
						}
					}
					catch (...)
					{
					}
				}
			}
			else if (auto dsa = lt::alert_cast<lt::dht_stats_alert>(alert))
			{
				// One alert is emitted per local DHT endpoint (typically IPv4 and
				// IPv6). Do not overwrite the aggregate counter with the last
				// endpoint's table; an empty IPv6 table would otherwise erase a
				// healthy IPv4 count. session_stats_alert below is authoritative.
				(void)dsa;
			}
			else if (auto ssa = lt::alert_cast<lt::session_stats_alert>(alert))
			{
				// Cache session-level counters for use in GetSessionStats()
				if (!m_sessionStatsMetricsResolved)
				{
					ResolveSessionStatsMetricIndices();
				}

				auto const& counters = ssa->counters();
				auto const metrics = lt::session_stats_metrics();
				std::lock_guard lkStats(m_sessionStatsMutex);
				if (m_sessionMetricValues.empty())
					m_sessionMetricValues.reserve(metrics.size());
				for (auto const& metric : metrics)
				{
					if (metric.value_index >= 0
						&& metric.value_index < static_cast<int>(counters.size()))
					{
						m_sessionMetricValues[metric.name] = counters[metric.value_index];
					}
				}
				if (m_sessionStatsMetricIdxRecvBytes >= 0)
					m_sessionTotalDownload = counters[m_sessionStatsMetricIdxRecvBytes];
				if (m_sessionStatsMetricIdxSentBytes >= 0)
					m_sessionTotalUpload = counters[m_sessionStatsMetricIdxSentBytes];
				if (m_sessionStatsMetricIdxDhtNodes >= 0)
					m_cachedDhtNodeCount.store(static_cast<int>(counters[m_sessionStatsMetricIdxDhtNodes]));
				if (m_sessionStatsMetricIdxDiskBlocksInUse >= 0)
					m_sessionDiskBlocksInUse = counters[m_sessionStatsMetricIdxDiskBlocksInUse];
				if (m_sessionStatsMetricIdxDhtBytesReceived >= 0)
					m_sessionDhtBytesReceived = counters[m_sessionStatsMetricIdxDhtBytesReceived];
				if (m_sessionStatsMetricIdxDhtBytesSent >= 0)
					m_sessionDhtBytesSent = counters[m_sessionStatsMetricIdxDhtBytesSent];
			}
		}
	}

	void LibtorrentHandle::ResolveSessionStatsMetricIndices()
	{
		auto metrics = lt::session_stats_metrics();
		for (auto const& m : metrics)
		{
			if (m.name == std::string("net.recv_payload_bytes"))
				m_sessionStatsMetricIdxRecvBytes = m.value_index;
			else if (m.name == std::string("net.sent_payload_bytes"))
				m_sessionStatsMetricIdxSentBytes = m.value_index;
			else if (m.name == std::string("dht.dht_nodes"))
				m_sessionStatsMetricIdxDhtNodes = m.value_index;
			else if (m.name == std::string("disk.disk_blocks_in_use"))
				m_sessionStatsMetricIdxDiskBlocksInUse = m.value_index;
			else if (m.name == std::string("dht.dht_bytes_in"))
				m_sessionStatsMetricIdxDhtBytesReceived = m.value_index;
			else if (m.name == std::string("dht.dht_bytes_out"))
				m_sessionStatsMetricIdxDhtBytesSent = m.value_index;
		}
		m_sessionStatsMetricsResolved = true;
	}

	template<typename TAlert>
	void LibtorrentHandle::HandleSaveResumeDataAlert(TAlert const* alert)
	{
		if (!m_stateManager)
		{
			m_pendingResumeDataCount.fetch_sub(1);
			return;
		}
		if (!alert || !alert->handle.is_valid())
		{
			m_pendingResumeDataCount.fetch_sub(1);
			return;
		}

		try
		{
			std::string taskId;
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				auto const it = m_handleToTaskId.find(alert->handle);
				if (it != m_handleToTaskId.end())
					taskId = it->second;
			}
			if (!taskId.empty())
			{
				auto const data = lt::write_resume_data_buf(alert->params);
				auto const persisted = m_stateManager->SaveTaskResumeData(
					taskId, std::vector<std::uint8_t>(data.begin(), data.end()));
				std::lock_guard retryLock(m_resumeRetryMutex);
				if (persisted)
					m_resumeRetryTasks.erase(taskId);
				else
					m_resumeRetryTasks.insert(taskId);
			}
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("HandleSaveResumeDataAlert error: " + std::string(ex.what()) + "\n").c_str());
		}
		m_pendingResumeDataCount.fetch_sub(1);
	}

	template<typename TAlert>
	void LibtorrentHandle::HandleSaveResumeDataFailedAlert(TAlert const* alert)
	{
		m_pendingResumeDataCount.fetch_sub(1);
		if (alert && !IsLibtorrentError(
			alert->error, lt::errors::resume_data_not_modified))
		{
			OutputDebugStringA(("Save resume data failed: " + alert->message() + "\n").c_str());
			std::string taskId;
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				auto const task = m_handleToTaskId.find(alert->handle);
				if (task != m_handleToTaskId.end()) taskId = task->second;
			}
			if (!taskId.empty())
			{
				std::lock_guard retryLock(m_resumeRetryMutex);
				m_resumeRetryTasks.insert(taskId);
			}
		}
	}

	template<typename THandle>
	void LibtorrentHandle::RequestResumeDataForTorrent(
		THandle const& handle,
		bool const onlyIfModified)
	{
		if (handle.is_valid())
		{
			// Account for the request before handing it to libtorrent. The alert
			// loop runs on another thread and may deliver the completion before
			// save_resume_data() returns; incrementing afterwards could transiently
			// underflow the counter and let shutdown skip the persisted piece map.
			m_pendingResumeDataCount.fetch_add(1);
			try
			{
				auto flags = lt::torrent_handle::save_info_dict;
				if (onlyIfModified)
					flags |= lt::torrent_handle::only_if_modified;
				handle.save_resume_data(flags);
			}
			catch (...)
			{
				m_pendingResumeDataCount.fetch_sub(1);
			}
		}
	}

	void LibtorrentHandle::ReloadSettings(
		std::optional<int> const downloadRateOverride,
		std::optional<int> const uploadRateOverride)
	{
		if (m_session)
		{
			auto const currentSettings = m_session->get_settings();
			lt::settings_pack pack;
			ConfigureDefaultSettings(pack);
			if (downloadRateOverride)
				pack.set_int(lt::settings_pack::download_rate_limit, *downloadRateOverride);
			if (uploadRateOverride)
				pack.set_int(lt::settings_pack::upload_rate_limit, *uploadRateOverride);
			auto const targetNatPmpEnabled = pack.get_bool(lt::settings_pack::enable_natpmp);
			auto const restartNatPmp = targetNatPmpEnabled
				&& currentSettings.get_bool(lt::settings_pack::enable_natpmp)
				&& currentSettings.get_str(lt::settings_pack::natpmp_gateway) != pack.get_str(lt::settings_pack::natpmp_gateway);
			if (restartNatPmp) pack.set_bool(lt::settings_pack::enable_natpmp, false);
			m_session->apply_settings(pack);
			if (restartNatPmp)
			{
				lt::settings_pack restartPack;
				restartPack.set_bool(lt::settings_pack::enable_natpmp, true);
				m_session->apply_settings(restartPack);
			}
		}
	}

	LibtorrentHandle::RuntimeSettingsSnapshot
		LibtorrentHandle::GetRuntimeSettings() const
	{
		RuntimeSettingsSnapshot result{};
		if (!m_session) return result;
		auto const settings = m_session->get_settings();
		result.maxQueuedDiskBytes = settings.get_int(
			lt::settings_pack::max_queued_disk_bytes);
		result.receiveSocketBufferSize = settings.get_int(
			lt::settings_pack::recv_socket_buffer_size);
		result.sendSocketBufferSize = settings.get_int(
			lt::settings_pack::send_socket_buffer_size);
		result.dhtUploadRateLimit = settings.get_int(
			lt::settings_pack::dht_upload_rate_limit);
		return result;
	}

	std::optional<int> LibtorrentHandle::TryKilobytesToBytesPerSecond(
		double const value)
	{
		if (!std::isfinite(value) || value < 0.0
			|| value > static_cast<double>((std::numeric_limits<int>::max)()) / 1024.0)
		{
			return std::nullopt;
		}
		auto const bytes = static_cast<std::int64_t>(value * 1024.0);
		if (bytes > (std::numeric_limits<int>::max)())
			return std::nullopt;
		return static_cast<int>(bytes);
	}

	LibtorrentHandle::ListenStatus LibtorrentHandle::GetListenStatus() const
	{
		ListenStatus status{};
		if (!m_session)
		{
			return status;
		}

		{
			std::lock_guard lock(m_listenStateMutex);
			status.ipv4TcpPort = m_ipv4TcpListenPort;
			status.ipv4UtpPort = m_ipv4UtpListenPort;
			status.ipv6TcpPort = m_ipv6TcpListenPort;
			status.ipv6UtpPort = m_ipv6UtpListenPort;
			status.ipv4Port = status.ipv4TcpPort > 0
				? status.ipv4TcpPort
				: status.ipv4UtpPort;
			status.ipv6Port = status.ipv6TcpPort > 0
				? status.ipv6TcpPort
				: status.ipv6UtpPort;
			status.isListeningIPv4 =
				status.ipv4TcpPort > 0 || status.ipv4UtpPort > 0;
			status.isListeningIPv6 =
				status.ipv6TcpPort > 0 || status.ipv6UtpPort > 0;
			status.error = m_lastListenError;
			status.ipv4Error = m_ipv4ListenError;
			status.ipv6Error = m_ipv6ListenError;
		}

		try
		{
			status.isListening = m_session->is_listening()
				|| status.isListeningIPv4 || status.isListeningIPv6;
			status.port = status.ipv4Port > 0
				? status.ipv4Port
				: status.ipv6Port;
			// listen_succeeded_alert is normally processed before consumers ask
			// for status. Keep the session API as a compatibility fallback during
			// the very small startup window before that alert is dispatched.
			if (status.port == 0 && status.isListening)
				status.port = static_cast<int>(m_session->listen_port());
		}
		catch (...)
		{
			status.isListening = status.isListeningIPv4 || status.isListeningIPv6;
			status.port = status.ipv4Port > 0 ? status.ipv4Port : status.ipv6Port;
		}
		return status;
	}

	void LibtorrentHandle::ReloadIpFilter()
	{
		if (m_session)
		{
			m_session->set_ip_filter(
				::OpenNet::Core::IPFilterManager::Instance().BuildSessionFilter());
		}
	}

	LibtorrentHandle::PortMappingStatus LibtorrentHandle::GetPortMappingStatus() const
	{
		std::lock_guard lock(m_portMappingMutex);
		auto status = m_portMappingStatus;
		if (m_session)
		{
			auto settings = m_session->get_settings();
			status.upnpEnabled = settings.get_bool(lt::settings_pack::enable_upnp);
			status.natPmpEnabled = settings.get_bool(lt::settings_pack::enable_natpmp);
		}
		return status;
	}

	void LibtorrentHandle::RefreshPortMappings()
	{
		if (!m_session)
			return;
		{
			std::lock_guard lock(m_portMappingMutex);
			m_portMappingStatus.tcpExternalPort = 0;
			m_portMappingStatus.udpExternalPort = 0;
			m_portMappingStatus.tcpMechanism.clear();
			m_portMappingStatus.udpMechanism.clear();
			m_portMappingStatus.lastError.clear();
		}
		{
			std::lock_guard lock(m_listenStateMutex);
			m_ipv4ListenPort = 0;
			m_ipv6ListenPort = 0;
			m_ipv4TcpListenPort = 0;
			m_ipv4UtpListenPort = 0;
			m_ipv6TcpListenPort = 0;
			m_ipv6UtpListenPort = 0;
			m_ipv4ListenError.clear();
			m_ipv6ListenError.clear();
		}
		m_session->reopen_network_sockets(lt::session_handle::reopen_map_ports);
	}

	// ---------------------------------------------------------------
	//  Session-level aggregate statistics
	// ---------------------------------------------------------------
	LibtorrentHandle::SessionStats LibtorrentHandle::GetPerformanceStats() const
	{
		SessionStats stats{};
		stats.dhtNodes = m_cachedDhtNodeCount.load();
		stats.internalBannedIps = m_internalBanCount.load(std::memory_order_relaxed);
		{
			std::lock_guard lock(m_sessionStatsMutex);
			stats.totalDownloadRate = m_cachedDownloadRate;
			stats.totalUploadRate = m_cachedUploadRate;
			stats.longTermSeedingUploadRate = m_cachedLongTermSeedingUploadRate;
			stats.totalDownloaded = m_sessionTotalDownload;
			stats.totalUploaded = m_sessionTotalUpload;
			stats.diskCacheBytes = m_sessionDiskBlocksInUse * 16 * 1024;
			stats.dhtBytesReceived = m_sessionDhtBytesReceived;
			stats.dhtBytesSent = m_sessionDhtBytesSent;
			stats.numTorrents = static_cast<int>(m_cachedTorrentRates.size());

			std::int64_t cachedTotalDone{};
			std::int64_t cachedTotalUpload{};
			for (auto const& [handle, torrent] : m_cachedTorrentRates)
			{
				(void)handle;
				cachedTotalDone += torrent.totalDone;
				cachedTotalUpload += torrent.totalUpload;
				stats.numPeers += torrent.numPeers;
				stats.numSeeds += torrent.numSeeds;
				if (torrent.isPaused)
					++stats.numPausedTorrents;
				else
					++stats.numRunningTorrents;

				switch (torrent.state)
				{
					case lt::torrent_status::downloading_metadata:
						++stats.numMetadataTorrents;
						++stats.numDownloadingTorrents;
						break;
					case lt::torrent_status::downloading:
						++stats.numDownloadingTorrents;
						break;
					case lt::torrent_status::finished:
					case lt::torrent_status::seeding:
						++stats.numSeedingTorrents;
						break;
					case lt::torrent_status::checking_files:
					case lt::torrent_status::checking_resume_data:
						++stats.numCheckingTorrents;
						break;
					default:
						break;
				}
				if (torrent.hasError)
					++stats.numErrorTorrents;
			}
			if (stats.totalDownloaded <= 0)
				stats.totalDownloaded = cachedTotalDone;
			if (stats.totalUploaded <= 0)
				stats.totalUploaded = cachedTotalUpload;
		}

		// Socket state is independent of torrent status and cheap to read. Do it
		// after releasing the statistics lock to keep alert dispatch unblocked.
		auto const listenStatus = GetListenStatus();
		stats.isListening = listenStatus.isListening;
		stats.listenPort = listenStatus.port;
		stats.ipv4ListenPort = listenStatus.ipv4Port;
		stats.ipv6ListenPort = listenStatus.ipv6Port;
		stats.isListeningIPv4 = listenStatus.isListeningIPv4;
		stats.isListeningIPv6 = listenStatus.isListeningIPv6;
		stats.listenError = listenStatus.error;
		return stats;
	}

	LibtorrentHandle::SessionStats LibtorrentHandle::GetSessionStats() const
	{
		SessionStats stats{};
		stats.internalBannedIps = m_internalBanCount.load(std::memory_order_relaxed);
		if (!m_session)
			return stats;

		try
		{
			// Aggregate from all torrent handles
			auto torrents = m_session->get_torrents();
			stats.numTorrents = static_cast<int>(torrents.size());

			for (auto const& h : torrents)
			{
				if (!h.is_valid())
					continue;
				auto st = h.status(lt::torrent_handle::query_accurate_download_counters);
				stats.totalDownloadRate += st.download_rate;
				stats.totalUploadRate += st.upload_rate;
				stats.totalDownloaded += st.total_done;
				stats.totalUploaded += st.total_upload;
				stats.numPeers += st.num_peers;
				stats.numSeeds += st.num_seeds;
				if ((st.flags & lt::torrent_flags::paused)
					!= lt::torrent_flags_t{})
				{
					++stats.numPausedTorrents;
				}
				else
				{
					++stats.numRunningTorrents;
				}

				switch (st.state)
				{
					case lt::torrent_status::downloading_metadata:
						++stats.numMetadataTorrents;
						++stats.numDownloadingTorrents;
						break;
					case lt::torrent_status::downloading:
						++stats.numDownloadingTorrents;
						break;
					case lt::torrent_status::finished:
					case lt::torrent_status::seeding:
						++stats.numSeedingTorrents;
						break;
					case lt::torrent_status::checking_files:
					case lt::torrent_status::checking_resume_data:
						++stats.numCheckingTorrents;
						break;
					default:
						break;
				}
				if (st.errc)
					++stats.numErrorTorrents;
				if (st.state == lt::torrent_status::finished
					|| st.state == lt::torrent_status::seeding)
				{
					stats.longTermSeedingUploadRate += st.upload_rate;
				}
			}

			// DHT nodes — use the cached value from dht_stats_alert / session_stats_alert
			stats.dhtNodes = m_cachedDhtNodeCount.load();

			// Only expose the socket libtorrent actually opened. Reading this
			// through the lightweight status API keeps every UI consumer aligned.
			auto const listenStatus = GetListenStatus();
			stats.isListening = listenStatus.isListening;
			stats.listenPort = listenStatus.port;
			stats.ipv4ListenPort = listenStatus.ipv4Port;
			stats.ipv6ListenPort = listenStatus.ipv6Port;
			stats.isListeningIPv4 = listenStatus.isListeningIPv4;
			stats.isListeningIPv6 = listenStatus.isListeningIPv6;
			stats.listenError = listenStatus.error;

			// Session-level totals from session_stats_alert (more accurate than per-torrent sums)
			{
				std::lock_guard lkStats(m_sessionStatsMutex);
				if (m_sessionTotalDownload > 0)
					stats.totalDownloaded = m_sessionTotalDownload;
				if (m_sessionTotalUpload > 0)
					stats.totalUploaded = m_sessionTotalUpload;
				// Libtorrent's disk block metric counts 16 KiB blocks.
				stats.diskCacheBytes = m_sessionDiskBlocksInUse * 16 * 1024;
				stats.dhtBytesReceived = m_sessionDhtBytesReceived;
				stats.dhtBytesSent = m_sessionDhtBytesSent;
			}
		}
		catch (...)
		{
		}

		return stats;
	}

	std::unordered_map<std::string, std::int64_t>
		LibtorrentHandle::GetSessionMetrics() const
	{
		std::lock_guard lock(m_sessionStatsMutex);
		return m_sessionMetricValues;
	}

	// ---------------------------------------------------------------
	//  Per-torrent detail
	// ---------------------------------------------------------------
	std::vector<LibtorrentHandle::PeerConnectionEvent>
		LibtorrentHandle::GetRecentPeerEvents(
			std::string const& taskId,
			std::int64_t maxAgeSeconds) const
	{
		std::vector<PeerConnectionEvent> result;
		auto const cutoff = maxAgeSeconds > 0
			? std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()).count()
			- maxAgeSeconds
			: 0;

		std::lock_guard lock(m_peerEventMutex);
		auto const found = m_peerEvents.find(taskId);
		if (found == m_peerEvents.end())
			return result;
		result.reserve(found->second.size());
		for (auto const& event : found->second)
		{
			if (cutoff == 0 || event.timestamp >= cutoff)
				result.push_back(event);
		}
		return result;
	}

	LibtorrentHandle::TorrentDetailInfo LibtorrentHandle::GetTorrentDetail(
		std::string const& taskId) const
	{
		return GetTorrentDetailImpl(taskId, true, true, true);
	}

	LibtorrentHandle::TorrentDetailInfo LibtorrentHandle::GetTorrentSummary(std::string const& taskId) const
	{
		return GetTorrentDetailImpl(taskId, false, false, false);
	}

	std::vector<LibtorrentHandle::TorrentTrackerInfo> LibtorrentHandle::GetTorrentTrackers(std::string const& taskId) const
	{
		return GetTorrentDetailImpl(taskId, false, true, false).trackers;
	}

	LibtorrentHandle::TorrentDetailInfo LibtorrentHandle::GetTorrentFilesSnapshot(std::string const& taskId) const
	{
		return GetTorrentDetailImpl(taskId, false, false, true);
	}

	LibtorrentHandle::TorrentDetailInfo LibtorrentHandle::GetTorrentDetailImpl(std::string const& taskId, bool const includePeers, bool const includeTrackers, bool const includeFiles) const
	{
		TorrentDetailInfo info{};
		info.taskId = taskId;

		lt::torrent_handle handle;
		{
			std::lock_guard lk(m_torrentMapMutex);
			auto it = m_taskIdToHandle.find(taskId);
			if (it == m_taskIdToHandle.end())
				return info;
			handle = it->second;
		}

		if (!handle.is_valid())
			return info;

		try
		{
			auto st = handle.status();
			info.name = st.name;
			info.savePath = st.save_path;
			info.totalSize = st.total_wanted;
			info.totalDone = st.total_done;
			info.totalUploaded = st.total_upload;
			info.sessionDownloaded = st.total_download;
			info.sessionUploaded = st.total_upload;
			info.allTimeDownloaded = st.all_time_download;
			info.allTimeUploaded = st.all_time_upload;
			info.downloadRate = st.download_rate;
			info.uploadRate = st.upload_rate;
			info.progressPpm = static_cast<int>(st.progress_ppm);
			info.numPeers = st.num_peers;
			info.numSeeds = st.num_seeds;
			info.numConnections = st.num_connections;
			info.numComplete = (std::max)(st.num_complete, st.list_seeds);
			info.numIncomplete = (std::max)(
				st.num_incomplete, (std::max)(0, st.list_peers - st.list_seeds));
			info.state = static_cast<int>(st.state);
			info.addedTimestamp =
				static_cast<std::int64_t>(st.added_time);
			info.completedTimestamp =
				static_cast<std::int64_t>(st.completed_time);
			info.activeTimeSeconds = st.active_duration.count();
			info.seedingTimeSeconds = st.finished_duration.count();
			info.isPaused = (st.flags & lt::torrent_flags::paused) != lt::torrent_flags_t{};
			info.isAutoManaged =
				bool(st.flags & lt::torrent_flags::auto_managed);
			info.isSequential =
				bool(st.flags & lt::torrent_flags::sequential_download);
			info.isSuperSeeding =
				bool(st.flags & lt::torrent_flags::super_seeding);
			info.queuePosition =
				static_cast<int>(handle.queue_position());

			if (st.all_time_download > 0)
				info.shareRatio = static_cast<double>(st.all_time_upload) /
				st.all_time_download;

			Impl::CachedTorrentMetadata metadata;
			bool metadataCached{};
			{
				std::lock_guard lock(m_torrentMetadataMutex);
				auto const cached = m_torrentMetadataCache.find(handle);
				if (cached != m_torrentMetadataCache.end())
				{
					metadata = cached->second;
					metadataCached = metadata.metadataLoaded;
				}
			}
			if (!metadataCached)
			{
				auto const hashes = handle.info_hashes();
				if (hashes.has_v1())
				{
					std::ostringstream stream;
					stream << hashes.v1;
					metadata.infoHashV1 = stream.str();
				}
				if (hashes.has_v2())
				{
					std::ostringstream stream;
					stream << hashes.v2;
					metadata.infoHashV2 = stream.str();
				}
				{
					std::ostringstream stream;
					stream << hashes.get_best();
					metadata.apiHash = stream.str();
				}
				metadata.infoHash = !metadata.infoHashV1.empty() ? metadata.infoHashV1 : metadata.infoHashV2;
				if (auto torrentInfo = handle.torrent_file())
				{
					metadata.isPrivate = torrentInfo->priv();
					metadata.pieceSize = torrentInfo->piece_length();
					metadata.piecesNum = torrentInfo->num_pieces();
					metadata.isPieceAligned = metadata.pieceSize > 0;
					auto const& files = torrentInfo->layout();
					for (int index = 0; index < files.num_files() && metadata.isPieceAligned; ++index)
					{
						auto const file = lt::file_index_t{ index };
						if (!files.pad_file_at(file) && files.file_offset(file) % metadata.pieceSize != 0) metadata.isPieceAligned = false;
					}
					auto const params = handle.get_resume_data(lt::torrent_handle::save_info_dict);
					metadata.comment = params.comment;
					metadata.creator = params.created_by;
					metadata.creationTimestamp = static_cast<std::int64_t>(params.creation_date);
					metadata.metadataLoaded = true;
					std::lock_guard lock(m_torrentMetadataMutex);
					m_torrentMetadataCache.insert_or_assign(handle, metadata);
				}
			}
			info.infoHash = metadata.infoHash;
			info.infoHashV1 = metadata.infoHashV1;
			info.infoHashV2 = metadata.infoHashV2;
			info.apiHash = metadata.apiHash;
			info.comment = metadata.comment;
			info.creator = metadata.creator;
			info.creationTimestamp = metadata.creationTimestamp;
			info.pieceSize = metadata.pieceSize;
			info.piecesNum = metadata.piecesNum;
			info.isPrivate = metadata.isPrivate;
			info.isPieceAligned = metadata.isPieceAligned;
			// Peers
			if (includePeers)
			{
				info.peers = GetTorrentPeers(taskId);
			}

			// Trackers
			if (includeTrackers)
			{
				{
					std::lock_guard lock(m_trackerSnapshotMutex);
					if (auto const cached = m_trackerSnapshots.find(handle);
						cached != m_trackerSnapshots.end())
						info.trackers = cached->second;
				}
				if (ShouldRequestDetails(handle, 1)) handle.post_trackers();
				if (info.trackers.empty())
				{
					// Mutation callers also use this snapshot. Bootstrap from confirmed
					// engine state so an unopened page cannot appear to have no trackers.
					info.trackers = ConvertTrackers(handle, handle.trackers());
				}
			}

			// Files
			if (includeFiles)
			{
				auto ti = handle.torrent_file();
				if (!ti) return info;
				info.pieceSize = ti->piece_length();
				info.piecesNum = ti->num_pieces();
				const auto piecePriorities =
					handle.get_piece_priorities();
				info.firstLastPiecePriority =
					!piecePriorities.empty()
					&& static_cast<std::uint8_t>(
						piecePriorities.front()) > 4
					&& static_cast<std::uint8_t>(
						piecePriorities.back()) > 4;
				auto const& fs = ti->layout();
				auto const renamedFiles = handle.get_renamed_files();
				lt::filenames const currentNames(fs, renamedFiles);
				int const numFiles = fs.num_files();
				std::vector<std::int64_t> fileProgress;
				{
					std::lock_guard lock(m_fileProgressMutex);
					if (auto const cached = m_fileProgressCache.find(handle);
						cached != m_fileProgressCache.end())
						fileProgress = cached->second;
				}
				if (ShouldRequestDetails(handle, 2))
					handle.post_file_progress(lt::torrent_handle::piece_granularity);
				std::vector<lt::download_priority_t> filePriorities;
				{
					std::lock_guard lock(m_filePrioritiesMutex);
					auto const cached = m_filePrioritiesCache.find(handle);
					if (cached != m_filePrioritiesCache.end()) filePriorities = cached->second;
				}
				if (filePriorities.empty())
				{
					// Unknown priorities must never be written back as "download all".
					filePriorities = handle.get_file_priorities();
					std::lock_guard mapLock(m_torrentMapMutex);
					if (m_handleToTaskId.contains(handle))
					{
						std::lock_guard cacheLock(m_filePrioritiesMutex);
						m_filePrioritiesCache.insert_or_assign(handle, filePriorities);
					}
				}
				if (ShouldRequestDetails(handle, 3)) handle.post_file_priorities();
				info.isPieceAligned = info.pieceSize > 0;
				for (int i = 0; i < numFiles && info.isPieceAligned; ++i)
				{
					auto const fileIndex = lt::file_index_t{ i };
					if (!fs.pad_file_at(fileIndex)
						&& fs.file_offset(fileIndex) % info.pieceSize != 0)
					{
						info.isPieceAligned = false;
					}
				}

				info.files.reserve(numFiles);
				for (int i = 0; i < numFiles; ++i)
				{
					TorrentFileEntry fe;
					auto const fileIndex = lt::file_index_t{ i };
					fe.originalPath = fs.file_path(fileIndex);
					fe.path = currentNames.file_path(fileIndex);
					fe.size = fs.file_size(fileIndex);
					fe.fileIndex = i;
					fe.isPadFile = fs.pad_file_at(fileIndex);
					if (fe.size > 0)
					{
						fe.firstPiece = static_cast<int>(
							fs.map_file(
								lt::file_index_t{ i }, 0, 0).piece);
						fe.lastPiece = static_cast<int>(
							fs.map_file(
								lt::file_index_t{ i },
								fe.size - 1, 0).piece);
					}
					if (i < static_cast<int>(fileProgress.size()))
						fe.bytesCompleted = fileProgress[i];
					if (i < static_cast<int>(filePriorities.size()))
						fe.priority = static_cast<int>(static_cast<std::uint8_t>(filePriorities[i]));
					info.files.push_back(std::move(fe));
				}
			}
		}
		catch (...)
		{
		}

		return info;
	}

	template<typename THandle>
	bool LibtorrentHandle::ShouldRequestDetails(THandle const& handle, std::size_t kind) const
	{
		std::lock_guard mapLock(m_torrentMapMutex);
		if (!m_handleToTaskId.contains(handle) || kind >= 5) return false;
		std::lock_guard requestLock(m_detailRequestMutex);
		auto const now = std::chrono::steady_clock::now();
		auto& last = m_detailRequests[handle][kind];
		if (now - last < std::chrono::seconds(1)) return false;
		last = now;
		return true;
	}

	std::vector<LibtorrentHandle::TorrentPeerInfo>
		LibtorrentHandle::GetTorrentPeers(std::string const& taskId) const
	{
		lt::torrent_handle handle;
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const found = m_taskIdToHandle.find(taskId);
			if (found == m_taskIdToHandle.end())
				return {};
			handle = found->second;
		}

		if (!handle.is_valid())
			return {};

		try
		{
			if (ShouldRequestDetails(handle, 0)) handle.post_peer_info();
			std::lock_guard lock(m_peerSnapshotMutex);
			if (auto const snapshot = m_peerSnapshots.find(handle);
				snapshot != m_peerSnapshots.end())
				return snapshot->second;
			return {};
		}
		catch (...)
		{
			// The torrent can disappear between copying the handle and querying it.
			return {};
		}
	}

	LibtorrentHandle::TorrentPieceInfo
		LibtorrentHandle::GetTorrentPieceInfo(
			std::string const& taskId) const
	{
		TorrentPieceInfo result;
		lt::torrent_handle handle;
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const item = m_taskIdToHandle.find(taskId);
			if (item == m_taskIdToHandle.end() || !item->second.is_valid()) return result;
			handle = item->second;
		}
		if (!handle.is_valid())
		{
			return result;
		}

		try
		{
			const auto torrentInfo = handle.torrent_file();
			if (!torrentInfo)
				return result;

			result.pieceSize = torrentInfo->piece_length();
			const int pieceCount = torrentInfo->num_pieces();
			result.states.assign(
				static_cast<std::size_t>(pieceCount), 0);
			const auto status =
				handle.status(lt::torrent_handle::query_pieces);
			const auto priorities = handle.get_piece_priorities();
			for (int index = 0;
				 index < pieceCount
				 && index < status.pieces.size();
				 ++index)
			{
				if (status.pieces[lt::piece_index_t{ index }])
					result.states[static_cast<std::size_t>(index)] = 2;
				else if (index < static_cast<int>(priorities.size())
						 && static_cast<std::uint8_t>(priorities[
							 static_cast<std::size_t>(index)]) == 0)
					result.states[static_cast<std::size_t>(index)] = 3;
				else if (status.state == lt::torrent_status::checking_files
						 || status.state == lt::torrent_status::checking_resume_data)
					result.states[static_cast<std::size_t>(index)] = 4;
			}

			std::vector<TorrentPeerInfo> peers;
			{
				std::lock_guard lock(m_peerSnapshotMutex);
				if (auto const snapshot = m_peerSnapshots.find(handle);
					snapshot != m_peerSnapshots.end())
					peers = snapshot->second;
			}
			if (ShouldRequestDetails(handle, 0)) handle.post_peer_info();
			for (const auto& peer : peers)
			{
				const int index = peer.downloadingPiece;
				if (index >= 0 && index < pieceCount
					&& result.states[
						static_cast<std::size_t>(index)] == 0)
				{
					result.states[
						static_cast<std::size_t>(index)] = 1;
				}
			}

			{
				std::lock_guard lock(m_pieceAvailabilityMutex);
				if (auto const cached = m_pieceAvailabilityCache.find(handle);
					cached != m_pieceAvailabilityCache.end())
					result.availability = cached->second;
			}
			if (ShouldRequestDetails(handle, 4)) handle.post_piece_availability();
			{
				std::lock_guard lock(m_torrentMetadataMutex);
				auto const cached = m_torrentMetadataCache.find(handle);
				if (cached != m_torrentMetadataCache.end()) result.hashes = cached->second.pieceHashes;
			}
			if (result.hashes.empty()
				&& torrentInfo->info_hashes().has_v1())
			{
				result.hashes.reserve(static_cast<std::size_t>(pieceCount));
				for (int index = 0; index < pieceCount; ++index)
				{
					std::ostringstream stream;
					stream << torrentInfo->hash_for_piece(lt::piece_index_t{ index });
					result.hashes.push_back(stream.str());
				}
				std::lock_guard lock(m_torrentMetadataMutex);
				m_torrentMetadataCache[handle].pieceHashes = result.hashes;
			}
			auto const urlSeeds = handle.url_seeds();
			result.webSeeds.reserve(urlSeeds.size());
			result.webSeeds.insert(result.webSeeds.end(), urlSeeds.begin(), urlSeeds.end());
		}
		catch (...)
		{
		}
		return result;
	}

	LibtorrentHandle::TorrentPieceInfo LibtorrentHandle::GetTorrentPieceSummary(std::string const& taskId) const
	{
		TorrentPieceInfo result;
		lt::torrent_handle handle;
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const item = m_taskIdToHandle.find(taskId);
			if (item == m_taskIdToHandle.end() || !item->second.is_valid()) return result;
			handle = item->second;
		}
		try
		{
			auto const torrentInfo = handle.torrent_file();
			if (!torrentInfo) return result;
			result.pieceSize = torrentInfo->piece_length();
			auto const pieceCount = torrentInfo->num_pieces();
			result.states.assign(static_cast<std::size_t>(pieceCount), 0);
			auto const status = handle.status(lt::torrent_handle::query_pieces);
			for (int index = 0; index < pieceCount && index < status.pieces.size(); ++index)
			{
				if (status.pieces[lt::piece_index_t{ index }]) result.states[static_cast<std::size_t>(index)] = 2;
			}
			{
				std::lock_guard lock(m_pieceAvailabilityMutex);
				if (auto const cached = m_pieceAvailabilityCache.find(handle);
					cached != m_pieceAvailabilityCache.end())
					result.availability = cached->second;
			}
			if (ShouldRequestDetails(handle, 4)) handle.post_piece_availability();
		}
		catch (...)
		{
		}
		return result;
	}

	std::vector<std::uint8_t> LibtorrentHandle::ExportTorrentFile(
		std::string const& taskId) const
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return {};
		}

		try
		{
			auto params = item->second.get_resume_data(
				lt::torrent_handle::save_info_dict);
			auto encoded = lt::write_torrent_file_buf(
				params, lt::write_torrent_flags_t{});
			return { encoded.begin(), encoded.end() };
		}
		catch (...)
		{
			return {};
		}
	}

	// ---------------------------------------------------------------
	//  Force recheck
	// ---------------------------------------------------------------
	bool LibtorrentHandle::ForceRecheck(std::string const& taskId)
	{
		lt::torrent_handle handle;
		bool restorePause{};
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const item = m_taskIdToHandle.find(taskId);
			if (item == m_taskIdToHandle.end() || !item->second.is_valid()
				|| !item->second.torrent_file())
				return false;
			handle = item->second;
			restorePause = (handle.flags() & lt::torrent_flags::paused)
				!= lt::torrent_flags_t{};
			if (restorePause)
				m_recheckCompletionActions.insert_or_assign(
					taskId, RecheckCompletionAction::Pause);
		}

		handle.force_recheck();
		if (restorePause)
		{
			handle.unset_flags(lt::torrent_flags::auto_managed);
			handle.resume();
		}
		return true;
	}

	void LibtorrentHandle::ForceReannounce(std::string const& taskId)
	{
		std::lock_guard lk(m_torrentMapMutex);
		const auto it = m_taskIdToHandle.find(taskId);
		if (it == m_taskIdToHandle.end() || !it->second.is_valid())
			return;

		// Pausing a task is also a request to stop its network activity.  Do not
		// let UI/WebUI tracker actions bypass that state.
		if ((it->second.flags() & lt::torrent_flags::paused)
			!= lt::torrent_flags_t{})
			return;

		it->second.force_reannounce(0, lt::torrent_handle::high_priority);
	}

	void LibtorrentHandle::ForceReannounceTracker(
		std::string const& taskId,
		std::string const& trackerUrl)
	{
		std::lock_guard lock(m_torrentMapMutex);
		auto const found = m_taskIdToHandle.find(taskId);
		if (found == m_taskIdToHandle.end() || !found->second.is_valid())
			return;
		if ((found->second.flags() & lt::torrent_flags::paused)
			!= lt::torrent_flags_t{})
			return;

		auto const trackers = found->second.trackers();
		for (std::size_t index = 0; index < trackers.size(); ++index)
		{
			if (trackers[index].url != trackerUrl)
				continue;
			found->second.force_reannounce(0, static_cast<int>(index), lt::torrent_handle::ignore_min_interval | lt::torrent_handle::high_priority);
			found->second.scrape_tracker(static_cast<int>(index));
			return;
		}
	}

	std::vector<LibtorrentHandle::TrackerLogEntry>
		LibtorrentHandle::GetTrackerLog(
			std::string const& taskId,
			std::string const& trackerUrl) const
	{
		std::lock_guard lock(m_trackerLogMutex);
		auto const task = m_trackerLogs.find(taskId);
		if (task == m_trackerLogs.end())
			return {};
		auto const tracker = task->second.find(trackerUrl);
		if (tracker == task->second.end())
			return {};
		return std::vector<TrackerLogEntry>(
			tracker->second.begin(), tracker->second.end());
	}

	std::optional<LibtorrentHandle::TrackerLogEntry>
		LibtorrentHandle::GetLatestTrackerLog(
			std::string const& taskId,
			std::string const& trackerUrl) const
	{
		std::lock_guard lock(m_trackerLogMutex);
		auto const task = m_trackerLogs.find(taskId);
		if (task == m_trackerLogs.end())
			return std::nullopt;
		auto const tracker = task->second.find(trackerUrl);
		if (tracker == task->second.end() || tracker->second.empty())
			return std::nullopt;
		return tracker->second.back();
	}

	void LibtorrentHandle::ClearTrackerLog(
		std::string const& taskId,
		std::string const& trackerUrl)
	{
		std::lock_guard lock(m_trackerLogMutex);
		auto const task = m_trackerLogs.find(taskId);
		if (task == m_trackerLogs.end())
			return;
		task->second.erase(trackerUrl);
		if (task->second.empty())
			m_trackerLogs.erase(task);
	}

	void LibtorrentHandle::ClearTrackerLogs(std::string const& taskId)
	{
		std::lock_guard lock(m_trackerLogMutex);
		m_trackerLogs.erase(taskId);
	}

	void LibtorrentHandle::ClearAllTrackerLogs()
	{
		std::lock_guard lock(m_trackerLogMutex);
		m_trackerLogs.clear();
	}

	int LibtorrentHandle::GetTorrentDownloadLimit(
		std::string const& taskId) const
	{
		std::lock_guard lk(m_torrentMapMutex);
		const auto it = m_taskIdToHandle.find(taskId);
		if (it == m_taskIdToHandle.end() || !it->second.is_valid())
			return 0;
		return it->second.download_limit();
	}

	int LibtorrentHandle::GetTorrentUploadLimit(
		std::string const& taskId) const
	{
		std::lock_guard lk(m_torrentMapMutex);
		const auto it = m_taskIdToHandle.find(taskId);
		if (it == m_taskIdToHandle.end() || !it->second.is_valid())
			return 0;
		return it->second.upload_limit();
	}

	void LibtorrentHandle::SetTorrentDownloadLimit(std::string const& taskId, int const limit)
	{
		TorrentTaskPatch patch;
		patch.downloadLimit = limit;
		PatchTorrentTaskSettings(taskId, patch);
	}

	void LibtorrentHandle::SetTorrentUploadLimit(std::string const& taskId, int const limit)
	{
		TorrentTaskPatch patch;
		patch.uploadLimit = limit;
		PatchTorrentTaskSettings(taskId, patch);
	}

	LibtorrentHandle::TorrentTaskSettings LibtorrentHandle::GetTorrentTaskSettings(std::string const& taskId) const
	{
		std::lock_guard lock(m_torrentMapMutex);
		auto const item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end() || !item->second.is_valid()) return {};

		auto const handle = item->second;
		auto const flags = handle.flags();
		TorrentTaskSettings settings;
		settings.downloadLimit = std::max(0, handle.download_limit());
		settings.uploadLimit = std::max(0, handle.upload_limit());
		settings.maxConnections = handle.max_connections();
		settings.maxUploads = handle.max_uploads();
		settings.enableDht = (flags & lt::torrent_flags::disable_dht) == lt::torrent_flags_t{};
		settings.enableLsd = (flags & lt::torrent_flags::disable_lsd) == lt::torrent_flags_t{};
		settings.enablePex = (flags & lt::torrent_flags::disable_pex) == lt::torrent_flags_t{};
		settings.applyIpFilter = (flags & lt::torrent_flags::apply_ip_filter) != lt::torrent_flags_t{};
		settings.sequentialDownload = (flags & lt::torrent_flags::sequential_download) != lt::torrent_flags_t{};
		settings.superSeeding = (flags & lt::torrent_flags::super_seeding) != lt::torrent_flags_t{};
		settings.forceStart = (flags & (lt::torrent_flags::auto_managed | lt::torrent_flags::paused)) == lt::torrent_flags_t{};
		settings.uploadMode = (flags & lt::torrent_flags::upload_mode) != lt::torrent_flags_t{};
		settings.shareMode = (flags & lt::torrent_flags::share_mode) != lt::torrent_flags_t{};
		if (m_stateManager)
		{
			if (auto const stored = m_stateManager->LoadTaskSettings(taskId))
			{
				settings.minimumUploadRate = stored->minimumUploadRate;
				settings.shareRatioLimit = stored->shareRatioLimit;
				settings.seedingTimeLimit = stored->seedingTimeLimit;
				settings.inactiveSeedingTimeLimit = stored->inactiveSeedingTimeLimit;
				settings.shareLimitMatchAll = stored->shareLimitMatchAll;
				settings.shareLimitAction = stored->shareLimitAction;
				settings.completionAction = stored->completionAction;
			}
			else
			{
				auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
				settings.minimumUploadRate = static_cast<int>(database.GetInt(TorrentTaskSettingsCategory, TaskSettingKey(taskId, "minimumUploadRate"), 0));
				if (settings.minimumUploadRate > 0)
				{
					m_stateManager->SaveTaskSettings(PersistedTaskSettings(taskId, settings));
					database.Delete(TorrentTaskSettingsCategory, TaskSettingKey(taskId, "minimumUploadRate"));
				}
			}
		}
		return settings;
	}

	bool LibtorrentHandle::SetTorrentTaskSettings(std::string const& taskId, TorrentTaskSettings const& settings)
	{
		std::lock_guard commandLock(m_taskCommandMutex);
		return ApplyTorrentTaskSettings(taskId, settings);
	}

	bool LibtorrentHandle::PatchTorrentTaskSettings(std::string const& taskId, TorrentTaskPatch const& patch)
	{
		std::lock_guard commandLock(m_taskCommandMutex);
		auto settings = GetTorrentTaskSettings(taskId);
		if (patch.downloadLimit) settings.downloadLimit = *patch.downloadLimit;
		if (patch.uploadLimit) settings.uploadLimit = *patch.uploadLimit;
		if (patch.minimumUploadRate) settings.minimumUploadRate = *patch.minimumUploadRate;
		if (patch.maxConnections) settings.maxConnections = *patch.maxConnections;
		if (patch.maxUploads) settings.maxUploads = *patch.maxUploads;
		if (patch.enableDht) settings.enableDht = *patch.enableDht;
		if (patch.enableLsd) settings.enableLsd = *patch.enableLsd;
		if (patch.enablePex) settings.enablePex = *patch.enablePex;
		if (patch.applyIpFilter) settings.applyIpFilter = *patch.applyIpFilter;
		if (patch.sequentialDownload) settings.sequentialDownload = *patch.sequentialDownload;
		if (patch.superSeeding) settings.superSeeding = *patch.superSeeding;
		if (patch.forceStart) settings.forceStart = *patch.forceStart;
		if (patch.uploadMode) settings.uploadMode = *patch.uploadMode;
		if (patch.shareMode) settings.shareMode = *patch.shareMode;
		if (patch.shareRatioLimit) settings.shareRatioLimit = *patch.shareRatioLimit;
		if (patch.seedingTimeLimit) settings.seedingTimeLimit = *patch.seedingTimeLimit;
		if (patch.inactiveSeedingTimeLimit) settings.inactiveSeedingTimeLimit = *patch.inactiveSeedingTimeLimit;
		if (patch.shareLimitMatchAll) settings.shareLimitMatchAll = *patch.shareLimitMatchAll;
		if (patch.shareLimitAction) settings.shareLimitAction = *patch.shareLimitAction;
		if (patch.completionAction) settings.completionAction = *patch.completionAction;
		return ApplyTorrentTaskSettings(taskId, settings);
	}

	bool LibtorrentHandle::ApplyTorrentTaskSettings(std::string const& taskId, TorrentTaskSettings const& settings)
	{
		if (settings.downloadLimit < 0 || settings.uploadLimit < 0
			|| settings.minimumUploadRate < 0
			|| (settings.uploadLimit > 0
				&& settings.minimumUploadRate > settings.uploadLimit)
			|| settings.maxConnections < -1
			|| settings.maxConnections == 0
			|| settings.maxConnections == 1
			|| settings.maxUploads < -1
			|| !std::isfinite(settings.shareRatioLimit)
			|| (settings.shareRatioLimit < 0 && settings.shareRatioLimit != -1.0
				&& settings.shareRatioLimit != -2.0)
			|| settings.seedingTimeLimit < -2
			|| settings.inactiveSeedingTimeLimit < -2
			|| settings.shareLimitAction < -1
			|| settings.shareLimitAction > 3
			|| settings.completionAction < -1
			|| settings.completionAction > 1)
		{
			return false;
		}
		std::lock_guard lock(m_torrentMapMutex);
		auto const item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end() || !item->second.is_valid()) return false;
		auto const persistedSettings = PersistedTaskSettings(taskId, settings);
		if (m_stateManager && !m_stateManager->SaveTaskSettings(persistedSettings)) return false;
		{
			std::lock_guard cacheLock(m_taskSettingsCacheMutex);
			m_taskSettingsCache.insert_or_assign(taskId, persistedSettings);
		}

		auto const handle = item->second;
		handle.set_download_limit(std::max(0, settings.downloadLimit));
		handle.set_upload_limit(std::max(0, settings.uploadLimit));
		handle.set_max_connections(settings.maxConnections < 0 ? -1 : std::max(2, settings.maxConnections));
		handle.set_max_uploads(settings.maxUploads < 0 ? -1 : settings.maxUploads);
		auto const discoveryMask = lt::torrent_flags::disable_dht | lt::torrent_flags::disable_lsd | lt::torrent_flags::disable_pex;
		auto discoveryFlags = lt::torrent_flags_t{};
		if (!settings.enableDht) discoveryFlags |= lt::torrent_flags::disable_dht;
		if (!settings.enableLsd) discoveryFlags |= lt::torrent_flags::disable_lsd;
		if (!settings.enablePex) discoveryFlags |= lt::torrent_flags::disable_pex;
		handle.set_flags(discoveryFlags, discoveryMask);
		auto const behaviorMask = lt::torrent_flags::apply_ip_filter | lt::torrent_flags::sequential_download | lt::torrent_flags::super_seeding | lt::torrent_flags::upload_mode | lt::torrent_flags::share_mode;
		auto behaviorFlags = lt::torrent_flags_t{};
		if (settings.applyIpFilter) behaviorFlags |= lt::torrent_flags::apply_ip_filter;
		if (settings.sequentialDownload) behaviorFlags |= lt::torrent_flags::sequential_download;
		if (settings.superSeeding) behaviorFlags |= lt::torrent_flags::super_seeding;
		if (settings.uploadMode) behaviorFlags |= lt::torrent_flags::upload_mode;
		if (settings.shareMode) behaviorFlags |= lt::torrent_flags::share_mode;
		handle.set_flags(behaviorFlags, behaviorMask);
		if (settings.forceStart)
		{
			handle.unset_flags(lt::torrent_flags::auto_managed);
			handle.resume();
		}
		else if ((handle.flags() & lt::torrent_flags::paused) == lt::torrent_flags_t{})
		{
			handle.set_flags(lt::torrent_flags::auto_managed);
		}

		{
			std::lock_guard constraintsLock(m_rateConstraintsMutex);
			m_rateConstraints.insert_or_assign(handle, Impl::RateConstraints{ std::max(0, settings.downloadLimit), std::max(0, settings.uploadLimit), std::max(0, settings.minimumUploadRate) });
		}
		RequestResumeDataForTorrent(handle);
		return true;
	}

	bool LibtorrentHandle::SetTorrentShareLimits(
		std::string const& taskId,
		double const ratioLimit,
		int const seedingTimeLimit,
		int const inactiveSeedingTimeLimit,
		bool const matchAll,
		int const action)
	{
		TorrentTaskPatch patch;
		patch.shareRatioLimit = ratioLimit;
		patch.seedingTimeLimit = seedingTimeLimit;
		patch.inactiveSeedingTimeLimit = inactiveSeedingTimeLimit;
		patch.shareLimitMatchAll = matchAll;
		patch.shareLimitAction = action;
		return PatchTorrentTaskSettings(taskId, patch);
	}

	void LibtorrentHandle::AddTrackers(
		std::string const& taskId,
		std::vector<std::string> const& urls)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		for (const auto& url : urls)
		{
			if (!url.empty())
				item->second.add_tracker(lt::announce_entry(url));
		}
		RequestResumeDataForTorrent(item->second);
	}

	void LibtorrentHandle::EditTracker(
		std::string const& taskId,
		std::string const& originalUrl,
		std::string const& newUrl,
		const std::optional<int> tier)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		auto trackers = item->second.trackers();
		for (auto& tracker : trackers)
		{
			if (tracker.url != originalUrl)
				continue;
			const auto existingTier = tracker.tier;
			if (!newUrl.empty() && newUrl != originalUrl)
				tracker = lt::announce_entry(newUrl);
			tracker.tier = tier
				? static_cast<std::uint8_t>(*tier)
				: existingTier;
		}
		item->second.replace_trackers(trackers);
		RequestResumeDataForTorrent(item->second);
	}

	void LibtorrentHandle::RemoveTrackers(
		std::string const& taskId,
		std::vector<std::string> const& urls)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		const std::unordered_set<std::string> removed(
			urls.begin(), urls.end());
		auto trackers = item->second.trackers();
		std::erase_if(trackers, [&removed](const auto& tracker)
		{
			return removed.contains(tracker.url);
		});
		item->second.replace_trackers(trackers);
		RequestResumeDataForTorrent(item->second);
	}

	void LibtorrentHandle::AddWebSeeds(
		std::string const& taskId,
		std::vector<std::string> const& urls)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		for (const auto& url : urls)
		{
			if (!url.empty())
				item->second.add_url_seed(url);
		}
		RequestResumeDataForTorrent(item->second);
	}

	void LibtorrentHandle::EditWebSeed(
		std::string const& taskId,
		std::string const& originalUrl,
		std::string const& newUrl)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		item->second.remove_url_seed(originalUrl);
		if (!newUrl.empty())
			item->second.add_url_seed(newUrl);
		RequestResumeDataForTorrent(item->second);
	}

	void LibtorrentHandle::RemoveWebSeeds(
		std::string const& taskId,
		std::vector<std::string> const& urls)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		for (const auto& url : urls)
			item->second.remove_url_seed(url);
		RequestResumeDataForTorrent(item->second);
	}

	bool LibtorrentHandle::AddPeer(
		std::string const& taskId,
		std::string const& endpoint)
	{
		std::string addressText;
		std::string_view portText;
		if (endpoint.starts_with('['))
		{
			const auto closing = endpoint.find(']');
			if (closing == std::string::npos
				|| closing + 2 > endpoint.size()
				|| endpoint[closing + 1] != ':')
			{
				return false;
			}
			addressText = endpoint.substr(1, closing - 1);
			portText = std::string_view(endpoint).substr(closing + 2);
		}
		else
		{
			const auto separator = endpoint.rfind(':');
			if (separator == std::string::npos)
				return false;
			addressText = endpoint.substr(0, separator);
			portText = std::string_view(endpoint).substr(separator + 1);
		}

		unsigned int port{};
		const auto [position, parseError] = std::from_chars(
			portText.data(), portText.data() + portText.size(), port);
		if (parseError != std::errc{}
			|| position != portText.data() + portText.size()
			|| port == 0 || port > 65535)
		{
			return false;
		}
		boost::system::error_code error;
		const auto address =
			boost::asio::ip::make_address(addressText, error);
		if (error)
			return false;
		const lt::tcp::endpoint parsed(
			address, static_cast<std::uint16_t>(port));
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return false;
		}
		item->second.connect_peer(parsed);
		return true;
	}

	void LibtorrentHandle::AdjustQueuePosition(
		std::string const& taskId,
		const std::string_view operation)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		if (operation == "increasePrio")
			item->second.queue_position_up();
		else if (operation == "decreasePrio")
			item->second.queue_position_down();
		else if (operation == "topPrio")
			item->second.queue_position_top();
		else if (operation == "bottomPrio")
			item->second.queue_position_bottom();
		if (m_stateManager)
		{
			for (auto const& [currentTaskId, handle] : m_taskIdToHandle)
			{
				if (handle.is_valid())
					m_stateManager->UpdateTaskQueuePosition(
						currentTaskId,
						static_cast<int>(handle.queue_position()));
			}
		}
	}

	void LibtorrentHandle::RestoreQueuePositions()
	{
		if (!m_stateManager) return;
		auto tasks = m_stateManager->LoadAllTasks();
		std::erase_if(tasks, [](auto const& task)
		{
			return task.queuePosition < 0;
		});
		std::ranges::sort(tasks, {}, &TaskMetadata::queuePosition);
		std::lock_guard lock(m_torrentMapMutex);
		for (auto const& task : tasks)
		{
			auto const handle = m_taskIdToHandle.find(task.taskId);
			if (handle != m_taskIdToHandle.end() && handle->second.is_valid())
				handle->second.queue_position_set(
					lt::queue_position_t{ task.queuePosition });
		}
	}

	void LibtorrentHandle::SetAutoManaged(
		std::string const& taskId, const bool enabled)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		if (enabled)
			item->second.set_flags(lt::torrent_flags::auto_managed);
		else
			item->second.unset_flags(lt::torrent_flags::auto_managed);
	}

	void LibtorrentHandle::SetForceStart(
		std::string const& taskId, const bool enabled)
	{
		TorrentTaskPatch patch;
		patch.forceStart = enabled;
		PatchTorrentTaskSettings(taskId, patch);
	}

	void LibtorrentHandle::SetSuperSeeding(
		std::string const& taskId, const bool enabled)
	{
		TorrentTaskPatch patch;
		patch.superSeeding = enabled;
		PatchTorrentTaskSettings(taskId, patch);
	}

	void LibtorrentHandle::ToggleSequentialDownload(
		std::string const& taskId)
	{
		std::lock_guard commandLock(m_taskCommandMutex);
		auto settings = GetTorrentTaskSettings(taskId);
		settings.sequentialDownload = !settings.sequentialDownload;
		ApplyTorrentTaskSettings(taskId, settings);
	}

	void LibtorrentHandle::ToggleFirstLastPiecePriority(
		std::string const& taskId)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		auto priorities = item->second.get_piece_priorities();
		if (priorities.empty())
			return;
		const bool enabled =
			static_cast<std::uint8_t>(priorities.front()) > 4
			&& static_cast<std::uint8_t>(priorities.back()) > 4;
		const auto priority = static_cast<lt::download_priority_t>(
			static_cast<std::uint8_t>(enabled ? 4 : 7));
		priorities.front() = priority;
		priorities.back() = priority;
		item->second.prioritize_pieces(priorities);
		RequestResumeDataForTorrent(item->second);
	}

	void LibtorrentHandle::MoveStorage(
		std::string const& taskId,
		std::string const& path)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item != m_taskIdToHandle.end()
			&& item->second.is_valid())
		{
			item->second.move_storage(path);
		}
	}

	void LibtorrentHandle::RenameFile(
		std::string const& taskId,
		const int fileIndex,
		std::string const& newPath)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item != m_taskIdToHandle.end()
			&& item->second.is_valid()
			&& fileIndex >= 0)
		{
			item->second.rename_file(
				lt::file_index_t{ fileIndex }, newPath);
		}
	}

	bool LibtorrentHandle::SetSslCertificateBuffers(
		std::string const& taskId,
		std::string const& certificate,
		std::string const& privateKey,
		std::string const& dhParameters)
	{
		if (!ValidateTlsBuffers(certificate, privateKey, dhParameters))
			return false;
		std::lock_guard lock(m_torrentMapMutex);
		auto const item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end() || !item->second.is_valid())
			return false;
		try
		{
			lt::entry credentials;
			credentials["certificate"] = certificate;
			credentials["key"] = privateKey;
			credentials["dh"] = dhParameters;
			std::vector<char> plaintext;
			lt::bencode(std::back_inserter(plaintext), credentials);
			auto const encrypted = ProtectTls(plaintext);
			auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
			database.SetString(TorrentTlsCategory, taskId, encrypted);
			if (database.GetString(TorrentTlsCategory, taskId) != encrypted) return false;
			item->second.set_ssl_certificate_buffer(
				certificate, privateKey, dhParameters);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool LibtorrentHandle::RestoreSslCertificate(std::string const& taskId)
	{
		try
		{
			auto const encrypted = ::OpenNet::Core::AppSettingsDatabase::Instance()
				.GetString(TorrentTlsCategory, taskId);
			if (!encrypted || encrypted->size() > 8 * 1024 * 1024
				|| encrypted->size() % 2 != 0) return false;
			std::vector<BYTE> bytes;
			bytes.reserve(encrypted->size() / 2);
			for (std::size_t i = 0; i < encrypted->size(); i += 2)
			{
				unsigned int value{};
				auto const begin = encrypted->data() + i;
				auto const parsed = std::from_chars(begin, begin + 2, value, 16);
				if (parsed.ec != std::errc{} || parsed.ptr != begin + 2) return false;
				bytes.push_back(static_cast<BYTE>(value));
			}
			DATA_BLOB input{ static_cast<DWORD>(bytes.size()), bytes.data() }, output{};
			if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
				CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
			auto wipe = [size = output.cbData](BYTE* memory)
			{
				SecureZeroMemory(memory, size);
				LocalFree(memory);
			};
			std::unique_ptr<BYTE, decltype(wipe)> memory(output.pbData, wipe);
			lt::error_code error;
			auto const root = lt::bdecode(lt::span<char const>(
				reinterpret_cast<char const*>(output.pbData), output.cbData), error);
			if (error) return false;
			std::string certificate(root.dict_find_string_value("certificate"));
			std::string key(root.dict_find_string_value("key"));
			std::string dh(root.dict_find_string_value("dh"));
			bool result = false;
			if (ValidateTlsBuffers(certificate, key, dh))
			{
				std::lock_guard mapLock(m_torrentMapMutex);
				auto const task = m_taskIdToHandle.find(taskId);
				if (task != m_taskIdToHandle.end() && task->second.is_valid())
				{
					task->second.set_ssl_certificate_buffer(certificate, key, dh);
					result = true;
				}
			}
			SecureZeroMemory(key.data(), key.size());
			return result;
		}
		catch (...) { return false; }
	}

	// ---------------------------------------------------------------
	//  Set file priorities
	// ---------------------------------------------------------------
	void LibtorrentHandle::SetFilePriorities(
		std::string const& taskId,
		std::vector<int> const& priorities)
	{
		std::lock_guard lk(m_torrentMapMutex);
		auto it = m_taskIdToHandle.find(taskId);
		if (it == m_taskIdToHandle.end() || !it->second.is_valid())
			return;

		std::vector<lt::download_priority_t> ltPri;
		ltPri.reserve(priorities.size());
		for (int p : priorities)
		{
			ltPri.push_back(static_cast<lt::download_priority_t>(
				static_cast<std::uint8_t>(std::clamp(p, 0, 7))));
		}
		it->second.prioritize_files(ltPri);
		{
			std::lock_guard lock(m_filePrioritiesMutex);
			m_filePrioritiesCache.insert_or_assign(it->second, ltPri);
		}
		RequestResumeDataForTorrent(it->second);
	}

	// ---------------------------------------------------------------
	//  Torrent count
	// ---------------------------------------------------------------
	int LibtorrentHandle::GetTorrentCount() const
	{
		std::lock_guard lk(m_torrentMapMutex);
		return static_cast<int>(m_taskIdToHandle.size());
	}
}
