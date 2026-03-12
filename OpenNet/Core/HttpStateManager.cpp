/*
 * PROJECT:   OpenNet
 * FILE:      Core/HttpStateManager.cpp
 * PURPOSE:   Persistence for HTTP/HTTPS/FTP download records (Aria2-based)
 *            SQLite backend for reliable, crash-safe storage.
 *
 * LICENSE:   Attribution-NonCommercial-ShareAlike 4.0 International
 */

#include "pch.h"
#include "Core/HttpStateManager.h"

#include "ThirdParty/Sqlite/sqlite3.h"
#include "Core/IO/FileSystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>

// JSON is only needed for one-time migration from the old http_downloads.json
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace OpenNet::Core
{
    // ------------------------------------------------------------------
    //  Singleton
    // ------------------------------------------------------------------
    HttpStateManager& HttpStateManager::Instance()
    {
        static HttpStateManager s_instance;
        return s_instance;
    }

    HttpStateManager::~HttpStateManager()
    {
        Close();
    }

    // ------------------------------------------------------------------
    //  Lifecycle
    // ------------------------------------------------------------------
    void HttpStateManager::Initialize()
    {
        std::lock_guard lock(m_mutex);
        if (m_initialized) return;

        try
        {
            m_folderPath = winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW();
        }
        catch (...)
        {
            m_folderPath = L".";
        }

        m_dbPath = m_folderPath + L"\\http_downloads.db";

        std::string dbPathUtf8 = winrt::to_string(m_dbPath);
        int rc = sqlite3_open(dbPathUtf8.c_str(), &m_db);
        if (rc != SQLITE_OK)
        {
            OutputDebugStringA("HttpStateManager: Failed to open SQLite database\n");
            if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
            return;
        }

        // Enable WAL mode for better concurrent read/write performance
        sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

        CreateTables();

        m_initialized = true;

        // Migrate data from old JSON file (one-time)
        MigrateFromJsonIfNeeded();
    }

    void HttpStateManager::Close()
    {
        std::lock_guard lock(m_mutex);
        if (m_db)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        m_initialized = false;
    }

    void HttpStateManager::CreateTables()
    {
        if (!m_db) return;

        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS http_downloads (
                record_id       TEXT PRIMARY KEY,
                url             TEXT NOT NULL DEFAULT '',
                save_path       TEXT NOT NULL DEFAULT '',
                file_name       TEXT NOT NULL DEFAULT '',
                name            TEXT NOT NULL DEFAULT '',
                added_timestamp INTEGER NOT NULL DEFAULT 0,
                total_size      INTEGER NOT NULL DEFAULT 0,
                completed_size  INTEGER NOT NULL DEFAULT 0,
                status          INTEGER NOT NULL DEFAULT 0,
                last_gid        TEXT NOT NULL DEFAULT ''
            );
            CREATE INDEX IF NOT EXISTS idx_http_gid ON http_downloads(last_gid);
        )";

        char* errMsg = nullptr;
        int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK)
        {
            OutputDebugStringA(("HttpStateManager: CreateTables error: " + std::string(errMsg ? errMsg : "unknown") + "\n").c_str());
            sqlite3_free(errMsg);
        }
    }

    // ------------------------------------------------------------------
    //  One-time JSON → SQLite migration
    // ------------------------------------------------------------------
    void HttpStateManager::MigrateFromJsonIfNeeded()
    {
        // Check if old JSON file exists
        std::wstring jsonPath = m_folderPath + L"\\http_downloads.json";
        if (!std::filesystem::exists(jsonPath))
            return;

        try
        {
            std::ifstream ifs(jsonPath);
            if (!ifs.is_open()) return;

            json j = json::parse(ifs, nullptr, false);
            if (j.is_discarded() || !j.is_array()) return;

            ifs.close();

            // Insert each record into SQLite (skip duplicates)
            const char* insertSql = R"(
                INSERT OR IGNORE INTO http_downloads
                    (record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
            )";

            sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(m_db, insertSql, -1, &stmt, nullptr);

            for (auto const& item : j)
            {
                std::string recordId  = item.value("recordId", std::string{});
                std::string url       = item.value("url", std::string{});
                std::string savePath  = item.value("savePath", std::string{});
                std::string fileName  = item.value("fileName", std::string{});
                std::string name      = item.value("name", std::string{});
                int64_t addedTs       = item.value("addedTimestamp", int64_t{0});
                int64_t totalSize     = item.value("totalSize", int64_t{0});
                int64_t completedSize = item.value("completedSize", int64_t{0});
                int status            = item.value("status", 0);
                std::string lastGid   = item.value("lastGid", std::string{});

                if (recordId.empty()) continue;

                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, recordId.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, url.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, savePath.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 4, fileName.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 5, name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 6, addedTs);
                sqlite3_bind_int64(stmt, 7, totalSize);
                sqlite3_bind_int64(stmt, 8, completedSize);
                sqlite3_bind_int(stmt, 9, status);
                sqlite3_bind_text(stmt, 10, lastGid.c_str(), -1, SQLITE_TRANSIENT);

                sqlite3_step(stmt);
            }

            sqlite3_finalize(stmt);
            sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);

            // Remove old JSON file so migration doesn't run again.
            // Use remove instead of rename for reliability.
            try { std::filesystem::remove(jsonPath); }
            catch (...)
            {
                // If deletion fails, try renaming as fallback.
                std::wstring backupPath = jsonPath + L".migrated";
                try { std::filesystem::rename(jsonPath, backupPath); }
                catch (...) {}
            }

            OutputDebugStringA("HttpStateManager: Migrated records from JSON to SQLite\n");
        }
        catch (...)
        {
            OutputDebugStringA("HttpStateManager: JSON migration failed (non-fatal)\n");
        }
    }

    // ------------------------------------------------------------------
    //  Record CRUD
    // ------------------------------------------------------------------
    std::optional<HttpDownloadRecord> HttpStateManager::FindActiveByUrl(std::string const& url) const
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return std::nullopt;

        // status: 0=pending, 1=downloading, 2=paused → active
        const char* sql = "SELECT record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid "
                          "FROM http_downloads WHERE url = ? AND status < 3 LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<HttpDownloadRecord> result;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            HttpDownloadRecord rec;
            auto safeText = [&](int col) -> const char* {
                auto ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
                return ptr ? ptr : "";
            };
            rec.recordId       = safeText(0);
            rec.url            = safeText(1);
            rec.savePath       = safeText(2);
            rec.fileName       = safeText(3);
            rec.name           = safeText(4);
            rec.addedTimestamp  = sqlite3_column_int64(stmt, 5);
            rec.totalSize      = sqlite3_column_int64(stmt, 6);
            rec.completedSize  = sqlite3_column_int64(stmt, 7);
            rec.status         = sqlite3_column_int(stmt, 8);
            rec.lastGid        = safeText(9);
            result = std::move(rec);
        }

        sqlite3_finalize(stmt);
        return result;
    }

    std::string HttpStateManager::AddRecord(std::string const& url, std::string const& savePath, std::string const& fileName)
    {
        // Check for existing active record with the same URL to prevent duplicates.
        // Note: FindActiveByUrl also takes m_mutex, so call it before locking.
        auto existing = FindActiveByUrl(url);
        if (existing.has_value())
        {
            OutputDebugStringA(("HttpStateManager: Duplicate URL detected, returning existing recordId: " + existing->recordId + "\n").c_str());
            return existing->recordId;
        }

        std::lock_guard lock(m_mutex);
        if (!m_db) return {};

        std::string recordId = GenerateRecordId();
        std::string name = fileName.empty() ? url : fileName;
        int64_t addedTs = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        const char* sql = R"(
            INSERT INTO http_downloads
                (record_id, url, save_path, file_name, name, added_timestamp, status)
            VALUES (?, ?, ?, ?, ?, ?, 0);
        )";

        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, recordId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, savePath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, fileName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 6, addedTs);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE)
        {
            OutputDebugStringA("HttpStateManager: AddRecord failed\n");
            return {};
        }

        return recordId;
    }

    void HttpStateManager::UpdateRecordGid(std::string const& recordId, std::string const& gid)
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return;

        const char* sql = "UPDATE http_downloads SET last_gid = ? WHERE record_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, gid.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, recordId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void HttpStateManager::UpdateRecordName(std::string const& recordId, std::string const& name)
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return;

        const char* sql = "UPDATE http_downloads SET name = ? WHERE record_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, recordId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void HttpStateManager::UpdateRecordProgress(std::string const& recordId, int64_t completedSize, int64_t totalSize)
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return;

        const char* sql = "UPDATE http_downloads SET completed_size = ?, total_size = ? WHERE record_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, completedSize);
        sqlite3_bind_int64(stmt, 2, totalSize);
        sqlite3_bind_text(stmt, 3, recordId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void HttpStateManager::UpdateRecordStatus(std::string const& recordId, int status)
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return;

        const char* sql = "UPDATE http_downloads SET status = ? WHERE record_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, status);
        sqlite3_bind_text(stmt, 2, recordId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void HttpStateManager::DeleteRecord(std::string const& recordId)
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return;

        const char* sql = "DELETE FROM http_downloads WHERE record_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, recordId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // ------------------------------------------------------------------
    //  Lookup
    // ------------------------------------------------------------------
    std::optional<HttpDownloadRecord> HttpStateManager::FindByGid(std::string const& gid) const
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return std::nullopt;

        const char* sql = "SELECT record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid FROM http_downloads WHERE last_gid = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, gid.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<HttpDownloadRecord> result;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            HttpDownloadRecord rec;
            rec.recordId       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            rec.url            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            rec.savePath       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            rec.fileName       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            rec.name           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            rec.addedTimestamp  = sqlite3_column_int64(stmt, 5);
            rec.totalSize      = sqlite3_column_int64(stmt, 6);
            rec.completedSize  = sqlite3_column_int64(stmt, 7);
            rec.status         = sqlite3_column_int(stmt, 8);
            rec.lastGid        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            result = std::move(rec);
        }

        sqlite3_finalize(stmt);
        return result;
    }

    std::optional<HttpDownloadRecord> HttpStateManager::FindByRecordId(std::string const& recordId) const
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return std::nullopt;

        const char* sql = "SELECT record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid FROM http_downloads WHERE record_id = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, recordId.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<HttpDownloadRecord> result;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            HttpDownloadRecord rec;
            rec.recordId       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            rec.url            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            rec.savePath       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            rec.fileName       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            rec.name           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            rec.addedTimestamp  = sqlite3_column_int64(stmt, 5);
            rec.totalSize      = sqlite3_column_int64(stmt, 6);
            rec.completedSize  = sqlite3_column_int64(stmt, 7);
            rec.status         = sqlite3_column_int(stmt, 8);
            rec.lastGid        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            result = std::move(rec);
        }

        sqlite3_finalize(stmt);
        return result;
    }

    std::vector<HttpDownloadRecord> HttpStateManager::LoadAllRecords() const
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return {};

        const char* sql = "SELECT record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid FROM http_downloads ORDER BY added_timestamp DESC;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);

        std::vector<HttpDownloadRecord> records;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            HttpDownloadRecord rec;
            auto safeText = [&](int col) -> const char* {
                auto ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
                return ptr ? ptr : "";
            };
            rec.recordId       = safeText(0);
            rec.url            = safeText(1);
            rec.savePath       = safeText(2);
            rec.fileName       = safeText(3);
            rec.name           = safeText(4);
            rec.addedTimestamp  = sqlite3_column_int64(stmt, 5);
            rec.totalSize      = sqlite3_column_int64(stmt, 6);
            rec.completedSize  = sqlite3_column_int64(stmt, 7);
            rec.status         = sqlite3_column_int(stmt, 8);
            rec.lastGid        = safeText(9);
            records.push_back(std::move(rec));
        }

        sqlite3_finalize(stmt);
        return records;
    }

    void HttpStateManager::Save()
    {
        // No-op for SQLite with WAL mode — all writes are immediately durable.
        // Kept for backward API compatibility.
    }

    // ------------------------------------------------------------------
    //  ID generation
    // ------------------------------------------------------------------
    std::string HttpStateManager::GenerateRecordId()
    {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

        static std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, 0xFFFF);

        char buf[32];
        snprintf(buf, sizeof(buf), "%013llx%04x",
                 static_cast<unsigned long long>(ms), dist(rng));
        return std::string(buf);
    }
}
