export module OpenNet.Core.torrentCore.TorrentMetadataInfo;

import std;

export namespace OpenNet::Core::Torrent
{
	// Information about a single file in a torrent
	struct TorrentFileInfo
	{
		std::string path;           // Relative file path within torrent
		std::int64_t size{};             // File size in bytes
		std::int32_t priority{ 4 };            // Download priority (0=skip, 1-7=priority levels, 4=normal)
		bool selected{ true };        // Whether this file is selected for download
		std::int32_t fileIndex{};            // Index in the torrent's file list
	};

	// Complete metadata information for a torrent
	struct TorrentMetadataInfo
	{
		std::string infoHash;           // Torrent info hash (hex string)
		std::string name;               // Torrent name
		std::string comment;            // Torrent comment
		std::string creator;            // Torrent creator
		std::int64_t totalSize{};            // Total size in bytes
		std::int64_t creationDate{};         // Creation timestamp
		std::int32_t pieceLength{};              // Piece size in bytes
		std::int32_t numPieces{};                // Total number of pieces
		bool isPrivate{ false };          // Private torrent flag

		std::vector<TorrentFileInfo> files;     // List of files
		std::vector<std::string> trackers;      // List of tracker URLs
		std::vector<std::string> webSeeds;      // List of web seed URLs

		// Helper methods
		std::int64_t GetSelectedSize() const
		{
			std::int64_t size = 0;
			for (const auto& f : files)
			{
				if (f.selected) size += f.size;
			}
			return size;
		}

		std::int32_t GetSelectedFileCount() const
		{
			std::int32_t count = 0;
			for (const auto& f : files)
			{
				if (f.selected) count++;
			}
			return count;
		}

		std::string FormatSize(std::int64_t bytes) const
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
				std::snprintf(buffer, sizeof(buffer), "%.0f %s", size, units[unitIndex]);
			else
				std::snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unitIndex]);

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
	using MetadataProgressCallback = std::function<void(const std::string& status, std::int32_t progressPercent)>;
	using MetadataCompletedCallback = std::function<void(const TorrentMetadataInfo& metadata)>;
	using MetadataFailedCallback = std::function<void(MetadataFetchResult result, const std::string& errorMessage)>;

} // namespace OpenNet::Core::Torrent
