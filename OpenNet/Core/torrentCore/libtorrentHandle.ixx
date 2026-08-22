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
			std::int64_t totalSize{};
			std::int64_t downloadedSize{};
			std::int64_t sessionDownloaded{};
			std::int64_t sessionUploaded{};
			std::int64_t allTimeDownloaded{};
			std::int64_t allTimeUploaded{};
			std::int64_t completedTimestamp{};
			int connectedPeers{};
			int connectedSeeds{};
			int knownPeers{ -1 };
			int knownSeeds{ -1 };
			std::string name;
			bool isPaused{};
			bool isChecking{};
			bool isFinished{};
			bool isSeeding{};
			int downloadLimit{};
			int uploadLimit{};
			int minimumUploadRate{};

			bool operator==(ProgressEvent const&) const = default;
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
			bool startImmediately = true,
			bool seedMode = false);

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
		void SetStateManager(TorrentStateManager* stateManager) noexcept;

		void SetProgressCallback(ProgressCallback cb);
		void SetFinishedCallback(FinishedCallback cb);
		void SetErrorCallback(ErrorCallback cb);

		bool IsRunning() const noexcept;

		// Get the task ID for a torrent name
		std::string GetTaskIdByName(std::string const& name) const;

		// Rebuild and apply the live libtorrent settings from OpenNet's persisted
		// TorrentSettings. Third-party settings_pack types stay implementation-only.
		void ReloadSettings(
			std::optional<int> downloadRateOverride = std::nullopt,
			std::optional<int> uploadRateOverride = std::nullopt);
		void ReloadIpFilter();

		struct RuntimeSettingsSnapshot
		{
			int maxQueuedDiskBytes{};
			int receiveSocketBufferSize{};
			int sendSocketBufferSize{};
			int dhtUploadRateLimit{};
		};
		RuntimeSettingsSnapshot GetRuntimeSettings() const;

		// -----------------------------------------------------------
		//  Session-level statistics (aggregated across all torrents)
		// -----------------------------------------------------------
		struct ListenStatus
		{
			int port{};
			bool isListening{};
			std::string error;
		};
		ListenStatus GetListenStatus() const;

		struct SessionStats
		{
			std::int64_t totalDownloadRate{}; // bytes/sec
			std::int64_t totalUploadRate{};   // bytes/sec
			std::int64_t totalDownloaded{};   // bytes (session lifetime)
			std::int64_t totalUploaded{};     // bytes (session lifetime)
			std::int64_t diskCacheBytes{};    // approximate bytes held by libtorrent disk blocks
			std::int64_t dhtBytesReceived{};  // DHT/UDP bytes (session lifetime)
			std::int64_t dhtBytesSent{};      // DHT/UDP bytes (session lifetime)
			std::int64_t longTermSeedingUploadRate{}; // bytes/sec from finished/seeding torrents
			int numTorrents{};
			int numRunningTorrents{};
			int numPausedTorrents{};
			int numDownloadingTorrents{};
			int numMetadataTorrents{};
			int numSeedingTorrents{};
			int numCheckingTorrents{};
			int numErrorTorrents{};
			int numPeers{};
			int numSeeds{};
			int dhtNodes{};
			std::int64_t internalBannedIps{};
			int listenPort{};                 // primary listen port (0 if not listening)
			bool isListening{};
			std::string listenError;

		};
		SessionStats GetSessionStats() const;
		// Alert-backed counters for high-frequency UI graphs. This avoids
		// enumerating every torrent on the caller thread for each sample.
		SessionStats GetPerformanceStats() const;

		// Complete session_stats_alert snapshot for diagnostics. This is kept
		// separate from SessionStats so normal UI polling does not copy hundreds
		// of counters when the Runtime Status window is closed.
		std::unordered_map<std::string, std::int64_t>
			GetSessionMetrics() const;

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
			std::int64_t totalDownloaded{};
			std::int64_t totalUploaded{};
			double progress{};
			std::uint32_t flags{};    // libtorrent peer_info::flags (bitmask)
			int connectionType{};     // 0=standard_bittorrent, 1=web_seed, 2=http_seed
			int source{};             // libtorrent peer_info::source_flags bitmask
			bool isIncoming{};        // true if peer initiated the connection
			bool isConnecting{};      // connecting or waiting for handshake
			bool isI2p{};
		};

		// Lightweight snapshot used by the peer table.  Unlike
		// GetTorrentDetail(), this does not query trackers, files, piece
		// priorities or file progress.
		std::vector<TorrentPeerInfo> GetTorrentPeers(
			std::string const& taskId) const;

		struct TorrentTrackerInfo
		{
			std::string url;
			int tier{};
			int numPeers{};
			int retries{};
			int nextAnnounceSeconds{ -1 };
			int seeders{ -1 };
			int leechers{ -1 };
			int downloaded{ -1 };
			std::string status; // "working", "updating", "error", "not contacted"
			std::string message;
		};

		struct TrackerLogEntry
		{
			std::int64_t timestamp{};
			std::string content;
			bool isError{};
		};

		struct TorrentFileEntry
		{
			std::string path;
			std::int64_t size{};           // file size in bytes
			std::int64_t bytesCompleted{}; // bytes downloaded so far
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
			std::string creator;
			std::int64_t addedTimestamp{};
			std::int64_t completedTimestamp{};
			std::int64_t creationTimestamp{};
			std::int64_t activeTimeSeconds{};
			std::int64_t seedingTimeSeconds{};
			std::int64_t totalSize{};
			std::int64_t totalDone{};
			std::int64_t totalUploaded{};
			std::int64_t sessionDownloaded{};
			std::int64_t sessionUploaded{};
			std::int64_t allTimeDownloaded{};
			std::int64_t allTimeUploaded{};
			int pieceSize{};
			int piecesNum{};
			int downloadRate{};
			int uploadRate{};
			int progressPpm{}; // parts per million
			int numPeers{};
			int numSeeds{};
			int numConnections{};
			int numComplete{ -1 };
			int numIncomplete{ -1 };
			double shareRatio{}; // all-time uploaded / all-time downloaded
			int state{};         // lt::torrent_status::state_t
			bool isPaused{};
			bool isAutoManaged{};
			bool isSequential{};
			bool isSuperSeeding{};
			bool firstLastPiecePriority{};
			bool isPrivate{};
			bool isPieceAligned{};
			int queuePosition{};
			std::vector<TorrentPeerInfo> peers;
			std::vector<TorrentTrackerInfo> trackers;
			std::vector<TorrentFileEntry> files;
		};

		TorrentDetailInfo GetTorrentDetail(std::string const& taskId) const;
		TorrentDetailInfo GetTorrentSummary(std::string const& taskId) const;
		std::vector<TorrentTrackerInfo> GetTorrentTrackers(std::string const& taskId) const;
		TorrentDetailInfo GetTorrentFilesSnapshot(std::string const& taskId) const;

		struct PeerConnectionEvent
		{
			std::string ip;
			int port{};
			std::string reason;
			std::int64_t timestamp{};
			bool isBan{};
		};

		// Recent terminal peer states captured from libtorrent alerts. These
		// are kept separately from peer_info because disconnected peers no
		// longer appear in a torrent's current peer snapshot.
		std::vector<PeerConnectionEvent> GetRecentPeerEvents(
			std::string const& taskId,
			std::int64_t maxAgeSeconds = 0) const;

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
		TorrentPieceInfo GetTorrentPieceSummary(std::string const& taskId) const;
		std::vector<std::uint8_t> ExportTorrentFile(
			std::string const& taskId) const;

		// Force re-check (hash verify) a torrent
		bool ForceRecheck(std::string const& taskId);

		// Force an immediate tracker announce
		void ForceReannounce(std::string const& taskId);
		void ForceReannounceTracker(
			std::string const& taskId,
			std::string const& trackerUrl);
		std::vector<TrackerLogEntry> GetTrackerLog(
			std::string const& taskId,
			std::string const& trackerUrl) const;
		std::optional<TrackerLogEntry> GetLatestTrackerLog(
			std::string const& taskId,
			std::string const& trackerUrl) const;
		void ClearTrackerLog(
			std::string const& taskId,
			std::string const& trackerUrl);
		void ClearTrackerLogs(std::string const& taskId);
		void ClearAllTrackerLogs();

		// Per-torrent transfer limits (bytes/sec, 0 = unlimited)
		int GetTorrentDownloadLimit(std::string const& taskId) const;
		int GetTorrentUploadLimit(std::string const& taskId) const;
		void SetTorrentDownloadLimit(std::string const& taskId, int limit);
		void SetTorrentUploadLimit(std::string const& taskId, int limit);

		struct TorrentTaskSettings
		{
			int downloadLimit{};
			int uploadLimit{};
			int minimumUploadRate{};
			int maxConnections{ -1 };
			int maxUploads{ -1 };
			bool enableDht{ true };
			bool enableLsd{ true };
			bool enablePex{ true };
			bool applyIpFilter{ true };
			bool sequentialDownload{};
			bool superSeeding{};
			bool forceStart{};
			bool uploadMode{};
			bool shareMode{};
		};
		TorrentTaskSettings GetTorrentTaskSettings(std::string const& taskId) const;
		bool SetTorrentTaskSettings(std::string const& taskId, TorrentTaskSettings const& settings);

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
		// Implementation-only helpers use abbreviated templates so the exported
		// declaration does not name libtorrent types. They are instantiated only
		// in libtorrentHandle.cpp.
		void AlertLoop();
		void DispatchAlerts(auto const& alerts);
		void ConfigureDefaultSettings(auto& pack);
		void HandleSaveResumeDataAlert(auto const* alert);
		void HandleSaveResumeDataFailedAlert(auto const* alert);
		void RequestResumeDataForTorrent(auto const& handle);
		void EnforceClientFilters();
		void RecordPeerEvent(
			auto const& handle,
			auto const& endpoint,
			std::string reason,
			bool isBan);
		void ClearPeerEvent(auto const& handle, auto const& endpoint);
		void RecordTrackerLog(
			auto const& handle,
			std::string const& trackerUrl,
			std::string content,
			bool isError = false);
		void ResolveSessionStatsMetricIndices();
		TorrentDetailInfo GetTorrentDetailImpl(std::string const& taskId, bool includePeers, bool includeTrackers, bool includeFiles) const;

		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

}
