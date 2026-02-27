#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <optional>

namespace OpenNet::Core::RSS
{
    /// <summary>
    /// Represents a single item in an RSS feed
    /// </summary>
    struct RSSItem
    {
        std::wstring title;
        std::wstring link;
        std::wstring description;
        std::wstring guid;
        std::wstring enclosureUrl;      // Torrent URL or magnet link
        std::wstring enclosureType;     // MIME type (application/x-bittorrent)
        uint64_t enclosureLength{ 0 };  // File size in bytes
        std::chrono::system_clock::time_point pubDate;
        std::wstring category;
        bool isDownloaded{ false };
    };

    /// <summary>
    /// Represents an RSS feed subscription
    /// </summary>
    struct RSSFeed
    {
        std::wstring id;                // Unique identifier
        std::wstring title;
        std::wstring url;
        std::wstring description;
        std::wstring savePath;          // Default save path for torrents
        std::chrono::system_clock::time_point lastUpdated;
        std::chrono::minutes updateInterval{ 30 };
        bool autoDownload{ false };
        std::wstring filterPattern;     // Regex pattern for auto-download filtering
        std::vector<RSSItem> items;
        bool enabled{ true };
    };

    /// <summary>
    /// RSS feed subscription configuration for persistence
    /// </summary>
    struct RSSSubscription
    {
        std::wstring id;
        std::wstring url;
        std::wstring name;
        std::wstring savePath;
        std::chrono::minutes updateInterval{ 30 };
        bool autoDownload{ false };
        std::wstring filterPattern;
        bool enabled{ true };
    };
}
