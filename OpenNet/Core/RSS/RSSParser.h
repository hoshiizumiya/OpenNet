#pragma once
#include "RSSTypes.h"
#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace OpenNet::Core::RSS
{
    /// <summary>
    /// Parses RSS/Atom feeds for torrent subscriptions
    /// </summary>
    class RSSParser
    {
    public:
        /// <summary>
        /// Parse RSS XML content and extract feed information
        /// </summary>
        static std::optional<RSSFeed> Parse(const std::wstring& xmlContent, const std::wstring& feedUrl);

        /// <summary>
        /// Extract torrent/magnet links from RSS items
        /// </summary>
        static std::wstring ExtractTorrentLink(const RSSItem& item);

        /// <summary>
        /// Check if the item matches a filter pattern
        /// </summary>
        static bool MatchesFilter(const RSSItem& item, const std::wstring& filterPattern);

    private:
        /// <summary>
        /// Parse RFC 2822 or ISO 8601 date strings with timezone support
        /// </summary>
        static std::chrono::system_clock::time_point ParsePubDate(const std::wstring& dateStr);
    };
}
