/*
 * PROJECT:   OpenNet
 * FILE:      Core/HttpStateManager.cpp
 * PURPOSE:   Persistence for HTTP/HTTPS/FTP download records (Aria2-based)
 *            SQLite backend for reliable, crash-safe storage.
 *
 * LICENSE:   Attribution-NonCommercial-ShareAlike 4.0 International
 */
module;
#include <Windows.h>
#include <sqlite3.h>
// JSON is only needed for one-time migration from the old http_downloads.json
#include <nlohmann/json.hpp>

module OpenNet.Core.HttpStateManager;
import OpenNet.Core.IO.FileSystem;

using json = nlohmann::json;

namespace OpenNet::Core
{
    namespace
    {
        std::string NormalizeHttpOutputKey(
            std::string const& savePath,
            std::string const& fileName)
        {
            if (savePath.empty() || fileName.empty())
                return {};

            try
            {
                auto path = std::filesystem::path{
                    winrt::to_hstring(savePath).c_str() }
                    / std::filesystem::path{
                        winrt::to_hstring(fileName).c_str() };

                std::error_code error;
                auto const absolute = std::filesystem::absolute(path, error);
                if (!error)
                    path = absolute;
                path = path.lexically_normal();

                auto folded = path.wstring();
                std::ranges::transform(
                    folded,
                    folded.begin(),
                    [](wchar_t const value)
                    {
                        return static_cast<wchar_t>(
                            std::towlower(value));
                    });
                return winrt::to_string(winrt::hstring{ folded });
            }
            catch (...)
            {
                return {};
            }
        }
    }

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
            if (m_db)
            {
                sqlite3_close(m_db); m_db = nullptr;
            }
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
                last_gid        TEXT NOT NULL DEFAULT '',
                completed_timestamp INTEGER NOT NULL DEFAULT 0,
                transfer_mode   INTEGER NOT NULL DEFAULT 0,
                active_engine   INTEGER NOT NULL DEFAULT 0,
                user_requested_paused INTEGER NOT NULL DEFAULT 0,
                canonical_info_hash_v2 TEXT NOT NULL DEFAULT '',
                output_key      TEXT NOT NULL DEFAULT ''
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

        // Older builds could persist more than one active row for the same source
        // URL. Keep the row with the most downloaded data, then enforce the same
        // invariant used by AddRecord at the database boundary.
        sqlite3_exec(m_db, R"(
            DELETE FROM http_downloads AS duplicate
            WHERE duplicate.status < 3
              AND EXISTS (
                SELECT 1 FROM http_downloads AS preferred
                WHERE preferred.url = duplicate.url
                  AND preferred.status < 3
                  AND (preferred.completed_size > duplicate.completed_size
                    OR (preferred.completed_size = duplicate.completed_size
                        AND preferred.added_timestamp < duplicate.added_timestamp)
                    OR (preferred.completed_size = duplicate.completed_size
                        AND preferred.added_timestamp = duplicate.added_timestamp
                        AND preferred.record_id < duplicate.record_id))
              );
            CREATE UNIQUE INDEX IF NOT EXISTS idx_http_active_url
            ON http_downloads(url) WHERE status < 3;
        )", nullptr, nullptr, nullptr);

        // Existing databases predate the completion timestamp column.
        errMsg = nullptr;
        rc = sqlite3_exec(m_db,
                          "ALTER TABLE http_downloads ADD COLUMN completed_timestamp INTEGER NOT NULL DEFAULT 0;",
                          nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK && errMsg)
        {
            // SQLITE_ERROR is expected when the column already exists.
            sqlite3_free(errMsg);
        }

        auto addColumnIfMissing = [this](char const* statement)
        {
            char* error = nullptr;
            auto const result = sqlite3_exec(
                m_db, statement, nullptr, nullptr, &error);
            if (result != SQLITE_OK && error)
                sqlite3_free(error);
        };
        addColumnIfMissing(
            "ALTER TABLE http_downloads ADD COLUMN transfer_mode INTEGER NOT NULL DEFAULT 0;");
        addColumnIfMissing(
            "ALTER TABLE http_downloads ADD COLUMN active_engine INTEGER NOT NULL DEFAULT 0;");
        addColumnIfMissing(
            "ALTER TABLE http_downloads ADD COLUMN user_requested_paused INTEGER NOT NULL DEFAULT 0;");
        addColumnIfMissing(
            "ALTER TABLE http_downloads ADD COLUMN canonical_info_hash_v2 TEXT NOT NULL DEFAULT '';");
        addColumnIfMissing(
            "ALTER TABLE http_downloads ADD COLUMN output_key TEXT NOT NULL DEFAULT '';");

        // Normalize pre-existing active outputs before enforcing the physical
        // writer invariant. The key is intentionally conservative on Windows:
        // absolute + lexical normalization + case folding.
        sqlite3_stmt* readOutputs = nullptr;
        sqlite3_stmt* writeOutputKey = nullptr;
        sqlite3_prepare_v2(
            m_db,
            "SELECT record_id, save_path, file_name FROM http_downloads WHERE output_key = '';",
            -1,
            &readOutputs,
            nullptr);
        sqlite3_prepare_v2(
            m_db,
            "UPDATE http_downloads SET output_key = ? WHERE record_id = ?;",
            -1,
            &writeOutputKey,
            nullptr);
        while (readOutputs
            && writeOutputKey
            && sqlite3_step(readOutputs) == SQLITE_ROW)
        {
            auto const text = [&](int const column) -> char const*
            {
                auto const value = reinterpret_cast<char const*>(
                    sqlite3_column_text(readOutputs, column));
                return value ? value : "";
            };
            auto const recordId = std::string{ text(0) };
            auto const key = NormalizeHttpOutputKey(text(1), text(2));
            if (recordId.empty() || key.empty())
                continue;

            sqlite3_reset(writeOutputKey);
            sqlite3_clear_bindings(writeOutputKey);
            sqlite3_bind_text(
                writeOutputKey, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(
                writeOutputKey, 2, recordId.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(writeOutputKey);
        }
        if (readOutputs) sqlite3_finalize(readOutputs);
        if (writeOutputKey) sqlite3_finalize(writeOutputKey);

        sqlite3_exec(m_db, R"(
            DELETE FROM http_downloads AS duplicate
            WHERE duplicate.status < 3
              AND duplicate.output_key <> ''
              AND EXISTS (
                SELECT 1 FROM http_downloads AS preferred
                WHERE preferred.output_key = duplicate.output_key
                  AND preferred.status < 3
                  AND preferred.record_id <> duplicate.record_id
                  AND (preferred.completed_size > duplicate.completed_size
                    OR (preferred.completed_size = duplicate.completed_size
                        AND preferred.added_timestamp < duplicate.added_timestamp)
                    OR (preferred.completed_size = duplicate.completed_size
                        AND preferred.added_timestamp = duplicate.added_timestamp
                        AND preferred.record_id < duplicate.record_id))
              );
            CREATE UNIQUE INDEX IF NOT EXISTS idx_http_active_output
            ON http_downloads(output_key)
            WHERE status < 3 AND output_key <> '';
        )", nullptr, nullptr, nullptr);
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
                    (record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid, output_key)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
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
                int64_t addedTs       = item.value("addedTimestamp", int64_t{ 0 });
                int64_t totalSize     = item.value("totalSize", int64_t{ 0 });
                int64_t completedSize = item.value("completedSize", int64_t{ 0 });
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
                auto const outputKey =
                    NormalizeHttpOutputKey(savePath, fileName);
                sqlite3_bind_text(
                    stmt, 11, outputKey.c_str(), -1, SQLITE_TRANSIENT);

                sqlite3_step(stmt);
            }

            sqlite3_finalize(stmt);
            sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);

            // Remove old JSON file so migration doesn't run again.
            // Use remove instead of rename for reliability.
            try
            {
                std::filesystem::remove(jsonPath);
            }
            catch (...)
            {
                // If deletion fails, try renaming as fallback.
                std::wstring backupPath = jsonPath + L".migrated";
                try
                {
                    std::filesystem::rename(jsonPath, backupPath);
                }
                catch (...)
                {
                }
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
        const char* sql = "SELECT record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid, completed_timestamp, transfer_mode, active_engine, user_requested_paused, canonical_info_hash_v2 "
            "FROM http_downloads WHERE url = ? AND status < 3 LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<HttpDownloadRecord> result;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            HttpDownloadRecord rec;
            auto safeText = [&](int col) -> const char*
            {
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
            rec.completedTimestamp = sqlite3_column_int64(stmt, 10);
            rec.transferMode = sqlite3_column_int(stmt, 11);
            rec.activeEngine = sqlite3_column_int(stmt, 12);
            rec.userRequestedPaused = sqlite3_column_int(stmt, 13) != 0;
            rec.canonicalInfoHashV2 = safeText(14);
            result = std::move(rec);
        }

        sqlite3_finalize(stmt);
        return result;
    }

    std::optional<HttpDownloadRecord> HttpStateManager::FindActiveByOutputPath(
        std::string const& savePath,
        std::string const& fileName) const
    {
        auto const outputKey = NormalizeHttpOutputKey(savePath, fileName);
        if (outputKey.empty())
            return std::nullopt;

        std::lock_guard lock(m_mutex);
        if (!m_db) return std::nullopt;

        constexpr char sql[] =
            "SELECT record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid, completed_timestamp, transfer_mode, active_engine, user_requested_paused, canonical_info_hash_v2 "
            "FROM http_downloads WHERE output_key = ? AND status < 3 LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(
            stmt, 1, outputKey.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<HttpDownloadRecord> result;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            auto const text = [&](int const column) -> char const*
            {
                auto const value = reinterpret_cast<char const*>(
                    sqlite3_column_text(stmt, column));
                return value ? value : "";
            };
            HttpDownloadRecord rec;
            rec.recordId = text(0);
            rec.url = text(1);
            rec.savePath = text(2);
            rec.fileName = text(3);
            rec.name = text(4);
            rec.addedTimestamp = sqlite3_column_int64(stmt, 5);
            rec.totalSize = sqlite3_column_int64(stmt, 6);
            rec.completedSize = sqlite3_column_int64(stmt, 7);
            rec.status = sqlite3_column_int(stmt, 8);
            rec.lastGid = text(9);
            rec.completedTimestamp = sqlite3_column_int64(stmt, 10);
            rec.transferMode = sqlite3_column_int(stmt, 11);
            rec.activeEngine = sqlite3_column_int(stmt, 12);
            rec.userRequestedPaused = sqlite3_column_int(stmt, 13) != 0;
            rec.canonicalInfoHashV2 = text(14);
            result = std::move(rec);
        }
        sqlite3_finalize(stmt);
        return result;
    }

    std::string HttpStateManager::AddRecord(std::string const& url, std::string const& savePath, std::string const& fileName)
    {
        // These lookups intentionally happen before taking m_mutex because the
        // lookup methods lock the same mutex. URL identity and physical output
        // identity are both active-task uniqueness constraints.
        if (auto existing = FindActiveByUrl(url))
        {
            OutputDebugStringA(("HttpStateManager: Duplicate URL detected, returning existing recordId: " + existing->recordId + "\n").c_str());
            return existing->recordId;
        }
        if (auto existing = FindActiveByOutputPath(savePath, fileName))
        {
            OutputDebugStringA(("HttpStateManager: Duplicate output path detected, returning existing recordId: " + existing->recordId + "\n").c_str());
            return existing->recordId;
        }

        auto const outputKey = NormalizeHttpOutputKey(savePath, fileName);
        std::lock_guard lock(m_mutex);
        if (!m_db) return {};

        std::string recordId = GenerateRecordId();
        std::string name = fileName.empty() ? url : fileName;
        int64_t addedTs = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        const char* sql = R"(
            INSERT INTO http_downloads
                (record_id, url, save_path, file_name, name, added_timestamp, status, output_key)
            VALUES (?, ?, ?, ?, ?, ?, 0, ?);
        )";

        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, recordId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, savePath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, fileName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 6, addedTs);
        sqlite3_bind_text(stmt, 7, outputKey.c_str(), -1, SQLITE_TRANSIENT);

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

    bool HttpStateManager::UpdateRecordOutputPath(
        std::string const& recordId,
        std::string const& savePath,
        std::string const& fileName)
    {
        std::lock_guard lock(m_mutex);
        if (!m_db || recordId.empty() || savePath.empty() || fileName.empty())
            return false;

        auto const outputKey = NormalizeHttpOutputKey(savePath, fileName);
        constexpr char sql[] =
            "UPDATE http_downloads SET save_path = ?, file_name = ?, output_key = ? WHERE record_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, savePath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, fileName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, outputKey.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, recordId.c_str(), -1, SQLITE_TRANSIENT);
        auto const result = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return result == SQLITE_DONE;
    }

    void HttpStateManager::UpdateRecordProgress(std::string const& recordId, int64_t completedSize, int64_t totalSize)
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return;

        const char* sql = "UPDATE http_downloads SET completed_size = MAX(completed_size, ?), total_size = MAX(total_size, ?) WHERE record_id = ?;";
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

        const char* sql = "UPDATE http_downloads SET status = ?, "
            "completed_timestamp = CASE WHEN ? = 3 AND completed_timestamp = 0 THEN ? ELSE completed_timestamp END "
            "WHERE record_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, status);
        sqlite3_bind_int(stmt, 2, status);
        auto const completedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        sqlite3_bind_int64(stmt, 3, completedTimestamp);
        sqlite3_bind_text(stmt, 4, recordId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void HttpStateManager::UpdateRecordTransferPolicy(
        std::string const& recordId,
        int const transferMode,
        bool const userRequestedPaused)
    {
        std::lock_guard lock(m_mutex);
        if (!m_db || recordId.empty()) return;

        constexpr char sql[] =
            "UPDATE http_downloads SET transfer_mode = ?, user_requested_paused = ? WHERE record_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, transferMode);
        sqlite3_bind_int(stmt, 2, userRequestedPaused ? 1 : 0);
        sqlite3_bind_text(stmt, 3, recordId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void HttpStateManager::UpdateRecordActiveEngine(
        std::string const& recordId,
        int const activeEngine,
        std::string const& canonicalInfoHashV2)
    {
        std::lock_guard lock(m_mutex);
        if (!m_db || recordId.empty()) return;

        constexpr char sql[] =
            "UPDATE http_downloads SET active_engine = ?, canonical_info_hash_v2 = ? WHERE record_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, activeEngine);
        sqlite3_bind_text(
            stmt, 2, canonicalInfoHashV2.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, recordId.c_str(), -1, SQLITE_TRANSIENT);
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

        const char* sql = "SELECT record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid, completed_timestamp, transfer_mode, active_engine, user_requested_paused, canonical_info_hash_v2 FROM http_downloads WHERE last_gid = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, gid.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<HttpDownloadRecord> result;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            HttpDownloadRecord rec;
            auto safeText = [&](int const column) -> char const*
            {
                auto const value = reinterpret_cast<char const*>(
                    sqlite3_column_text(stmt, column));
                return value ? value : "";
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
            rec.completedTimestamp = sqlite3_column_int64(stmt, 10);
            rec.transferMode = sqlite3_column_int(stmt, 11);
            rec.activeEngine = sqlite3_column_int(stmt, 12);
            rec.userRequestedPaused = sqlite3_column_int(stmt, 13) != 0;
            rec.canonicalInfoHashV2 = safeText(14);
            result = std::move(rec);
        }

        sqlite3_finalize(stmt);
        return result;
    }

    std::optional<HttpDownloadRecord> HttpStateManager::FindByRecordId(std::string const& recordId) const
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return std::nullopt;

        const char* sql = "SELECT record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid, completed_timestamp, transfer_mode, active_engine, user_requested_paused, canonical_info_hash_v2 FROM http_downloads WHERE record_id = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, recordId.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<HttpDownloadRecord> result;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            HttpDownloadRecord rec;
            auto safeText = [&](int const column) -> char const*
            {
                auto const value = reinterpret_cast<char const*>(
                    sqlite3_column_text(stmt, column));
                return value ? value : "";
            };
            rec.recordId = safeText(0);
            rec.url = safeText(1);
            rec.savePath = safeText(2);
            rec.fileName = safeText(3);
            rec.name = safeText(4);
            rec.addedTimestamp = sqlite3_column_int64(stmt, 5);
            rec.totalSize = sqlite3_column_int64(stmt, 6);
            rec.completedSize = sqlite3_column_int64(stmt, 7);
            rec.status = sqlite3_column_int(stmt, 8);
            rec.lastGid = safeText(9);
            rec.completedTimestamp = sqlite3_column_int64(stmt, 10);
            rec.transferMode = sqlite3_column_int(stmt, 11);
            rec.activeEngine = sqlite3_column_int(stmt, 12);
            rec.userRequestedPaused = sqlite3_column_int(stmt, 13) != 0;
            rec.canonicalInfoHashV2 = safeText(14);
            result = std::move(rec);
        }

        sqlite3_finalize(stmt);
        return result;
    }

    std::vector<HttpDownloadRecord> HttpStateManager::LoadAllRecords() const
    {
        std::lock_guard lock(m_mutex);
        if (!m_db) return {};

        const char* sql = "SELECT record_id, url, save_path, file_name, name, added_timestamp, total_size, completed_size, status, last_gid, completed_timestamp, transfer_mode, active_engine, user_requested_paused, canonical_info_hash_v2 FROM http_downloads ORDER BY added_timestamp DESC;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);

        std::vector<HttpDownloadRecord> records;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            HttpDownloadRecord rec;
            auto safeText = [&](int col) -> const char*
            {
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
            rec.completedTimestamp = sqlite3_column_int64(stmt, 10);
            rec.transferMode = sqlite3_column_int(stmt, 11);
            rec.activeEngine = sqlite3_column_int(stmt, 12);
            rec.userRequestedPaused = sqlite3_column_int(stmt, 13) != 0;
            rec.canonicalInfoHashV2 = safeText(14);
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

        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> dist(0, 0xFFFF);

        char buf[32];
        snprintf(buf, sizeof(buf), "%013llx%04x",
                 static_cast<unsigned long long>(ms), dist(rng));
        return std::string(buf);
    }
}
