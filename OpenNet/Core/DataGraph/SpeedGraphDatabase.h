/*
 * PROJECT:   OpenNet
 * FILE:      Core/DataGraph/SpeedGraphDatabase.h
 * PURPOSE:   SQLite persistence for speed graph data points.
 *            Stores (taskId, percent, speedKB) rows so the graph
 *            can be reconstructed when a task is re-selected.
 *
 * SCHEMA:    speed_graph(task_id TEXT, percent INTEGER, speed_kb INTEGER,
 *                        PRIMARY KEY(task_id, percent))
 *
 * LICENSE:   The MIT License
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct sqlite3;

namespace OpenNet::Core
{
    /// A single speed data point
    struct SpeedPoint
    {
        int percent;        // 0-100
        uint64_t speedKB;   // download speed in KB/s
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

        /// Save a speed data point for a task (UPSERT by taskId+percent)
        void SavePoint(std::string const& taskId, int percent, uint64_t speedKB);

        /// Load all speed points for a task, sorted by percent ascending
        std::vector<SpeedPoint> LoadPoints(std::string const& taskId) const;

        /// Delete all speed data for a task
        void DeleteTask(std::string const& taskId);

    private:
        SpeedGraphDatabase() = default;
        ~SpeedGraphDatabase();
        SpeedGraphDatabase(SpeedGraphDatabase const&) = delete;
        SpeedGraphDatabase& operator=(SpeedGraphDatabase const&) = delete;

        void CreateTables();

        mutable std::mutex m_mutex;
        sqlite3* m_db{ nullptr };
        bool m_initialized{ false };
    };

} // namespace OpenNet::Core
