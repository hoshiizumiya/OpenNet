/*
 * PROJECT:   OpenNet
 * FILE:      Core/RSS/RSSDatabase.cpp
 * PURPOSE:   SQLite-based persistence for RSS feeds and items
 *
 * LICENSE:   Attribution-NonCommercial-ShareAlike 4.0 International
 */

#include "pch.h"
#include "RSSDatabase.h"

#include <ThirdParty/Sqlite/sqlite3.h>
#include "Core/IO/FileSystem.h"
#include <filesystem>
#include <codecvt>

namespace OpenNet::Core::RSS
{
	// ---------------------------------------------------------------
	//  Singleton
	// ---------------------------------------------------------------
	RSSDatabase& RSSDatabase::Instance()
	{
		static RSSDatabase s_instance;
		return s_instance;
	}

	RSSDatabase::RSSDatabase()
	{
		Open();
		CreateTables();
	}

	RSSDatabase::~RSSDatabase()
	{
		if (m_db)
		{
			sqlite3_close(m_db);
			m_db = nullptr;
		}
	}

	// ---------------------------------------------------------------
	//  Open / schema
	// ---------------------------------------------------------------
	void RSSDatabase::Open()
	{
		try
		{
			int rc = sqlite3_open16((std::filesystem::path(winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW()) / "rss_data.db").c_str(), &m_db);
			if (rc != SQLITE_OK)
			{
				OutputDebugString((L"RSSDatabase: failed to open DB – " + std::wstring(sqlite3_errmsg(m_db), sqlite3_errmsg(m_db) + strlen(sqlite3_errmsg(m_db))) + L"\n").c_str());
				m_db = nullptr;
				return;
			}

			// WAL mode + relaxed sync for performance
			sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
			sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
			sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
		}
		catch (std::exception const& ex)
		{
			OutputDebugString((L"RSSDatabase::Open exception: " + std::wstring(std::string(ex.what()).begin(), std::string(ex.what()).end()) + L"\n").c_str());
		}
	}

	void RSSDatabase::CreateTables()
	{
		if (!m_db) return;

		char const* sql = R"(
            CREATE TABLE IF NOT EXISTS rss_feeds (
                id              TEXT PRIMARY KEY,
                url             TEXT NOT NULL,
                title           TEXT,
                description     TEXT,
                save_path       TEXT,
                update_interval_min INTEGER DEFAULT 30,
                auto_download   INTEGER DEFAULT 0,
                filter_pattern  TEXT,
                enabled         INTEGER DEFAULT 1,
                last_updated    INTEGER DEFAULT 0
            );

            CREATE TABLE IF NOT EXISTS rss_items (
                id              INTEGER PRIMARY KEY AUTOINCREMENT,
                feed_id         TEXT NOT NULL REFERENCES rss_feeds(id) ON DELETE CASCADE,
                guid            TEXT,
                title           TEXT,
                link            TEXT,
                description     TEXT,
                enclosure_url   TEXT,
                enclosure_type  TEXT,
                enclosure_length INTEGER DEFAULT 0,
                pub_date        INTEGER DEFAULT 0,
                category        TEXT,
                is_downloaded   INTEGER DEFAULT 0,
                UNIQUE(feed_id, guid)
            );

            CREATE INDEX IF NOT EXISTS idx_rss_items_feed_id ON rss_items(feed_id);
            CREATE INDEX IF NOT EXISTS idx_rss_items_pub_date ON rss_items(pub_date);
        )";

		char* errMsg = nullptr;
		int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
		if (rc != SQLITE_OK)
		{
			OutputDebugStringA(("RSSDatabase::CreateTables error: " + std::string(errMsg ? errMsg : "unknown") + "\n").c_str());
			sqlite3_free(errMsg);
		}
	}

	// ---------------------------------------------------------------
	//  Feed CRUD
	// ---------------------------------------------------------------
	void RSSDatabase::UpsertFeed(RSSFeed const& feed)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		char const* sql =
			"INSERT OR REPLACE INTO rss_feeds "
			"(id, url, title, description, save_path, update_interval_min, auto_download, filter_pattern, enabled, last_updated) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return;

		auto idU = WideToUtf8(feed.id);
		auto urlU = WideToUtf8(feed.url);
		auto titleU = WideToUtf8(feed.title);
		auto descU = WideToUtf8(feed.description);
		auto saveU = WideToUtf8(feed.savePath);
		auto filterU = WideToUtf8(feed.filterPattern);
		auto lastUpdated = std::chrono::duration_cast<std::chrono::seconds>(
			feed.lastUpdated.time_since_epoch())
			.count();

		sqlite3_bind_text(stmt, 1, idU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, urlU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, titleU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, descU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, saveU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 6, static_cast<int>(feed.updateInterval.count()));
		sqlite3_bind_int(stmt, 7, feed.autoDownload ? 1 : 0);
		sqlite3_bind_text(stmt, 8, filterU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 9, feed.enabled ? 1 : 0);
		sqlite3_bind_int64(stmt, 10, lastUpdated);

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	void RSSDatabase::DeleteFeed(std::wstring const& feedId)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		// Foreign key ON DELETE CASCADE will remove items automatically
		char const* sql = "DELETE FROM rss_feeds WHERE id = ?;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return;

		auto idU = WideToUtf8(feedId);
		sqlite3_bind_text(stmt, 1, idU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	std::vector<RSSFeed> RSSDatabase::LoadAllFeeds()
	{
		std::lock_guard lk(m_mutex);
		std::vector<RSSFeed> result;
		if (!m_db) return result;

		char const* sql = "SELECT id, url, title, description, save_path, "
			"update_interval_min, auto_download, filter_pattern, enabled, last_updated "
			"FROM rss_feeds;";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return result;

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			RSSFeed feed;
			feed.id = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 0)));
			feed.url = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 1)));
			feed.title = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 2)));
			feed.description = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 3)));
			feed.savePath = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 4)));
			feed.updateInterval = std::chrono::minutes(sqlite3_column_int(stmt, 5));
			feed.autoDownload = sqlite3_column_int(stmt, 6) != 0;
			feed.filterPattern = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 7)));
			feed.enabled = sqlite3_column_int(stmt, 8) != 0;
			feed.lastUpdated = std::chrono::system_clock::time_point(
				std::chrono::seconds(sqlite3_column_int64(stmt, 9)));

			result.push_back(std::move(feed));
		}
		sqlite3_finalize(stmt);

		// Load items for each feed (lock is already held)
		for (auto& feed : result)
		{
			feed.items = LoadItemsInternal(feed.id);
		}

		return result;
	}

	std::optional<RSSFeed> RSSDatabase::LoadFeed(std::wstring const& feedId)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return std::nullopt;

		char const* sql = "SELECT id, url, title, description, save_path, "
			"update_interval_min, auto_download, filter_pattern, enabled, last_updated "
			"FROM rss_feeds WHERE id = ?;";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return std::nullopt;

		auto idU = WideToUtf8(feedId);
		sqlite3_bind_text(stmt, 1, idU.c_str(), -1, SQLITE_TRANSIENT);

		if (sqlite3_step(stmt) != SQLITE_ROW)
		{
			sqlite3_finalize(stmt);
			return std::nullopt;
		}

		RSSFeed feed;
		feed.id = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 0)));
		feed.url = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 1)));
		feed.title = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 2)));
		feed.description = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 3)));
		feed.savePath = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 4)));
		feed.updateInterval = std::chrono::minutes(sqlite3_column_int(stmt, 5));
		feed.autoDownload = sqlite3_column_int(stmt, 6) != 0;
		feed.filterPattern = Utf8ToWide(reinterpret_cast<char const*>(sqlite3_column_text(stmt, 7)));
		feed.enabled = sqlite3_column_int(stmt, 8) != 0;
		feed.lastUpdated = std::chrono::system_clock::time_point(
			std::chrono::seconds(sqlite3_column_int64(stmt, 9)));

		sqlite3_finalize(stmt);

		feed.items = LoadItemsInternal(feed.id);
		return feed;
	}

	void RSSDatabase::UpdateFeedMeta(std::wstring const& feedId,
									 std::wstring const& title,
									 std::wstring const& description,
									 int64_t lastUpdatedEpoch)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		char const* sql =
			"UPDATE rss_feeds SET title = ?, description = ?, last_updated = ? WHERE id = ?;";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return;

		auto titleU = WideToUtf8(title);
		auto descU = WideToUtf8(description);
		auto idU = WideToUtf8(feedId);

		sqlite3_bind_text(stmt, 1, titleU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, descU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, lastUpdatedEpoch);
		sqlite3_bind_text(stmt, 4, idU.c_str(), -1, SQLITE_TRANSIENT);

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	// ---------------------------------------------------------------
	//  Item CRUD
	// ---------------------------------------------------------------
	int RSSDatabase::InsertItems(std::wstring const& feedId, std::vector<RSSItem> const& items)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db || items.empty()) return 0;

		int inserted = 0;

		sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

		char const* sql =
			"INSERT OR IGNORE INTO rss_items "
			"(feed_id, guid, title, link, description, enclosure_url, enclosure_type, "
			"enclosure_length, pub_date, category, is_downloaded) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
		{
			sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
			return 0;
		}

		auto feedIdU = WideToUtf8(feedId);

		for (auto const& item : items)
		{
			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);

			auto guidU = WideToUtf8(item.guid.empty() ? item.link : item.guid);
			auto titleU = WideToUtf8(item.title);
			auto linkU = WideToUtf8(item.link);
			auto descU = WideToUtf8(item.description);
			auto encUrlU = WideToUtf8(item.enclosureUrl);
			auto encTypeU = WideToUtf8(item.enclosureType);
			auto catU = WideToUtf8(item.category);
			auto pubDateEpoch = std::chrono::duration_cast<std::chrono::seconds>(
				item.pubDate.time_since_epoch())
				.count();

			sqlite3_bind_text(stmt, 1, feedIdU.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, guidU.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, titleU.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, linkU.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, descU.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 6, encUrlU.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 7, encTypeU.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 8, static_cast<int64_t>(item.enclosureLength));
			sqlite3_bind_int64(stmt, 9, pubDateEpoch);
			sqlite3_bind_text(stmt, 10, catU.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 11, item.isDownloaded ? 1 : 0);

			if (sqlite3_step(stmt) == SQLITE_DONE)
			{
				if (sqlite3_changes(m_db) > 0)
					++inserted;
			}
		}

		sqlite3_finalize(stmt);
		sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);

		return inserted;
	}

	std::vector<RSSItem> RSSDatabase::LoadItems(std::wstring const& feedId)
	{
		std::lock_guard lk(m_mutex);
		return LoadItemsInternal(feedId);
	}

	void RSSDatabase::MarkItemDownloaded(std::wstring const& feedId, std::wstring const& guid)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		char const* sql =
			"UPDATE rss_items SET is_downloaded = 1 WHERE feed_id = ? AND guid = ?;";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return;

		auto feedU = WideToUtf8(feedId);
		auto guidU = WideToUtf8(guid);
		sqlite3_bind_text(stmt, 1, feedU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, guidU.c_str(), -1, SQLITE_TRANSIENT);

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	void RSSDatabase::PruneItems(std::wstring const& feedId, int maxItems)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		// Delete items that are NOT in the top N by pub_date
		char const* sql =
			"DELETE FROM rss_items WHERE feed_id = ? AND id NOT IN "
			"(SELECT id FROM rss_items WHERE feed_id = ? ORDER BY pub_date DESC LIMIT ?);";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return;

		auto feedU = WideToUtf8(feedId);
		sqlite3_bind_text(stmt, 1, feedU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, feedU.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, maxItems);

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	// ---------------------------------------------------------------
	//  Migration check
	// ---------------------------------------------------------------
	bool RSSDatabase::HasFeeds() const
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return false;

		char const* sql = "SELECT COUNT(*) FROM rss_feeds;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return false;

		bool has = false;
		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			has = sqlite3_column_int(stmt, 0) > 0;
		}
		sqlite3_finalize(stmt);
		return has;
	}

	// ---------------------------------------------------------------
	//  Private: load items without acquiring lock (for internal use)
	// ---------------------------------------------------------------
	std::vector<RSSItem> RSSDatabase::LoadItemsInternal(std::wstring const& feedId)
	{
		std::vector<RSSItem> items;
		if (!m_db) return items;

		char const* sql =
			"SELECT guid, title, link, description, enclosure_url, enclosure_type, "
			"enclosure_length, pub_date, category, is_downloaded "
			"FROM rss_items WHERE feed_id = ? ORDER BY pub_date DESC;";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return items;

		auto feedU = WideToUtf8(feedId);
		sqlite3_bind_text(stmt, 1, feedU.c_str(), -1, SQLITE_TRANSIENT);

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			RSSItem item;
			auto getText = [&](int col) -> std::wstring
			{
				auto p = reinterpret_cast<char const*>(sqlite3_column_text(stmt, col));
				return p ? Utf8ToWide(p) : std::wstring{};
			};

			item.guid = getText(0);
			item.title = getText(1);
			item.link = getText(2);
			item.description = getText(3);
			item.enclosureUrl = getText(4);
			item.enclosureType = getText(5);
			item.enclosureLength = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
			item.pubDate = std::chrono::system_clock::time_point(
				std::chrono::seconds(sqlite3_column_int64(stmt, 7)));
			item.category = getText(8);
			item.isDownloaded = sqlite3_column_int(stmt, 9) != 0;

			items.push_back(std::move(item));
		}
		sqlite3_finalize(stmt);
		return items;
	}

	// ---------------------------------------------------------------
	//  String conversion helpers
	// ---------------------------------------------------------------
	std::string RSSDatabase::WideToUtf8(std::wstring const& w)
	{
		if (w.empty()) return {};
		int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
									  nullptr, 0, nullptr, nullptr);
		std::string result(len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
							result.data(), len, nullptr, nullptr);
		return result;
	}

	std::wstring RSSDatabase::Utf8ToWide(std::string const& s)
	{
		if (s.empty()) return {};
		int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
									  nullptr, 0);
		std::wstring result(len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
							result.data(), len);
		return result;
	}

} // namespace OpenNet::Core::RSS
