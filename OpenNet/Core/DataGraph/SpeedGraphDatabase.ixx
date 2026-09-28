/*
 * PROJECT:   OpenNet
 * FILE:      Core/DataGraph/SpeedGraphDatabase.ixx
 * PURPOSE:   SQLite persistence for speed graph data points.
 *            Stores (taskId, percent, speedKB) rows so the graph
 *            can be reconstructed when a task is re-selected.
 *
 * SCHEMA:    speed_graph_v2(task_id TEXT, progress_ppm INTEGER,
 *                           speed_kb INTEGER,
 *                           PRIMARY KEY(task_id, progress_ppm))
 *
 * LICENSE:   The MIT License
 */

module;
struct sqlite3;

export module OpenNet.Core.DataGraph.SpeedGraphDatabase;

export import std;

export namespace OpenNet::Core
{
    /// A single speed data point
    struct SpeedPoint
    {
        double percent;     // 0-100, retained at 0.01% precision
        std::uint64_t speedKB;   // download speed in KB/s
    };

    /// Thread-safe singleton that stores speed graph data in SQLite.
    class SpeedGraphDatabase
    {
    public:
        static SpeedGraphDatabase& Instance();

        /// Initialize the database (creates table if needed)
        bool Initialize();

        /// Close the database connection
        void Close();

        /// Save a speed data point for a task at 0.01% progress resolution.
        void SavePoint(std::string const& taskId, double percent, std::uint64_t speedKB);

        /// Load all speed points for a task, sorted by percent ascending
        std::vector<SpeedPoint> LoadPoints(std::string const& taskId);

        /// Delete all speed data for a task
        void DeleteTask(std::string const& taskId);

    private:
        SpeedGraphDatabase() = default;
        ~SpeedGraphDatabase();
        SpeedGraphDatabase(SpeedGraphDatabase const&) = delete;
        SpeedGraphDatabase& operator=(SpeedGraphDatabase const&) = delete;

        bool CreateTables();

        mutable std::mutex m_mutex;
        sqlite3* m_db{ nullptr };
        bool m_initialized{ false };
        std::unordered_map<std::string, int> m_lastSavedProgressPpm;
    };

} // namespace OpenNet::Core
