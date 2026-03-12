/*
 * PROJECT:   OpenNet
 * FILE:      Core/HttpStateManager.h
 * PURPOSE:   Persistence for HTTP/HTTPS/FTP download records (Aria2-based)
 *            Uses SQLite for reliable storage (replaces old JSON backend).
 *
 * LICENSE:   Attribution-NonCommercial-ShareAlike 4.0 International
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace OpenNet::Core
{
    // A single HTTP download record persisted to disk
    struct HttpDownloadRecord
    {
        std::string recordId;           // Unique identifier (hex timestamp)
        std::string url;                // Original download URL
        std::string savePath;           // Download save directory
        std::string fileName;           // File name (may be auto-detected)
        std::string name;               // Display name
        int64_t     addedTimestamp{};   // When the task was added (epoch seconds)
        int64_t     totalSize{};        // Total size in bytes
        int64_t     completedSize{};    // Downloaded size in bytes
        int         status{};           // 0=pending, 1=downloading, 2=paused, 3=completed, 4=failed
        std::string lastGid;            // Last known Aria2 GID (for session re-association)
    };

    // Manages HTTP download record persistence via SQLite
    class HttpStateManager
    {
    public:
        static HttpStateManager& Instance();

        // Initialize: determine storage path, create/open database
        void Initialize();

        // Close the database connection
        void Close();

        // Record CRUD
        std::string AddRecord(std::string const& url, std::string const& savePath, std::string const& fileName);
        void UpdateRecordGid(std::string const& recordId, std::string const& gid);
        void UpdateRecordName(std::string const& recordId, std::string const& name);
        void UpdateRecordProgress(std::string const& recordId, int64_t completedSize, int64_t totalSize);
        void UpdateRecordStatus(std::string const& recordId, int status);
        void DeleteRecord(std::string const& recordId);

        // Lookup
        std::optional<HttpDownloadRecord> FindByGid(std::string const& gid) const;
        std::optional<HttpDownloadRecord> FindByRecordId(std::string const& recordId) const;
        std::optional<HttpDownloadRecord> FindActiveByUrl(std::string const& url) const;
        std::vector<HttpDownloadRecord> LoadAllRecords() const;

        // Flush changes to disk (no-op for SQLite WAL, kept for API compat)
        void Save();

    private:
        HttpStateManager() = default;
        ~HttpStateManager();

        HttpStateManager(HttpStateManager const&) = delete;
        HttpStateManager& operator=(HttpStateManager const&) = delete;

        void CreateTables();
        void MigrateFromJsonIfNeeded();
        static std::string GenerateRecordId();

        mutable std::mutex m_mutex;
        sqlite3* m_db{ nullptr };
        std::wstring m_dbPath;
        std::wstring m_folderPath;
        bool m_initialized{ false };
    };
}
