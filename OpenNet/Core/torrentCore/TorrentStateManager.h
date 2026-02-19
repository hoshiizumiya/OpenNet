#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <optional>

#include <libtorrent/fwd.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/session.hpp>

namespace OpenNet::Core::Torrent
{
    // Task metadata stored in SQLite
    struct TaskMetadata
    {
        std::string taskId;           // Unique task identifier
        std::string magnetUri;        // Original magnet URI
        std::string savePath;         // Download save path
        std::string name;             // Torrent name
        int64_t addedTimestamp{};     // When the task was added
        int64_t totalSize{};          // Total size in bytes
        int64_t downloadedSize{};     // Downloaded size in bytes
        int status{};                 // Task status: 0=pending, 1=downloading, 2=paused, 3=completed, 4=failed
        std::vector<uint8_t> resumeData; // libtorrent resume data blob
    };

    // Manages libtorrent session state and resume data persistence
    // Uses SQLite for metadata and WinUI3 LocalFolder for storage
    class TorrentStateManager
    {
    public:
        TorrentStateManager();
        ~TorrentStateManager();

        TorrentStateManager(TorrentStateManager const&) = delete;
        TorrentStateManager& operator=(TorrentStateManager const&) = delete;

        // Initialize the state manager (creates database, loads existing state)
        bool Initialize(std::wstring const& basePath = L"");

        // Get the storage base path (LocalFolder)
        std::wstring GetStoragePath() const;

        // Session state operations
        bool SaveSessionState(libtorrent::session& session);
        bool LoadSessionState(libtorrent::session& session);

        // Task resume data operations
        bool SaveTaskResumeData(std::string const& taskId, libtorrent::add_torrent_params const& params);
        std::optional<libtorrent::add_torrent_params> LoadTaskResumeData(std::string const& taskId);

        // Task metadata operations
        bool SaveTaskMetadata(TaskMetadata const& metadata);
        std::optional<TaskMetadata> LoadTaskMetadata(std::string const& taskId);
        std::vector<TaskMetadata> LoadAllTasks();
        bool DeleteTask(std::string const& taskId);
        bool UpdateTaskStatus(std::string const& taskId, int status);
        bool UpdateTaskProgress(std::string const& taskId, int64_t downloadedSize);

        // Import/Export functionality
        bool ExportToFile(std::wstring const& filePath);
        bool ImportFromFile(std::wstring const& filePath);

        // Generate a unique task ID
        static std::string GenerateTaskId();

    private:
        bool InitializeDatabase();
        bool CreateTables();
        bool CloseDatabase();

        std::wstring m_storagePath;
        std::wstring m_dbPath;
        void* m_db{ nullptr }; // sqlite3*
        std::mutex m_dbMutex;
        bool m_initialized{ false };

        // File paths
        static constexpr const wchar_t* DATABASE_FILENAME = L"torrent_state.db";
        static constexpr const wchar_t* SESSION_STATE_FILENAME = L"session_state.dat";
        static constexpr const char* RESUME_DATA_FOLDER = "resume_data";
    };

} // namespace OpenNet::Core::Torrent
