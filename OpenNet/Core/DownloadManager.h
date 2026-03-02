/*
 * PROJECT:   OpenNet
 * FILE:      Core/DownloadManager.h
 * PURPOSE:   Unified download manager orchestrating BT (libtorrent) and HTTP (Aria2)
 *
 * LICENSE:   The MIT License
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>

#include "Core/Aria2/Aria2Engine.h"
#include "Core/Aria2/Aria2Models.h"
#include "Core/HttpStateManager.h"

namespace OpenNet::Core
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

    using HttpProgressCallback = std::function<void(HttpTaskProgress const &)>;
    using HttpFinishedCallback = std::function<void(std::string const &gid, std::string const &name)>;
    using HttpErrorCallback = std::function<void(std::string const &gid, std::string const &message)>;

    // ------------------------------------------------------------------
    //  DownloadManager – singleton
    // ------------------------------------------------------------------
    class DownloadManager
    {
    public:
        static DownloadManager &Instance();

        // Lifecycle
        void Initialize();
        void Shutdown();
        bool IsAria2Available() const;

        // HTTP download operations via Aria2
        std::string AddHttpDownload(std::string const &url, std::string const &dir = {}, std::string const &fileName = {});
        // Get the record-id associated with a GID (set after AddHttpDownload)
        std::string GetRecordIdForGid(std::string const &gid) const;
        void PauseHttpDownload(std::string const &gid);
        void ResumeHttpDownload(std::string const &gid);
        void CancelHttpDownload(std::string const &gid);
        void RemoveHttpDownload(std::string const &gid);

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
        Aria2::LocalAria2Instance *Aria2() { return m_aria2.get(); }

    private:
        DownloadManager() = default;
        ~DownloadManager();

        DownloadManager(DownloadManager const &) = delete;
        DownloadManager &operator=(DownloadManager const &) = delete;

        void RefreshThreadEntry();
        void ProcessAria2Tasks();

    private:
        std::unique_ptr<Aria2::LocalAria2Instance> m_aria2;
        bool m_initialized = false;

        std::thread m_refreshThread;
        std::atomic<bool> m_stopRefresh{false};
        mutable std::mutex m_mutex;

        // Cached task GIDs for change detection
        std::set<std::string> m_knownGids;

        // GID -> HttpStateManager record-id mapping
        std::unordered_map<std::string, std::string> m_gidToRecordId;

        // Callbacks
        HttpProgressCallback m_progressCb;
        HttpFinishedCallback m_finishedCb;
        HttpErrorCallback m_errorCb;

        // Cached global speeds
        std::atomic<uint64_t> m_totalDlSpeed{0};
        std::atomic<uint64_t> m_totalUlSpeed{0};
    };
}
