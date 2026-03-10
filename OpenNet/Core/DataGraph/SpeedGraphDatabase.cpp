/*
 * PROJECT:   OpenNet
 * FILE:      Core/DataGraph/SpeedGraphDatabase.cpp
 * PURPOSE:   SQLite persistence for speed graph data points.
 *
 * LICENSE:   The MIT License
 */

#include "pch.h"
#include "Core/DataGraph/SpeedGraphDatabase.h"
#include "ThirdParty/Sqlite/sqlite3.h"

#include <winrt/Microsoft.Windows.Storage.h>

namespace OpenNet::Core
{
    SpeedGraphDatabase& SpeedGraphDatabase::Instance()
    {
        static SpeedGraphDatabase s_instance;
        return s_instance;
    }

    SpeedGraphDatabase::~SpeedGraphDatabase()
    {
        Close();
    }

    bool SpeedGraphDatabase::Initialize()
    {
        std::lock_guard lk(m_mutex);
        if (m_initialized) return true;

        try
        {
            auto localFolder = winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().LocalFolder();
            std::wstring dbPath = std::wstring(localFolder.Path().c_str()) + L"\\speed_graph.db";

            int rc = sqlite3_open16(dbPath.c_str(), &m_db);
            if (rc != SQLITE_OK)
            {
                OutputDebugStringA(("SpeedGraphDatabase: Failed to open database: " +
                    std::string(sqlite3_errmsg(m_db)) + "\n").c_str());
                return false;
            }

            // WAL mode for better concurrent read/write
            sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
            sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

            CreateTables();
            m_initialized = true;
            return true;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("SpeedGraphDatabase::Initialize error: " +
                std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    void SpeedGraphDatabase::Close()
    {
        std::lock_guard lk(m_mutex);
        if (m_db)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        m_initialized = false;
    }

    void SpeedGraphDatabase::CreateTables()
    {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS speed_graph (
                task_id  TEXT    NOT NULL,
                percent  INTEGER NOT NULL,
                speed_kb INTEGER NOT NULL,
                PRIMARY KEY (task_id, percent)
            );
        )";

        char* errMsg = nullptr;
        int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK)
        {
            OutputDebugStringA(("SpeedGraphDatabase: CreateTables error: " +
                std::string(errMsg ? errMsg : "unknown") + "\n").c_str());
            sqlite3_free(errMsg);
        }
    }

    void SpeedGraphDatabase::SavePoint(std::string const& taskId, int percent, uint64_t speedKB)
    {
        std::lock_guard lk(m_mutex);
        if (!m_db) return;

        const char* sql =
            "INSERT OR REPLACE INTO speed_graph (task_id, percent, speed_kb) VALUES (?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            OutputDebugStringA("SpeedGraphDatabase: SavePoint prepare failed\n");
            return;
        }

        sqlite3_bind_text(stmt, 1, taskId.c_str(), static_cast<int>(taskId.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, percent);
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(speedKB));

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::vector<SpeedPoint> SpeedGraphDatabase::LoadPoints(std::string const& taskId) const
    {
        std::lock_guard lk(m_mutex);
        std::vector<SpeedPoint> result;
        if (!m_db) return result;

        const char* sql =
            "SELECT percent, speed_kb FROM speed_graph WHERE task_id = ? ORDER BY percent ASC LIMIT 100;";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            OutputDebugStringA("SpeedGraphDatabase: LoadPoints prepare failed\n");
            return result;
        }

        sqlite3_bind_text(stmt, 1, taskId.c_str(), static_cast<int>(taskId.size()), SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            SpeedPoint pt;
            pt.percent = sqlite3_column_int(stmt, 0);
            pt.speedKB = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
            result.push_back(pt);
        }

        sqlite3_finalize(stmt);
        return result;
    }

    void SpeedGraphDatabase::DeleteTask(std::string const& taskId)
    {
        std::lock_guard lk(m_mutex);
        if (!m_db) return;

        const char* sql = "DELETE FROM speed_graph WHERE task_id = ?;";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return;

        sqlite3_bind_text(stmt, 1, taskId.c_str(), static_cast<int>(taskId.size()), SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

} // namespace OpenNet::Core
