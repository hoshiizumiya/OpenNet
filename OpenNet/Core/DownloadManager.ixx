/*
 * PROJECT:   OpenNet
 * FILE:      Core/DownloadManager.h
 * PURPOSE:   Unified download manager orchestrating BT (libtorrent) and HTTP (Aria2)
 *
 * LICENSE:   The MIT License
 */

export module OpenNet.Core.DownloadManager;

export import OpenNet.Core.Aria2.Aria2Engine;
export import OpenNet.Core.Aria2.Aria2Models;
export import OpenNet.Core.Content.ContentDirectoryContracts;
import OpenNet.Core.Content.ResourceKey;
import OpenNet.Core.HttpStateManager;
import winrt.Windows.Foundation;

export namespace OpenNet::Core
{
	// ------------------------------------------------------------------
	//  Download task type
	// ------------------------------------------------------------------
	enum class DownloadTaskType
	{
		Http,       // HTTP/HTTPS/FTP via Aria2
		BitTorrent, // Managed by libtorrent (existing P2PManager)
	};

	// ------------------------------------------------------------------
	//  Callback / event types exposed to ViewModels
	// ------------------------------------------------------------------
	struct HttpTaskProgress
	{
		std::string gid;
		std::string name;
		Aria2::DownloadStatus status;
		std::uint64_t totalLength;
		std::uint64_t completedLength;
		std::uint64_t downloadSpeed;
		std::uint64_t uploadSpeed;
		int progressPercent; // 0-100
	};
	struct HttpTaskLogEntry
	{
		std::int64_t timestamp{};
		std::string content;
	};

	struct HttpResourceDiscovery
	{
		bool completed{};
		bool checksumValidated{};
		bool peerFallbackQueued{};
		bool peerFallbackReady{};
		std::string contentId;
		std::uint64_t size{};
		std::uint32_t observationCount{};
		std::optional<::OpenNet::Core::Content::ContentIdentity> bep52Identity;
	};

	using HttpProgressCallback = std::function<void(HttpTaskProgress const&)>;
	using HttpFinishedCallback = std::function<void(std::string const& gid, std::string const& name)>;
	using HttpErrorCallback = std::function<void(std::string const& gid, std::string const& message)>;

	// ------------------------------------------------------------------
	//  DownloadManager – singleton
	// ------------------------------------------------------------------
	class DownloadManager
	{
	public:
		static DownloadManager& Instance();

		// Lifecycle
		winrt::Windows::Foundation::IAsyncAction InitializeAsync();
		void Shutdown();
		bool IsAria2Available() const;

		// HTTP download operations via Aria2
		std::string AddHttpDownload(std::string const& url, std::string const& dir = {}, std::string const& fileName = {});
		std::string AddHttpDownload(Aria2::HttpDownloadOptions const& options);
		std::optional<Aria2::DownloadInformation> GetHttpTaskInformation(std::string const& gid);
		std::vector<Aria2::ServersInformation> GetHttpTaskServers(std::string const& gid);
		std::vector<HttpTaskLogEntry> GetHttpTaskLog(std::string const& gid) const;
		std::optional<HttpResourceDiscovery> GetHttpResourceDiscovery(
			std::string const& gid) const;
		// Get the record-id associated with a GID (set after AddHttpDownload)
		std::string GetRecordIdForGid(std::string const& gid) const;
		void PauseHttpDownload(std::string const& gid);
		void ResumeHttpDownload(std::string const& gid);
		void CancelHttpDownload(std::string const& gid);
		void RemoveHttpDownload(std::string const& gid);
		void DeleteHttpDownload(
			std::string const& gid,
			bool deleteDownloadedFiles);

		// Bulk operations
		void PauseAllHttp();
		void ResumeAllHttp();
		void ClearCompletedHttp();

		// Info
		std::uint64_t TotalHttpDownloadSpeed() const;
		std::uint64_t TotalHttpUploadSpeed() const;

		// Callbacks
		void SetHttpProgressCallback(HttpProgressCallback cb);
		void SetHttpFinishedCallback(HttpFinishedCallback cb);
		void SetHttpErrorCallback(HttpErrorCallback cb);

		// Direct access for advanced usage (LocalAria2Instance IS-A Aria2Instance)
		Aria2::LocalAria2Instance* Aria2()
		{
			return m_aria2.get();
		}

	private:
		struct ResourceDiscoveryJob;

		DownloadManager() = default;
		~DownloadManager();

		DownloadManager(DownloadManager const&) = delete;
		DownloadManager& operator=(DownloadManager const&) = delete;

		void RefreshThreadEntry();
		void ProcessAria2Tasks();
		void ResourceDiscoveryThreadEntry();
		void QueueResourceDiscovery(
			std::string gid,
			std::vector<::OpenNet::Core::Content::ResourceKey> resourceKeys,
			std::optional<::OpenNet::Core::Content::ContentIdentity> expectedSha256,
			std::filesystem::path targetFilePath = {},
			std::uint64_t expectedSize = 0,
			bool hybridPrimary = false,
			bool userRequestedPaused = false,
			std::vector<std::string> webSeeds = {});
		void QueuePeerFallback(
			ResourceDiscoveryJob const& discovery,
			HttpResourceDiscovery const& summary);
		void PeerFallbackThreadEntry();
		bool HasPeerFallbackPending(std::string const& gid) const;
		bool TryPromotePeerFallback(
			std::string const& gid,
			Aria2::DownloadInformation const& task,
			std::string const& recordId);
		void CancelPeerFallback(std::string const& gid);
		void ShowHttpCompletionToast(std::string const& gid, Aria2::DownloadInformation const& task);

	private:
		std::unique_ptr<Aria2::LocalAria2Instance> m_aria2;
		bool m_initialized = false;
		bool m_initializing = false;

		std::thread m_refreshThread;
		std::atomic<bool> m_stopRefresh{ false };
		std::condition_variable m_stopCv;
		std::mutex m_stopMutex;

		struct ResourceDiscoveryJob
		{
			std::string gid;
			std::vector<::OpenNet::Core::Content::ResourceKey> resourceKeys;
			std::optional<::OpenNet::Core::Content::ContentIdentity> expectedSha256;
			std::filesystem::path targetFilePath;
			std::uint64_t expectedSize{};
			bool hybridPrimary{};
			bool userRequestedPaused{};
			std::vector<std::string> webSeeds;
		};
		std::thread m_resourceDiscoveryThread;
		std::atomic<bool> m_stopResourceDiscovery{ false };
		std::condition_variable m_resourceDiscoveryCv;
		std::mutex m_resourceDiscoveryMutex;
		std::deque<ResourceDiscoveryJob> m_resourceDiscoveryJobs;

		enum class PeerFallbackPhase : std::uint8_t
		{
			Pending,
			Downloading,
			Ready,
			Failed,
		};

		struct PeerFallbackJob
		{
			std::string gid;
			std::string sessionId;
			std::filesystem::path targetFilePath;
			std::filesystem::path temporaryFilePath;
			::OpenNet::Core::Content::ContentIdentity bep52Identity;
			::OpenNet::Core::Content::ContentIdentity expectedSha256;
			std::vector<::OpenNet::Core::Content::ResourceKey> resourceKeys;
			std::uint64_t expectedSize{};
			bool hybridPrimary{};
			std::vector<std::string> webSeeds;
		};

		struct PeerFallbackState
		{
			PeerFallbackPhase phase{ PeerFallbackPhase::Pending };
			PeerFallbackJob job;
			bool cancelRequested{};
			bool userPaused{};
			bool workerQueued{};
			int progressPercent{};
			std::int64_t downloadRate{};
			std::int64_t completedBytes{};
			std::string error;
		};

		std::atomic<bool> m_stopPeerFallback{ false };
		std::condition_variable m_peerFallbackCv;
		mutable std::mutex m_peerFallbackMutex;
		std::deque<PeerFallbackJob> m_peerFallbackJobs;
		std::unordered_map<std::string, PeerFallbackState> m_peerFallbacks;
		std::unordered_set<std::string> m_peerFallbackSuppressedGids;
		std::unordered_set<std::string> m_hybridProbeGids;
		std::unordered_set<std::string> m_userPausedHttpGids;
		std::vector<std::thread> m_peerFallbackWorkers;

		mutable std::mutex m_mutex;

		// Cached task GIDs for change detection
		std::set<std::string> m_knownGids;
		std::unordered_map<std::string, Aria2::DownloadStatus> m_lastHttpStatuses;
		std::unordered_map<std::string, Aria2::DownloadInformation> m_httpTaskSnapshots;
		std::unordered_map<std::string, std::vector<Aria2::ServersInformation>> m_httpServerSnapshots;
		std::unordered_map<std::string, std::vector<HttpTaskLogEntry>> m_httpTaskLogs;
		std::unordered_map<std::string, HttpResourceDiscovery> m_httpResourceDiscoveries;

		// GID -> HttpStateManager record-id mapping (mutable: acts as a cache)
		mutable std::unordered_map<std::string, std::string> m_gidToRecordId;

		// Callbacks
		HttpProgressCallback m_progressCb;
		HttpFinishedCallback m_finishedCb;
		HttpErrorCallback m_errorCb;

		// Cached global speeds
		std::atomic<uint64_t> m_totalDlSpeed{ 0 };
		std::atomic<uint64_t> m_totalUlSpeed{ 0 };
		std::chrono::steady_clock::time_point m_lastStoppedListRefresh{};
	};
}
