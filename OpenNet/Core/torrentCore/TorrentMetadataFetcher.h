#pragma once

#include "TorrentMetadataInfo.h"
#include <libtorrent/fwd.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <winrt/Windows.Foundation.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <functional>
#include <optional>

namespace OpenNet::Core::Torrent
{
    // Handles fetching torrent metadata from magnet links or torrent files
    // Uses a temporary session to download only metadata without actual file content
    class TorrentMetadataFetcher
    {
    public:
        TorrentMetadataFetcher();
        ~TorrentMetadataFetcher();

        TorrentMetadataFetcher(TorrentMetadataFetcher const&) = delete;
        TorrentMetadataFetcher& operator=(TorrentMetadataFetcher const&) = delete;

        // Fetch metadata from a magnet link or torrent file path
        // Uses callbacks instead of returning WinRT type
        winrt::Windows::Foundation::IAsyncAction FetchMetadataAsync(
            std::string const& torrentSource,
            std::function<void(TorrentMetadataInfo const&)> onSuccess,
            std::function<void(std::string const&)> onError,
            int timeoutSeconds = 60);

        // Set progress callback
        void SetProgressCallback(MetadataProgressCallback callback);

        // Cancel ongoing fetch operation
        void Cancel();

        // Check if currently fetching
        bool IsFetching() const { return m_isFetching.load(); }

        // Get the result after fetch completes (use after FetchMetadataAsync)
        std::optional<TorrentMetadataInfo> GetResult() const;

        // Parse a torrent file directly (synchronous)
        static std::optional<TorrentMetadataInfo> ParseTorrentFile(std::string const& filePath);

        // Validate if string is a valid magnet link or torrent file path
        static bool IsValidTorrentSource(std::string const& source);
        static bool IsMagnetLink(std::string const& source);
        static bool IsTorrentFile(std::string const& source);

    private:
        // Initialize libtorrent session for metadata fetching
        bool InitializeSession();

        // Extract metadata from torrent handle
        TorrentMetadataInfo ExtractMetadata(libtorrent::torrent_handle const& handle);

        // Alert processing
        void ProcessAlerts();

        std::unique_ptr<libtorrent::session> m_session;
        libtorrent::torrent_handle m_handle;

        std::mutex m_mutex;
        std::atomic<bool> m_isFetching{ false };
        std::atomic<bool> m_cancelled{ false };
        std::atomic<bool> m_metadataReceived{ false };
        std::atomic<bool> m_metadataFailed{ false };

        MetadataProgressCallback m_progressCallback;
        std::optional<TorrentMetadataInfo> m_result;
        std::string m_errorMessage;
    };

} // namespace OpenNet::Core::Torrent
