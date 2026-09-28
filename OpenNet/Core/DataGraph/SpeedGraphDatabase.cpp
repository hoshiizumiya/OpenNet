module;

/*
 * PROJECT:   OpenNet
 * FILE:      Core/DataGraph/SpeedGraphDatabase.cpp
 * PURPOSE:   SQLite persistence for speed graph data points.
 *
 * LICENSE:   The MIT License
 */

#include <Windows.h>
#include <sqlite3.h>

module OpenNet.Core.DataGraph.SpeedGraphDatabase;

import OpenNet.Core.IO.FileSystem;

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
			int rc = sqlite3_open16((std::filesystem::path(winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW()) / "speed_graph.db").c_str(), &m_db);
			if (rc != SQLITE_OK)
			{
				OutputDebugStringA(("SpeedGraphDatabase: Failed to open database: " +
									std::string(sqlite3_errmsg(m_db)) + "\n").c_str());
				return false;
			}

			// WAL mode for better concurrent read/write
			sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
			sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

			if (!CreateTables()) return false;
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
		m_lastSavedProgressPpm.clear();
		m_initialized = false;
	}

	bool SpeedGraphDatabase::CreateTables()
	{
		const char* sql = R"(
			CREATE TABLE IF NOT EXISTS speed_graph (
				task_id  TEXT    NOT NULL,
				percent  INTEGER NOT NULL,
				speed_kb INTEGER NOT NULL,
				PRIMARY KEY (task_id, percent)
			);
			CREATE TABLE IF NOT EXISTS speed_graph_v2 (
				task_id      TEXT    NOT NULL,
				progress_ppm INTEGER NOT NULL,
				speed_kb     INTEGER NOT NULL,
				PRIMARY KEY (task_id, progress_ppm)
			);
			INSERT OR IGNORE INTO speed_graph_v2 (task_id, progress_ppm, speed_kb)
				SELECT task_id, percent * 10000, speed_kb FROM speed_graph;
		)";

		char* errMsg = nullptr;
		int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
		if (rc != SQLITE_OK)
		{
			OutputDebugStringA(("SpeedGraphDatabase: CreateTables error: " +
								std::string(errMsg ? errMsg : "unknown") + "\n").c_str());
			sqlite3_free(errMsg);
			return false;
		}
		return true;
	}

	void SpeedGraphDatabase::SavePoint(std::string const& taskId, double percent, uint64_t speedKB)
	{
		if (taskId.empty() || !std::isfinite(percent) || !Initialize()) return;
		const auto progressPpm = static_cast<int>(
			std::round(std::clamp(percent, 0.0, 100.0) * 100.0)) * 100;

		std::lock_guard lk(m_mutex);
		if (!m_db) return;
		if (auto const last = m_lastSavedProgressPpm.find(taskId);
			last != m_lastSavedProgressPpm.end() && progressPpm <= last->second)
		{
			return;
		}

		const char* sql =
			"INSERT OR REPLACE INTO speed_graph_v2 (task_id, progress_ppm, speed_kb) VALUES (?, ?, ?);";
		sqlite3_stmt* stmt = nullptr;
		int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
		if (rc != SQLITE_OK)
		{
			OutputDebugStringA("SpeedGraphDatabase: SavePoint prepare failed\n");
			return;
		}
		sqlite3_bind_text(stmt, 1, taskId.c_str(), static_cast<int>(taskId.size()), SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, progressPpm);
		sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(speedKB));
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		if (rc == SQLITE_DONE)
			m_lastSavedProgressPpm.insert_or_assign(taskId, progressPpm);
	}

	std::vector<SpeedPoint> SpeedGraphDatabase::LoadPoints(std::string const& taskId)
	{
		std::vector<SpeedPoint> result;
		if (taskId.empty() || !Initialize()) return result;

		std::lock_guard lk(m_mutex);
		if (!m_db) return result;

		const char* sql =
			"SELECT progress_ppm, speed_kb FROM speed_graph_v2 WHERE task_id = ? ORDER BY progress_ppm ASC;";

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
			pt.percent = sqlite3_column_int(stmt, 0) / 10000.0;
			pt.speedKB = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
			result.push_back(pt);
		}

		sqlite3_finalize(stmt);
		// Limit replay work for long-running transfers without losing endpoints.
		constexpr std::size_t MaxGraphPoints = 1024;
		if (result.size() > MaxGraphPoints)
		{
			std::vector<SpeedPoint> sampled;
			sampled.reserve(MaxGraphPoints);
			for (std::size_t index = 0; index < MaxGraphPoints; ++index)
				sampled.push_back(result[index * (result.size() - 1)
					/ (MaxGraphPoints - 1)]);
			return sampled;
		}
		return result;
	}

	void SpeedGraphDatabase::DeleteTask(std::string const& taskId)
	{
		if (taskId.empty() || !Initialize()) return;

		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		const char* sql = "DELETE FROM speed_graph_v2 WHERE task_id = ?;";

		sqlite3_stmt* stmt = nullptr;
		int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
		if (rc != SQLITE_OK)
		{
			OutputDebugStringA("SpeedGraphDatabase: DeleteTask prepare failed\n");
			return;
		}

		sqlite3_bind_text(stmt, 1, taskId.c_str(), static_cast<int>(taskId.size()), SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		m_lastSavedProgressPpm.erase(taskId);
		// Old rows must also be removed or the startup migration restores them.
		if (sqlite3_prepare_v2(m_db,
			"DELETE FROM speed_graph WHERE task_id = ?;", -1, &stmt,
			nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, taskId.c_str(),
				static_cast<int>(taskId.size()), SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

} // namespace OpenNet::Core
