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
//forward declarations  前置声明
//Forward declaring types from the libtorrent namespace is discouraged as it may break in future releases.Instead include libtorrent / fwd.hpp for forward declarations of all public types in libtorrent.
//不建议在 libtorrent 命名空间中提前声明类型，因为这可能在未来的版本中出现问题。相反，应包含 libtorrent / fwd.hpp 以声明 libtorrent 中所有公共类型的提前声明。

#include <libtorrent/fwd.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/socket.hpp>
#include <libtorrent/torrent_handle.hpp>

// Forward declaration
namespace OpenNet::Core::Torrent { class TorrentStateManager; }

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

        typedef std::function<void(ProgressEvent const&)> ProgressCallback;
        typedef std::function<void(std::string const&)> FinishedCallback;
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
        bool AddMagnet(std::string const& magnetUri, std::string const& savePath);
        bool AddTorrentFile(std::string const& torrentFilePath, std::string const& savePath);

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
        void SetStateManager(TorrentStateManager* stateManager);

        void SetProgressCallback(ProgressCallback cb);
        void SetFinishedCallback(FinishedCallback cb);
        void SetErrorCallback(ErrorCallback cb);

        bool IsRunning() const noexcept { return m_running.load(); }

        // Get the task ID for a torrent name
        std::string GetTaskIdByName(std::string const& name) const;

    private:
        void AlertLoop();
        void DispatchAlerts(std::vector<libtorrent::alert*> const& alerts);
        void ConfigureDefaultSettings(libtorrent::settings_pack& pack);
        void HandleSaveResumeDataAlert(libtorrent::save_resume_data_alert const* alert);
        void HandleSaveResumeDataFailedAlert(libtorrent::save_resume_data_failed_alert const* alert);
        void RequestResumeDataForTorrent(libtorrent::torrent_handle const& handle);

        std::unique_ptr<libtorrent::session> m_session;
        std::atomic<bool> m_running{ false };
        std::thread m_thread;

        std::mutex m_cbMutex;
        ProgressCallback m_progressCb;
        FinishedCallback m_finishedCb;
        ErrorCallback m_errorCb;

        std::atomic<bool> m_stopRequested{ false };
        std::unordered_map<std::string, lt::torrent_handle> m_taskIdToHandle;
        std::unordered_map<lt::torrent_handle, std::string, std::hash<lt::torrent_handle>> m_handleToTaskId;
        mutable std::mutex m_torrentMapMutex;
        TorrentStateManager* m_stateManager{ nullptr };
    };

}