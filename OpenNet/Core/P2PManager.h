#pragma once

#include <winrt/Windows.Foundation.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <string>

// Include torrent core so nested ProgressEvent is known
#include "Core/torrentCore/libtorrentHandle.h"

namespace OpenNet::Core
{
    class P2PManager
    {
    public:
        static P2PManager& Instance();

        P2PManager(P2PManager const&) = delete;
        P2PManager& operator=(P2PManager const&) = delete;

        // Core lifecycle
        winrt::Windows::Foundation::IAsyncAction EnsureTorrentCoreInitializedAsync();
        bool IsTorrentCoreInitialized() const noexcept { return m_isTorrentCoreInitialized.load(); }
        OpenNet::Core::Torrent::LibtorrentHandle* TorrentCore() noexcept { return m_torrentCore.get(); }

        // Torrent operations
        winrt::Windows::Foundation::IAsyncOperation<bool> AddMagnetAsync(std::string magnetUri, std::string savePath);

        // Callback registration
        using ProgressCb = std::function<void(const ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent&)>
;        using FinishedCb = std::function<void(const std::string&)>;
        using ErrorCb = std::function<void(const std::string&)>;

        void SetProgressCallback(ProgressCb cb);
        void SetFinishedCallback(FinishedCb cb);
        void SetErrorCallback(ErrorCb cb);

    private:
        P2PManager() = default;
        ~P2PManager() = default;

        void WireCoreCallbacks();

        std::unique_ptr<OpenNet::Core::Torrent::LibtorrentHandle> m_torrentCore;
        std::mutex m_torrentMutex;
        std::atomic<bool> m_isTorrentCoreInitialized{ false };
        std::atomic<bool> m_initializing{ false };

        std::mutex m_cbMutex;
        ProgressCb m_progressCb;
        FinishedCb m_finishedCb;
        ErrorCb m_errorCb;
    };
}