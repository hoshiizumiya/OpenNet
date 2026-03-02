#pragma once
#include <functional>
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>

// 直接包含 libtorrent 头，避免与 inline namespace 冲突
// forward declarations  前置声明
// Forward declaring types from the libtorrent namespace is discouraged as it may break in future releases.Instead include libtorrent / fwd.hpp for forward declarations of all public types in libtorrent.
// 不建议在 libtorrent 命名空间中提前声明类型，因为这可能在未来的版本中出现问题。相反，应包含 libtorrent / fwd.hpp 以声明 libtorrent 中所有公共类型的提前声明。

#include <libtorrent/fwd.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/socket.hpp>
#include <libtorrent/torrent_handle.hpp>

// Forward declaration
namespace OpenNet::Core::Torrent
{
    class TorrentStateManager;
}

namespace OpenNet::Core::Torrent
{
    class LibtorrentHandle
    {
    public:
        struct ProgressEvent
        {
            int progressPercent{};
            int downloadRateKB{};
            int uploadRateKB{};
            std::string name;
        };

        typedef std::function<void(ProgressEvent const &)> ProgressCallback;
        typedef std::function<void(std::string const &)> FinishedCallback;
        typedef std::function<void(std::string const &)> ErrorCallback;

        LibtorrentHandle();
        ~LibtorrentHandle();

        LibtorrentHandle(LibtorrentHandle const &) = delete;
        LibtorrentHandle &operator=(LibtorrentHandle const &) = delete;
        LibtorrentHandle(LibtorrentHandle &&) = delete;
        LibtorrentHandle &operator=(LibtorrentHandle &&) = delete;

        bool Initialize();
        void Start();
        void Stop();
        bool AddMagnet(std::string const &magnetUri, std::string const &savePath);
        bool AddTorrentFile(std::string const &torrentFilePath, std::string const &savePath);

        // Resume torrent from saved state (returns task ID if successful)
        std::string AddTorrentFromResumeData(std::string const &taskId);

        // Pause a specific torrent by task ID
        void PauseTorrent(std::string const &taskId);

        // Resume a specific torrent by task ID
        void ResumeTorrent(std::string const &taskId);

        // Remove a torrent by task ID
        void RemoveTorrent(std::string const &taskId, bool deleteFiles = false);

        // Save resume data for all active torrents
        void SaveAllResumeData();

        // Set the state manager for persistence
        void SetStateManager(TorrentStateManager *stateManager);

        void SetProgressCallback(ProgressCallback cb);
        void SetFinishedCallback(FinishedCallback cb);
        void SetErrorCallback(ErrorCallback cb);

        bool IsRunning() const noexcept { return m_running.load(); }

        // Get the task ID for a torrent name
        std::string GetTaskIdByName(std::string const &name) const;

        // Session settings access
        libtorrent::settings_pack GetSettings() const;
        void ApplySettings(libtorrent::settings_pack const &pack);

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
        };
        SessionStats GetSessionStats() const;

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
            int priority{4};          // 0=skip, 1=low, 4=normal, 7=high
            int fileIndex{};
        };

        struct TorrentDetailInfo
        {
            std::string taskId;
            std::string name;
            std::string infoHash;
            std::string savePath;
            std::string comment;
            int64_t totalSize{};
            int64_t totalDone{};
            int64_t totalUploaded{};
            int downloadRate{};
            int uploadRate{};
            int progressPpm{}; // parts per million
            int numPeers{};
            int numSeeds{};
            int numConnections{};
            double shareRatio{}; // uploaded / downloaded
            int state{};         // lt::torrent_status::state_t
            bool isPaused{};
            std::vector<TorrentPeerInfo> peers;
            std::vector<TorrentTrackerInfo> trackers;
            std::vector<TorrentFileEntry> files;
        };

        TorrentDetailInfo GetTorrentDetail(std::string const &taskId) const;

        // Force re-check (hash verify) a torrent
        void ForceRecheck(std::string const &taskId);

        // Set file priorities for a torrent
        //   priorities: one entry per file index, 0=skip 1=low 4=normal 7=high
        void SetFilePriorities(std::string const &taskId,
                               std::vector<int> const &priorities);

        // Get the number of active torrents
        int GetTorrentCount() const;

    private:
        void AlertLoop();
        void DispatchAlerts(std::vector<libtorrent::alert *> const &alerts);
        void ConfigureDefaultSettings(libtorrent::settings_pack &pack);
        void HandleSaveResumeDataAlert(libtorrent::save_resume_data_alert const *alert);
        void HandleSaveResumeDataFailedAlert(libtorrent::save_resume_data_failed_alert const *alert);
        void RequestResumeDataForTorrent(libtorrent::torrent_handle const &handle);

        std::unique_ptr<libtorrent::session> m_session;
        std::atomic<bool> m_running{false};
        std::thread m_thread;

        std::mutex m_cbMutex;
        ProgressCallback m_progressCb;
        FinishedCallback m_finishedCb;
        ErrorCallback m_errorCb;

        std::atomic<bool> m_stopRequested{false};
        std::atomic<int> m_pendingResumeDataCount{0};  // outstanding save_resume_data requests
        std::unordered_map<std::string, lt::torrent_handle> m_taskIdToHandle;
        std::unordered_map<lt::torrent_handle, std::string, std::hash<lt::torrent_handle>> m_handleToTaskId;
        mutable std::mutex m_torrentMapMutex;
        TorrentStateManager *m_stateManager{nullptr};

        // Cached DHT node count (updated via dht_stats_alert)
        std::atomic<int> m_cachedDhtNodeCount{0};

        // Cached session-level counters (updated via session_stats_alert)
        mutable std::mutex m_sessionStatsMutex;
        std::int64_t m_sessionTotalDownload{};
        std::int64_t m_sessionTotalUpload{};
        int m_sessionStatsMetricIdxRecvBytes{-1};
        int m_sessionStatsMetricIdxSentBytes{-1};
        int m_sessionStatsMetricIdxDhtNodes{-1};
        bool m_sessionStatsMetricsResolved{false};

        // Time-gated stats requests to avoid self-excitation in AlertLoop
        std::chrono::steady_clock::time_point m_lastStatsRequest{ std::chrono::steady_clock::now() };

        void ResolveSessionStatsMetricIndices();
    };

}