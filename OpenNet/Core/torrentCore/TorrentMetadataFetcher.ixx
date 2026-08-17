export module OpenNet.Core.torrentCore.TorrentMetadataFetcher;

import std;
import OpenNet.Core.torrentCore.TorrentMetadataInfo;
import winrt.Windows.Foundation;

export namespace OpenNet::Core::Torrent
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
            // A timeout <= 0 waits until metadata arrives or the operation is
            // cancelled. This is used by the add-torrent window.
            int timeoutSeconds = 0);

        // Set progress callback
        void SetProgressCallback(MetadataProgressCallback callback);

        // Cancel ongoing fetch operation
        void Cancel();

        // Check if currently fetching
        bool IsFetching() const noexcept;

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
        TorrentMetadataInfo ExtractMetadata(auto const& handle);

        // Alert processing
        void ProcessAlerts();

        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace OpenNet::Core::Torrent
