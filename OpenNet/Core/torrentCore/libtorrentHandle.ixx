// 直接包含 libtorrent 头，避免与 inline namespace 冲突
// forward declarations  前置声明
// Forward declaring types from the libtorrent namespace is discouraged as it may break in future releases.Instead include libtorrent / fwd.hpp for forward declarations of all public types in libtorrent.
// 不建议在 libtorrent 命名空间中提前声明类型，因为这可能在未来的版本中出现问题。相反，应包含 libtorrent / fwd.hpp 以声明 libtorrent 中所有公共类型的提前声明。
module;

#include <libtorrent/fwd.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/socket.hpp>
#include <libtorrent/torrent_handle.hpp>

export module OpenNet.Core.torrentCore.LibtorrentHandle;

import std;
import OpenNet.Core.torrentCore.TorrentStateManager;

export namespace OpenNet::Core::Torrent
{
	class LibtorrentHandle
	{
	public:
		struct ProgressEvent
		{
			std::string taskId;
			int progressPercent{};
			int downloadRateKB{};
			int uploadRateKB{};
			std::string name;
		};

		typedef std::function<void(ProgressEvent const&)> ProgressCallback;
		typedef std::function<void(std::string const&, std::string const&)> FinishedCallback;
		typedef std::function<void(std::string const&)> ErrorCallback;

		LibtorrentHandle();
		~LibtorrentHandle();

		LibtorrentHandle(LibtorrentHandle const&) = delete;
		LibtorrentHandle& operator=(LibtorrentHandle const&) = delete;
		LibtorrentHandle(LibtorrentHandle&&) = delete;
		LibtorrentHandle& operator=(LibtorrentHandle&&) = delete;

		bool Initialize();
		void Start();
		void Stop();
		bool AddMagnet(
			std::string const& magnetUri,
			std::string const& savePath,
			std::vector<int> const& filePriorities = {},
			std::vector<std::string> const& extraTrackers = {},
			bool startImmediately = true);
		bool AddTorrentFile(
			std::string const& torrentFilePath,
			std::string const& savePath,
			std::vector<int> const& filePriorities = {},
			std::vector<std::string> const& extraTrackers = {},
			bool startImmediately = true);

		// Resume torrent from saved state (returns task ID if successful)
		std::string AddTorrentFromResumeData(std::string const& taskId);

		// Pause a specific torrent by task ID
		void PauseTorrent(std::string const& taskId);

		// Resume a specific torrent by task ID
		void ResumeTorrent(std::string const& taskId);

		// Remove a torrent by task ID
		void RemoveTorrent(std::string const& taskId, bool deleteFiles = false);

		// Save resume data for all active torrents
		void SaveAllResumeData();

		// Set the state manager for persistence
		void SetStateManager(TorrentStateManager* stateManager) noexcept
		{
			m_stateManager = stateManager;
		}

		void SetProgressCallback(ProgressCallback cb);
		void SetFinishedCallback(FinishedCallback cb);
		void SetErrorCallback(ErrorCallback cb);

		bool IsRunning() const noexcept
		{
			return m_running.load();
		}

		libtorrent::session* NativeSession() const noexcept
		{
			return m_session.get();
		}

		// Get the task ID for a torrent name
		std::string GetTaskIdByName(std::string const& name) const;

		// Session settings access
		libtorrent::settings_pack GetSettings() const;
		void ApplySettings(libtorrent::settings_pack const& pack);

		// IP filter
		void SetIpFilter(libtorrent::ip_filter const& filter);

		// -----------------------------------------------------------
		//  Session-level statistics (aggregated across all torrents)
		// -----------------------------------------------------------
		struct SessionStats
		{
			std::int64_t totalDownloadRate{}; // bytes/sec
			std::int64_t totalUploadRate{};   // bytes/sec
			std::int64_t totalDownloaded{};   // bytes (session lifetime)
			std::int64_t totalUploaded{};     // bytes (session lifetime)
			int numTorrents{};
			int numPeers{};
			int dhtNodes{};
			int listenPort{};                 // primary listen port (0 if not listening)
			bool isListening{};
			std::string listenError;
		};
		SessionStats GetSessionStats() const;

		struct PortMappingStatus
		{
			bool upnpEnabled{};
			bool natPmpEnabled{};
			int tcpExternalPort{};
			int udpExternalPort{};
			std::string tcpMechanism;
			std::string udpMechanism;
			std::string externalAddress;
			std::string lastError;
		};
		PortMappingStatus GetPortMappingStatus() const;
		void RefreshPortMappings();

		// -----------------------------------------------------------
		//  Per-torrent detail information
		// -----------------------------------------------------------
		struct TorrentPeerInfo
		{
			std::string ip;
			int port{};
			std::string client;
			int downloadRateKB{};
			int uploadRateKB{};
			int64_t totalDownloaded{};
			int64_t totalUploaded{};
			double progress{};
			uint32_t flags{};         // libtorrent peer_info::flags (bitmask)
			int connectionType{};     // 0=standard_bittorrent, 1=web_seed, 2=http_seed
			int source{};             // libtorrent peer_info::source_flags bitmask
			bool isIncoming{};        // true if peer initiated the connection
			bool isConnecting{};      // connecting or waiting for handshake
		};

		struct TorrentTrackerInfo
		{
			std::string url;
			int tier{};
			int numPeers{};
			std::string status; // "working", "updating", "error", "not contacted"
			std::string message;
		};

		struct TorrentFileEntry
		{
			std::string path;
			int64_t size{};           // file size in bytes
			int64_t bytesCompleted{}; // bytes downloaded so far
			int priority{ 4 };          // 0=skip, 1=low, 4=normal, 7=high
			int fileIndex{};
			int firstPiece{};
			int lastPiece{};
		};

		struct TorrentDetailInfo
		{
			std::string taskId;
			std::string name;
			std::string infoHash;
			std::string infoHashV1;
			std::string infoHashV2;
			std::string apiHash;
			std::string savePath;
			std::string comment;
			int64_t totalSize{};
			int64_t totalDone{};
			int64_t totalUploaded{};
			int pieceSize{};
			int piecesNum{};
			int downloadRate{};
			int uploadRate{};
			int progressPpm{}; // parts per million
			int numPeers{};
			int numSeeds{};
			int numConnections{};
			double shareRatio{}; // uploaded / downloaded
			int state{};         // lt::torrent_status::state_t
			bool isPaused{};
			bool isAutoManaged{};
			bool isSequential{};
			bool isSuperSeeding{};
			bool firstLastPiecePriority{};
			int queuePosition{};
			std::vector<TorrentPeerInfo> peers;
			std::vector<TorrentTrackerInfo> trackers;
			std::vector<TorrentFileEntry> files;
		};

		TorrentDetailInfo GetTorrentDetail(std::string const& taskId) const;

		struct TorrentPieceInfo
		{
			int pieceSize{};
			std::vector<int> states;
			std::vector<int> availability;
			std::vector<std::string> hashes;
			std::vector<std::string> webSeeds;
		};

		TorrentPieceInfo GetTorrentPieceInfo(
			std::string const& taskId) const;
		std::vector<std::uint8_t> ExportTorrentFile(
			std::string const& taskId) const;

		// Force re-check (hash verify) a torrent
		void ForceRecheck(std::string const& taskId);

		// Force an immediate tracker announce
		void ForceReannounce(std::string const& taskId);

		// Per-torrent transfer limits (bytes/sec, 0 = unlimited)
		int GetTorrentDownloadLimit(std::string const& taskId) const;
		int GetTorrentUploadLimit(std::string const& taskId) const;
		void SetTorrentDownloadLimit(std::string const& taskId, int limit);
		void SetTorrentUploadLimit(std::string const& taskId, int limit);

		void AddTrackers(
			std::string const& taskId,
			std::vector<std::string> const& urls);
		void EditTracker(
			std::string const& taskId,
			std::string const& originalUrl,
			std::string const& newUrl,
			std::optional<int> tier = std::nullopt);
		void RemoveTrackers(
			std::string const& taskId,
			std::vector<std::string> const& urls);
		void AddWebSeeds(
			std::string const& taskId,
			std::vector<std::string> const& urls);
		void EditWebSeed(
			std::string const& taskId,
			std::string const& originalUrl,
			std::string const& newUrl);
		void RemoveWebSeeds(
			std::string const& taskId,
			std::vector<std::string> const& urls);
		bool AddPeer(
			std::string const& taskId,
			std::string const& endpoint);
		void AdjustQueuePosition(
			std::string const& taskId,
			std::string_view operation);
		void SetAutoManaged(
			std::string const& taskId, bool enabled);
		void SetForceStart(
			std::string const& taskId, bool enabled);
		void SetSuperSeeding(
			std::string const& taskId, bool enabled);
		void ToggleSequentialDownload(
			std::string const& taskId);
		void ToggleFirstLastPiecePriority(
			std::string const& taskId);
		void MoveStorage(
			std::string const& taskId,
			std::string const& path);
		void RenameFile(
			std::string const& taskId,
			int fileIndex,
			std::string const& newPath);

		// Set file priorities for a torrent
		//   priorities: one entry per file index, 0=skip 1=low 4=normal 7=high
		void SetFilePriorities(std::string const& taskId,
							   std::vector<int> const& priorities);

		// Get the number of active torrents
		int GetTorrentCount() const;

	private:
		void AlertLoop();
		void DispatchAlerts(std::vector<libtorrent::alert*> const& alerts);
		void ConfigureDefaultSettings(libtorrent::settings_pack& pack);
		void HandleSaveResumeDataAlert(libtorrent::save_resume_data_alert const* alert);
		void HandleSaveResumeDataFailedAlert(libtorrent::save_resume_data_failed_alert const* alert);
		void RequestResumeDataForTorrent(libtorrent::torrent_handle const& handle);
		void EnforceClientFilters();

		std::unique_ptr<libtorrent::session> m_session;
		std::optional<libtorrent::session_proxy> m_sessionProxy;
		std::atomic<bool> m_running{ false };
		std::thread m_thread;

		std::mutex m_cbMutex;
		ProgressCallback m_progressCb;
		FinishedCallback m_finishedCb;
		ErrorCallback m_errorCb;

		std::atomic<bool> m_stopRequested{ false };
		std::atomic<int> m_pendingResumeDataCount{ 0 };  // outstanding save_resume_data requests
		std::unordered_map<std::string, lt::torrent_handle> m_taskIdToHandle;
		std::unordered_map<lt::torrent_handle, std::string, std::hash<lt::torrent_handle>> m_handleToTaskId;
		mutable std::mutex m_torrentMapMutex;
		TorrentStateManager* m_stateManager{ nullptr };

		// Cached DHT node count (updated via dht_stats_alert)
		std::atomic<int> m_cachedDhtNodeCount{ 0 };

		mutable std::mutex m_portMappingMutex;
		PortMappingStatus m_portMappingStatus;
		mutable std::mutex m_listenStateMutex;
		std::string m_lastListenError;

		// Cached session-level counters (updated via session_stats_alert)
		mutable std::mutex m_sessionStatsMutex;
		std::int64_t m_sessionTotalDownload{};
		std::int64_t m_sessionTotalUpload{};
		int m_sessionStatsMetricIdxRecvBytes{ -1 };
		int m_sessionStatsMetricIdxSentBytes{ -1 };
		int m_sessionStatsMetricIdxDhtNodes{ -1 };
		bool m_sessionStatsMetricsResolved{ false };

		// Time-gated stats requests to avoid self-excitation in AlertLoop
		std::chrono::steady_clock::time_point m_lastTorrentUpdateRequest{
			std::chrono::steady_clock::now() };
		std::chrono::steady_clock::time_point m_lastStatsRequest{ std::chrono::steady_clock::now() };
		std::chrono::steady_clock::time_point m_lastClientFilterCheck{
			std::chrono::steady_clock::now() };

		void ResolveSessionStatsMetricIndices();
	};

}
