#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace OpenNet::Core::Torrent
{
    // Information about a single file in a torrent
    struct TorrentFileInfo
    {
        std::string path;           // Relative file path within torrent
        int64_t size{};             // File size in bytes
        int priority{4};            // Download priority (0=skip, 1-7=priority levels, 4=normal)
        bool selected{true};        // Whether this file is selected for download
        int fileIndex{};            // Index in the torrent's file list
    };

    // Complete metadata information for a torrent
    struct TorrentMetadataInfo
    {
        std::string infoHash;           // Torrent info hash (hex string)
        std::string name;               // Torrent name
        std::string comment;            // Torrent comment
        std::string creator;            // Torrent creator
        int64_t totalSize{};            // Total size in bytes
        int64_t creationDate{};         // Creation timestamp
        int pieceLength{};              // Piece size in bytes
        int numPieces{};                // Total number of pieces
        bool isPrivate{false};          // Private torrent flag
        
        std::vector<TorrentFileInfo> files;     // List of files
        std::vector<std::string> trackers;      // List of tracker URLs
        std::vector<std::string> webSeeds;      // List of web seed URLs
        
        // Helper methods
        int64_t GetSelectedSize() const
        {
            int64_t size = 0;
            for (const auto& f : files)
            {
                if (f.selected) size += f.size;
            }
            return size;
        }
        
        int GetSelectedFileCount() const
        {
            int count = 0;
            for (const auto& f : files)
            {
                if (f.selected) count++;
            }
            return count;
        }
        
        std::string FormatSize(int64_t bytes) const
        {
            const char* units[] = { "B", "KB", "MB", "GB", "TB" };
            int unitIndex = 0;
            double size = static_cast<double>(bytes);
            
            while (size >= 1024.0 && unitIndex < 4)
            {
                size /= 1024.0;
                unitIndex++;
            }
            
            char buffer[64];
            if (unitIndex == 0)
                snprintf(buffer, sizeof(buffer), "%.0f %s", size, units[unitIndex]);
            else
                snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unitIndex]);
            
            return buffer;
        }
    };

    // Result of metadata fetch operation
    enum class MetadataFetchResult
    {
        Success,
        InvalidLink,
        Timeout,
        NetworkError,
        ParseError,
        Cancelled,
        AlreadyExists
    };

    // Callback types for metadata operations
    using MetadataProgressCallback = std::function<void(const std::string& status, int progressPercent)>;
    using MetadataCompletedCallback = std::function<void(const TorrentMetadataInfo& metadata)>;
    using MetadataFailedCallback = std::function<void(MetadataFetchResult result, const std::string& errorMessage)>;

} // namespace OpenNet::Core::Torrent
