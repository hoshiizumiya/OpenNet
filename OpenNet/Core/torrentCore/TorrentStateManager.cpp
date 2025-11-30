#include "pch.h"
#include "TorrentStateManager.h"

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.h>

#include "ThirdParty/Sqlite/sqlite3.h"

#include <fstream>
#include <random>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace lt = libtorrent;

namespace OpenNet::Core::Torrent
{
    TorrentStateManager::TorrentStateManager() = default;

    TorrentStateManager::~TorrentStateManager()
    {
        CloseDatabase();
    }

    bool TorrentStateManager::Initialize(std::wstring const& basePath)
    {
        std::lock_guard lk(m_dbMutex);
        if (m_initialized) return true;

        try
        {
            if (basePath.empty())
            {
                // Use WinUI3 recommended LocalFolder
                auto localFolder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
                m_storagePath = localFolder.Path().c_str();
            }
            else
            {
                m_storagePath = basePath;
            }

            // Ensure the storage path ends with a separator
            if (!m_storagePath.empty() && m_storagePath.back() != L'\\' && m_storagePath.back() != L'/')
            {
                m_storagePath += L"\\";
            }

            // Create resume data subfolder
            std::wstring resumeFolder = m_storagePath + L"resume_data";
            std::filesystem::create_directories(resumeFolder);

            m_dbPath = m_storagePath + DATABASE_FILENAME;

            if (!InitializeDatabase())
            {
                return false;
            }

            m_initialized = true;
            return true;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("TorrentStateManager::Initialize error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    std::wstring TorrentStateManager::GetStoragePath() const
    {
        return m_storagePath;
    }

    bool TorrentStateManager::InitializeDatabase()
    {
        std::string dbPathUtf8 = winrt::to_string(m_dbPath);
        
        sqlite3* db = nullptr;
        int rc = sqlite3_open(dbPathUtf8.c_str(), &db);
        if (rc != SQLITE_OK)
        {
            if (db) sqlite3_close(db);
            OutputDebugStringA("Failed to open SQLite database\n");
            return false;
        }

        m_db = db;

        if (!CreateTables())
        {
            CloseDatabase();
            return false;
        }

        return true;
    }

    bool TorrentStateManager::CreateTables()
    {
        if (!m_db) return false;

        const char* createTasksTable = R"(
            CREATE TABLE IF NOT EXISTS tasks (
                task_id TEXT PRIMARY KEY,
                magnet_uri TEXT NOT NULL,
                save_path TEXT NOT NULL,
                name TEXT,
                added_timestamp INTEGER NOT NULL,
                total_size INTEGER DEFAULT 0,
                downloaded_size INTEGER DEFAULT 0,
                status INTEGER DEFAULT 0,
                resume_data BLOB
            );
        )";

        const char* createSessionTable = R"(
            CREATE TABLE IF NOT EXISTS session_state (
                id INTEGER PRIMARY KEY CHECK (id = 1),
                state_data BLOB
            );
        )";

        char* errMsg = nullptr;
        int rc = sqlite3_exec(static_cast<sqlite3*>(m_db), createTasksTable, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK)
        {
            if (errMsg)
            {
                OutputDebugStringA(("SQLite error creating tasks table: " + std::string(errMsg) + "\n").c_str());
                sqlite3_free(errMsg);
            }
            return false;
        }

        rc = sqlite3_exec(static_cast<sqlite3*>(m_db), createSessionTable, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK)
        {
            if (errMsg)
            {
                OutputDebugStringA(("SQLite error creating session_state table: " + std::string(errMsg) + "\n").c_str());
                sqlite3_free(errMsg);
            }
            return false;
        }

        return true;
    }

    bool TorrentStateManager::CloseDatabase()
    {
        if (m_db)
        {
            sqlite3_close(static_cast<sqlite3*>(m_db));
            m_db = nullptr;
        }
        return true;
    }

    bool TorrentStateManager::SaveSessionState(lt::session& session)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return false;

        try
        {
            // Get session state and serialize it
            lt::session_params params = session.session_state();
            std::vector<char> buf = lt::write_session_params_buf(params, lt::session::save_dht_state);

            const char* sql = R"(
                INSERT OR REPLACE INTO session_state (id, state_data) VALUES (1, ?);
            )";

            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return false;

            sqlite3_bind_blob(stmt, 1, buf.data(), static_cast<int>(buf.size()), SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            return rc == SQLITE_DONE;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("SaveSessionState error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    bool TorrentStateManager::LoadSessionState(lt::session& session)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return false;

        try
        {
            const char* sql = "SELECT state_data FROM session_state WHERE id = 1;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return false;

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW)
            {
                const void* data = sqlite3_column_blob(stmt, 0);
                int size = sqlite3_column_bytes(stmt, 0);

                if (data && size > 0)
                {
                    lt::span<char const> buf(static_cast<char const*>(data), size);
                    lt::session_params params = lt::read_session_params(buf);
                    session.apply_settings(params.settings);
                    // DHT state is applied automatically
                }
            }

            sqlite3_finalize(stmt);
            return true;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("LoadSessionState error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    bool TorrentStateManager::SaveTaskResumeData(std::string const& taskId, lt::add_torrent_params const& params)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return false;

        try
        {
            // Serialize resume data using libtorrent's write_resume_data
            std::vector<char> buf = lt::write_resume_data_buf(params);

            const char* sql = "UPDATE tasks SET resume_data = ? WHERE task_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return false;

            sqlite3_bind_blob(stmt, 1, buf.data(), static_cast<int>(buf.size()), SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, taskId.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            return rc == SQLITE_DONE;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("SaveTaskResumeData error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    std::optional<lt::add_torrent_params> TorrentStateManager::LoadTaskResumeData(std::string const& taskId)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return std::nullopt;

        try
        {
            const char* sql = "SELECT resume_data FROM tasks WHERE task_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return std::nullopt;

            sqlite3_bind_text(stmt, 1, taskId.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW)
            {
                const void* data = sqlite3_column_blob(stmt, 0);
                int size = sqlite3_column_bytes(stmt, 0);

                if (data && size > 0)
                {
                    lt::span<char const> buf(static_cast<char const*>(data), size);
                    lt::error_code ec;
                    lt::add_torrent_params params = lt::read_resume_data(buf, ec);
                    sqlite3_finalize(stmt);
                    
                    if (!ec)
                    {
                        return params;
                    }
                }
            }

            sqlite3_finalize(stmt);
            return std::nullopt;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("LoadTaskResumeData error: " + std::string(ex.what()) + "\n").c_str());
            return std::nullopt;
        }
    }

    bool TorrentStateManager::SaveTaskMetadata(TaskMetadata const& metadata)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return false;

        try
        {
            const char* sql = R"(
                INSERT OR REPLACE INTO tasks 
                (task_id, magnet_uri, save_path, name, added_timestamp, total_size, downloaded_size, status, resume_data)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
            )";

            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return false;

            sqlite3_bind_text(stmt, 1, metadata.taskId.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, metadata.magnetUri.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, metadata.savePath.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, metadata.name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 5, metadata.addedTimestamp);
            sqlite3_bind_int64(stmt, 6, metadata.totalSize);
            sqlite3_bind_int64(stmt, 7, metadata.downloadedSize);
            sqlite3_bind_int(stmt, 8, metadata.status);
            
            if (metadata.resumeData.empty())
            {
                sqlite3_bind_null(stmt, 9);
            }
            else
            {
                sqlite3_bind_blob(stmt, 9, metadata.resumeData.data(), 
                    static_cast<int>(metadata.resumeData.size()), SQLITE_TRANSIENT);
            }

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            return rc == SQLITE_DONE;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("SaveTaskMetadata error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    std::optional<TaskMetadata> TorrentStateManager::LoadTaskMetadata(std::string const& taskId)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return std::nullopt;

        try
        {
            const char* sql = R"(
                SELECT task_id, magnet_uri, save_path, name, added_timestamp, 
                       total_size, downloaded_size, status, resume_data 
                FROM tasks WHERE task_id = ?;
            )";

            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return std::nullopt;

            sqlite3_bind_text(stmt, 1, taskId.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW)
            {
                TaskMetadata metadata;
                metadata.taskId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                metadata.magnetUri = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                metadata.savePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                
                auto namePtr = sqlite3_column_text(stmt, 3);
                metadata.name = namePtr ? reinterpret_cast<const char*>(namePtr) : "";
                
                metadata.addedTimestamp = sqlite3_column_int64(stmt, 4);
                metadata.totalSize = sqlite3_column_int64(stmt, 5);
                metadata.downloadedSize = sqlite3_column_int64(stmt, 6);
                metadata.status = sqlite3_column_int(stmt, 7);

                const void* blobData = sqlite3_column_blob(stmt, 8);
                int blobSize = sqlite3_column_bytes(stmt, 8);
                if (blobData && blobSize > 0)
                {
                    metadata.resumeData.assign(
                        static_cast<const uint8_t*>(blobData),
                        static_cast<const uint8_t*>(blobData) + blobSize
                    );
                }

                sqlite3_finalize(stmt);
                return metadata;
            }

            sqlite3_finalize(stmt);
            return std::nullopt;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("LoadTaskMetadata error: " + std::string(ex.what()) + "\n").c_str());
            return std::nullopt;
        }
    }

    std::vector<TaskMetadata> TorrentStateManager::LoadAllTasks()
    {
        std::lock_guard lk(m_dbMutex);
        std::vector<TaskMetadata> tasks;
        if (!m_db) return tasks;

        try
        {
            const char* sql = R"(
                SELECT task_id, magnet_uri, save_path, name, added_timestamp, 
                       total_size, downloaded_size, status, resume_data 
                FROM tasks ORDER BY added_timestamp DESC;
            )";

            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return tasks;

            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                TaskMetadata metadata;
                metadata.taskId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                metadata.magnetUri = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                metadata.savePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                
                auto namePtr = sqlite3_column_text(stmt, 3);
                metadata.name = namePtr ? reinterpret_cast<const char*>(namePtr) : "";
                
                metadata.addedTimestamp = sqlite3_column_int64(stmt, 4);
                metadata.totalSize = sqlite3_column_int64(stmt, 5);
                metadata.downloadedSize = sqlite3_column_int64(stmt, 6);
                metadata.status = sqlite3_column_int(stmt, 7);

                const void* blobData = sqlite3_column_blob(stmt, 8);
                int blobSize = sqlite3_column_bytes(stmt, 8);
                if (blobData && blobSize > 0)
                {
                    metadata.resumeData.assign(
                        static_cast<const uint8_t*>(blobData),
                        static_cast<const uint8_t*>(blobData) + blobSize
                    );
                }

                tasks.push_back(std::move(metadata));
            }

            sqlite3_finalize(stmt);
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("LoadAllTasks error: " + std::string(ex.what()) + "\n").c_str());
        }

        return tasks;
    }

    bool TorrentStateManager::DeleteTask(std::string const& taskId)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return false;

        try
        {
            const char* sql = "DELETE FROM tasks WHERE task_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return false;

            sqlite3_bind_text(stmt, 1, taskId.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            return rc == SQLITE_DONE;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("DeleteTask error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    bool TorrentStateManager::UpdateTaskStatus(std::string const& taskId, int status)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return false;

        try
        {
            const char* sql = "UPDATE tasks SET status = ? WHERE task_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return false;

            sqlite3_bind_int(stmt, 1, status);
            sqlite3_bind_text(stmt, 2, taskId.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            return rc == SQLITE_DONE;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("UpdateTaskStatus error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    bool TorrentStateManager::UpdateTaskProgress(std::string const& taskId, int64_t downloadedSize)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return false;

        try
        {
            const char* sql = "UPDATE tasks SET downloaded_size = ? WHERE task_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) return false;

            sqlite3_bind_int64(stmt, 1, downloadedSize);
            sqlite3_bind_text(stmt, 2, taskId.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            return rc == SQLITE_DONE;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("UpdateTaskProgress error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    bool TorrentStateManager::ExportToFile(std::wstring const& filePath)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return false;

        try
        {
            // Export all tasks to a JSON-like format using libtorrent's bencode
            std::vector<TaskMetadata> tasks;
            
            // Temporarily unlock and reload tasks
            m_dbMutex.unlock();
            tasks = LoadAllTasks();
            m_dbMutex.lock();

            lt::entry exportData;
            lt::entry::list_type& taskList = exportData["tasks"].list();

            for (auto const& task : tasks)
            {
                lt::entry taskEntry;
                taskEntry["task_id"] = task.taskId;
                taskEntry["magnet_uri"] = task.magnetUri;
                taskEntry["save_path"] = task.savePath;
                taskEntry["name"] = task.name;
                taskEntry["added_timestamp"] = task.addedTimestamp;
                taskEntry["total_size"] = task.totalSize;
                taskEntry["downloaded_size"] = task.downloadedSize;
                taskEntry["status"] = task.status;
                
                if (!task.resumeData.empty())
                {
                    taskEntry["resume_data"] = std::string(
                        reinterpret_cast<const char*>(task.resumeData.data()),
                        task.resumeData.size()
                    );
                }

                taskList.push_back(std::move(taskEntry));
            }

            std::vector<char> buf;
            lt::bencode(std::back_inserter(buf), exportData);

            std::ofstream file(filePath, std::ios::binary);
            if (!file) return false;

            file.write(buf.data(), buf.size());
            return file.good();
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("ExportToFile error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    bool TorrentStateManager::ImportFromFile(std::wstring const& filePath)
    {
        std::lock_guard lk(m_dbMutex);
        if (!m_db) return false;

        try
        {
            std::ifstream file(filePath, std::ios::binary);
            if (!file) return false;

            std::vector<char> buf((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());

            lt::error_code ec;
            lt::bdecode_node node = lt::bdecode(buf, ec);
            if (ec) return false;

            auto tasksNode = node.dict_find_list("tasks");
            if (!tasksNode) return false;

            for (int i = 0; i < tasksNode.list_size(); ++i)
            {
                auto taskNode = tasksNode.list_at(i);
                if (taskNode.type() != lt::bdecode_node::dict_t) continue;

                TaskMetadata metadata;
                metadata.taskId = taskNode.dict_find_string_value("task_id").to_string();
                metadata.magnetUri = taskNode.dict_find_string_value("magnet_uri").to_string();
                metadata.savePath = taskNode.dict_find_string_value("save_path").to_string();
                metadata.name = taskNode.dict_find_string_value("name").to_string();
                metadata.addedTimestamp = taskNode.dict_find_int_value("added_timestamp");
                metadata.totalSize = taskNode.dict_find_int_value("total_size");
                metadata.downloadedSize = taskNode.dict_find_int_value("downloaded_size");
                metadata.status = static_cast<int>(taskNode.dict_find_int_value("status"));

                auto resumeStr = taskNode.dict_find_string_value("resume_data");
                if (!resumeStr.empty())
                {
                    metadata.resumeData.assign(
                        reinterpret_cast<const uint8_t*>(resumeStr.data()),
                        reinterpret_cast<const uint8_t*>(resumeStr.data()) + resumeStr.size()
                    );
                }

                // Generate new task ID if empty or duplicate
                if (metadata.taskId.empty())
                {
                    metadata.taskId = GenerateTaskId();
                }

                // Temporarily unlock to save metadata
                m_dbMutex.unlock();
                SaveTaskMetadata(metadata);
                m_dbMutex.lock();
            }

            return true;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("ImportFromFile error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    std::string TorrentStateManager::GenerateTaskId()
    {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 0xFFFF);

        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(12) << timestamp
            << std::setw(4) << dis(gen);

        return oss.str();
    }

} // namespace OpenNet::Core::Torrent
