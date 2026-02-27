#pragma once
#include <string>
#include <optional>

namespace OpenNet::Core::RSS
{
    /// <summary>
    /// Handles RSS feed URL and content link resolution
    /// </summary>
    class RSSLinkResolver
    {
    public:
        /// <summary>
        /// Normalize RSS feed URL (add https:// if missing)
        /// </summary>
        static std::wstring NormalizeFeedUrl(const std::wstring& url);

        /// <summary>
        /// Normalize content URL (add https:// if missing)
        /// </summary>
        static std::wstring NormalizeContentUrl(const std::wstring& url);

        /// <summary>
        /// Check if URL is a valid magnet link
        /// </summary>
        static bool IsMagnetLink(const std::wstring& url);

        /// <summary>
        /// Check if URL points to a torrent file (.torrent)
        /// </summary>
        static bool IsTorrentFileUrl(const std::wstring& url);

        /// <summary>
        /// Extract filename from URL
        /// </summary>
        static std::wstring ExtractFilename(const std::wstring& url);

        /// <summary>
        /// Validate URL format
        /// </summary>
        static bool IsValidUrl(const std::wstring& url);

        /// <summary>
        /// Get content type from URL
        /// </summary>
        static std::wstring GetContentType(const std::wstring& url);
    };
}
