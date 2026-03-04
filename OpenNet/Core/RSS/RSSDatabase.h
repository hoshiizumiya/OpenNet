/*
 * PROJECT:   OpenNet
 * FILE:      Core/RSS/RSSDatabase.h
 * PURPOSE:   SQLite-based persistence for RSS feeds and items
 *
 * LICENSE:   The MIT License
 */

#pragma once

#include "RSSTypes.h"
#include <string>
#include <vector>
#include <mutex>
#include <optional>

struct sqlite3;

namespace OpenNet::Core::RSS
{
    /// <summary>
    /// SQLite database for RSS feed subscriptions and items.
    /// Schema:
    ///   rss_feeds  (id, url, title, description, save_path, update_interval_min,
    ///               auto_download, filter_pattern, enabled, last_updated)
    ///   rss_items  (id, feed_id, guid, title, link, description, enclosure_url,
    ///               enclosure_type, enclosure_length, pub_date, category,
    ///               is_downloaded, UNIQUE(feed_id, guid))
    /// </summary>
    class RSSDatabase
    {
    public:
        static RSSDatabase& Instance();

        RSSDatabase(RSSDatabase const&) = delete;
        RSSDatabase& operator=(RSSDatabase const&) = delete;

        // ---- Feed CRUD ----

        /// Insert or replace a feed subscription (without items)
        void UpsertFeed(RSSFeed const& feed);

        /// Delete a feed and all its items (CASCADE)
        void DeleteFeed(std::wstring const& feedId);

        /// Load all feed subscriptions (with items)
        std::vector<RSSFeed> LoadAllFeeds();

        /// Load a single feed by ID (with items)
        std::optional<RSSFeed> LoadFeed(std::wstring const& feedId);

        /// Update feed metadata after a successful fetch
        void UpdateFeedMeta(std::wstring const& feedId,
                            std::wstring const& title,
                            std::wstring const& description,
                            int64_t lastUpdatedEpoch);

        // ---- Item CRUD ----

        /// Insert new items (skip duplicates via UNIQUE constraint).
        /// Returns the number of newly inserted items.
        int InsertItems(std::wstring const& feedId, std::vector<RSSItem> const& items);

        /// Load all items for a feed
        std::vector<RSSItem> LoadItems(std::wstring const& feedId);

        /// Mark a single item as downloaded
        void MarkItemDownloaded(std::wstring const& feedId, std::wstring const& guid);

        /// Keep only the most recent N items per feed (delete older ones)
        void PruneItems(std::wstring const& feedId, int maxItems = 100);

        // ---- Migration ----

        /// Check if the database has any feeds (used to detect first-run migration)
        bool HasFeeds() const;

    private:
        RSSDatabase();
        ~RSSDatabase();

        void Open();
        void CreateTables();

        /// Internal: load items without acquiring mutex (caller must hold lock)
        std::vector<RSSItem> LoadItemsInternal(std::wstring const& feedId);

        // Helpers
        static std::string WideToUtf8(std::wstring const& w);
        static std::wstring Utf8ToWide(std::string const& s);

        sqlite3* m_db{ nullptr };
        mutable std::mutex m_mutex;
    };
}
