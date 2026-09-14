module;
#include "WindowsPlatform.h"
#include "LibtorrentIncludeGuard.h"
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/bdecode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/error_code.hpp>
#include "LibtorrentIncludeRestore.h"
#include <sqlite3.h>

module OpenNet.Core.torrentCore.TorrentStateManager;

import OpenNet.Core.IO.FileSystem;
import winrt.Windows.Foundation;
namespace lt = libtorrent;

namespace OpenNet::Core::Torrent
{
	namespace
	{
		bool EnsureColumn(sqlite3* database, char const* table, char const* column,
			char const* migration)
		{
			auto const query = std::string("PRAGMA table_info(") + table + ");";
			sqlite3_stmt* statement = nullptr;
			if (sqlite3_prepare_v2(database, query.c_str(), -1, &statement, nullptr) != SQLITE_OK)
				return false;
			bool exists = false;
			int result;
			while ((result = sqlite3_step(statement)) == SQLITE_ROW)
			{
				auto const name = reinterpret_cast<char const*>(sqlite3_column_text(statement, 1));
				if (name && std::string_view(name) == column) exists = true;
			}
			sqlite3_finalize(statement);
			if (result != SQLITE_DONE) return false;
			return exists || sqlite3_exec(database, migration, nullptr, nullptr, nullptr) == SQLITE_OK;
		}
	}

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
				// Use unified FileSystem path
				m_storagePath = winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW();
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
		sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
		sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
		sqlite3_exec(db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);

		if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK)
		{
			CloseDatabase();
			return false;
		}
		if (!CreateTables()
			|| sqlite3_exec(db, "PRAGMA user_version = 1;", nullptr, nullptr, nullptr) != SQLITE_OK
			|| sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
		{
			sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
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
				uploaded_size INTEGER DEFAULT 0,
				completed_timestamp INTEGER DEFAULT 0,
				updated_timestamp INTEGER DEFAULT 0,
				status INTEGER DEFAULT 0,
				info_hash_v1 TEXT DEFAULT '',
				info_hash_v2 TEXT DEFAULT '',
				error_message TEXT DEFAULT '',
				queue_position INTEGER NOT NULL DEFAULT -1,
				resume_data BLOB
			);
		)";

		const char* createSessionTable = R"(
			CREATE TABLE IF NOT EXISTS session_state (
				id INTEGER PRIMARY KEY CHECK (id = 1),
				state_data BLOB
			);
		)";

		const char* createTaskSettingsTable = R"(
			CREATE TABLE IF NOT EXISTS task_settings (
				task_id TEXT PRIMARY KEY REFERENCES tasks(task_id) ON DELETE CASCADE,
				download_limit INTEGER NOT NULL DEFAULT 0,
				upload_limit INTEGER NOT NULL DEFAULT 0,
				minimum_upload_rate INTEGER NOT NULL DEFAULT 0,
				max_connections INTEGER NOT NULL DEFAULT -1,
				max_uploads INTEGER NOT NULL DEFAULT -1,
				enable_dht INTEGER NOT NULL DEFAULT 1,
				enable_lsd INTEGER NOT NULL DEFAULT 1,
				enable_pex INTEGER NOT NULL DEFAULT 1,
				apply_ip_filter INTEGER NOT NULL DEFAULT 1,
				sequential_download INTEGER NOT NULL DEFAULT 0,
				super_seeding INTEGER NOT NULL DEFAULT 0,
				force_start INTEGER NOT NULL DEFAULT 0,
				upload_mode INTEGER NOT NULL DEFAULT 0,
				share_mode INTEGER NOT NULL DEFAULT 0,
				share_ratio_limit REAL NOT NULL DEFAULT -2,
				seeding_time_limit INTEGER NOT NULL DEFAULT -2,
				inactive_seeding_time_limit INTEGER NOT NULL DEFAULT -2,
				share_limit_match_all INTEGER NOT NULL DEFAULT 0,
				share_limit_action INTEGER NOT NULL DEFAULT -1,
				completion_action INTEGER NOT NULL DEFAULT -1
			);
		)";

		const char* createHashAliasesTable = R"(
			CREATE TABLE IF NOT EXISTS torrent_hash_aliases (
				task_id TEXT NOT NULL REFERENCES tasks(task_id) ON DELETE CASCADE,
				version INTEGER NOT NULL CHECK(version IN (1, 2)),
				hash TEXT NOT NULL COLLATE NOCASE,
				PRIMARY KEY(version, hash),
				UNIQUE(task_id, version, hash)
			);
		)";

		const char* createMigrationConflictsTable = R"(
			CREATE TABLE IF NOT EXISTS torrent_migration_conflicts (
				id INTEGER PRIMARY KEY AUTOINCREMENT,
				task_id TEXT NOT NULL,
				conflicting_task_id TEXT NOT NULL,
				version INTEGER NOT NULL,
				hash TEXT NOT NULL,
				detected_timestamp INTEGER NOT NULL,
				UNIQUE(task_id, conflicting_task_id, version, hash)
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

		// Idempotent migrations for databases created by earlier OpenNet builds.
		auto* database = static_cast<sqlite3*>(m_db);
		if (!EnsureColumn(database, "tasks", "uploaded_size",
			"ALTER TABLE tasks ADD COLUMN uploaded_size INTEGER DEFAULT 0;")) return false;
		if (!EnsureColumn(database, "tasks", "completed_timestamp",
			"ALTER TABLE tasks ADD COLUMN completed_timestamp INTEGER DEFAULT 0;")) return false;
		if (!EnsureColumn(database, "tasks", "updated_timestamp",
			"ALTER TABLE tasks ADD COLUMN updated_timestamp INTEGER DEFAULT 0;")) return false;
		if (!EnsureColumn(database, "tasks", "info_hash_v1",
			"ALTER TABLE tasks ADD COLUMN info_hash_v1 TEXT DEFAULT '';")) return false;
		if (!EnsureColumn(database, "tasks", "info_hash_v2",
			"ALTER TABLE tasks ADD COLUMN info_hash_v2 TEXT DEFAULT '';")) return false;
		if (!EnsureColumn(database, "tasks", "error_message",
			"ALTER TABLE tasks ADD COLUMN error_message TEXT DEFAULT '';")) return false;
		if (!EnsureColumn(database, "tasks", "queue_position",
			"ALTER TABLE tasks ADD COLUMN queue_position INTEGER NOT NULL DEFAULT -1;")) return false;
		rc = sqlite3_exec(database, createTaskSettingsTable, nullptr, nullptr, &errMsg);
		if (rc != SQLITE_OK)
		{
			if (errMsg)
			{
				OutputDebugStringA(("SQLite error creating task_settings table: " + std::string(errMsg) + "\n").c_str());
				sqlite3_free(errMsg);
			}
			return false;
		}
		if (!EnsureColumn(database, "task_settings", "share_ratio_limit",
			"ALTER TABLE task_settings ADD COLUMN share_ratio_limit REAL NOT NULL DEFAULT -2;")) return false;
		if (!EnsureColumn(database, "task_settings", "seeding_time_limit",
			"ALTER TABLE task_settings ADD COLUMN seeding_time_limit INTEGER NOT NULL DEFAULT -2;")) return false;
		if (!EnsureColumn(database, "task_settings", "inactive_seeding_time_limit",
			"ALTER TABLE task_settings ADD COLUMN inactive_seeding_time_limit INTEGER NOT NULL DEFAULT -2;")) return false;
		if (!EnsureColumn(database, "task_settings", "share_limit_match_all",
			"ALTER TABLE task_settings ADD COLUMN share_limit_match_all INTEGER NOT NULL DEFAULT 0;")) return false;
		if (!EnsureColumn(database, "task_settings", "share_limit_action",
			"ALTER TABLE task_settings ADD COLUMN share_limit_action INTEGER NOT NULL DEFAULT -1;")) return false;
		if (!EnsureColumn(database, "task_settings", "completion_action",
			"ALTER TABLE task_settings ADD COLUMN completion_action INTEGER NOT NULL DEFAULT -1;")) return false;
		rc = sqlite3_exec(database, createHashAliasesTable, nullptr, nullptr, &errMsg);
		if (rc != SQLITE_OK)
		{
			if (errMsg)
			{
				OutputDebugStringA(("SQLite error creating torrent_hash_aliases table: " + std::string(errMsg) + "\n").c_str());
				sqlite3_free(errMsg);
				errMsg = nullptr;
			}
			return false;
		}
		rc = sqlite3_exec(database, createMigrationConflictsTable, nullptr, nullptr, &errMsg);
		if (rc != SQLITE_OK)
		{
			if (errMsg)
			{
				OutputDebugStringA(("SQLite error creating torrent_migration_conflicts table: " + std::string(errMsg) + "\n").c_str());
				sqlite3_free(errMsg);
				errMsg = nullptr;
			}
			return false;
		}
		sqlite3_exec(database, "CREATE INDEX IF NOT EXISTS idx_tasks_status_updated ON tasks(status, updated_timestamp DESC);", nullptr, nullptr, nullptr);
		for (auto const* const indexSql : {
			"CREATE UNIQUE INDEX IF NOT EXISTS idx_tasks_info_hash_v1 ON tasks(info_hash_v1 COLLATE NOCASE) WHERE info_hash_v1 <> '';",
				"CREATE UNIQUE INDEX IF NOT EXISTS idx_tasks_info_hash_v2 ON tasks(info_hash_v2 COLLATE NOCASE) WHERE info_hash_v2 <> '';" })
		{
			char* indexError = nullptr;
			if (sqlite3_exec(database, indexSql, nullptr, nullptr, &indexError) != SQLITE_OK)
			{
				OutputDebugStringA(("SQLite hash index migration requires conflict review: "
									+ std::string(indexError ? indexError : "unknown error") + "\n").c_str());
				sqlite3_free(indexError);
			}
		}

		// Record legacy duplicate identities before installing the canonical
		// alias rows. No task or resume blob is deleted automatically.
		auto const detectedAt = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		for (auto const& [version, column] : {
			std::pair{ 1, "info_hash_v1" },
			std::pair{ 2, "info_hash_v2" } })
		{
			auto const conflictSql = std::format(
				"INSERT OR IGNORE INTO torrent_migration_conflicts "
				"(task_id, conflicting_task_id, version, hash, detected_timestamp) "
				"SELECT later.task_id, earlier.task_id, {}, lower(later.{}), {} "
				"FROM tasks AS later JOIN tasks AS earlier "
				"ON lower(later.{}) = lower(earlier.{}) "
				"AND later.{} <> '' AND later.task_id > earlier.task_id;",
				version, column, detectedAt, column, column, column);
			sqlite3_exec(database, conflictSql.c_str(), nullptr, nullptr, nullptr);

			auto const aliasSql = std::format(
				"INSERT OR IGNORE INTO torrent_hash_aliases(task_id, version, hash) "
				"SELECT task_id, {}, lower({}) FROM tasks "
				"WHERE {} <> '' ORDER BY added_timestamp ASC, task_id ASC;",
				version, column, column);
			if (sqlite3_exec(database, aliasSql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK)
			{
				if (errMsg)
				{
					OutputDebugStringA(("SQLite error backfilling torrent hash aliases: " + std::string(errMsg) + "\n").c_str());
					sqlite3_free(errMsg);
					errMsg = nullptr;
				}
				return false;
			}
		}

		// Legacy duplicates retain their resume blobs for explicit conflict review.

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

	bool TorrentStateManager::SaveSessionState(
		std::vector<std::uint8_t> const& stateData)
	{
		std::lock_guard lk(m_dbMutex);
		if (!m_db) return false;

		try
		{
			const char* sql = R"(
				INSERT OR REPLACE INTO session_state (id, state_data) VALUES (1, ?);
			)";

			sqlite3_stmt* stmt = nullptr;
			int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
			if (rc != SQLITE_OK) return false;

			sqlite3_bind_blob(stmt, 1, stateData.data(),
							  static_cast<int>(stateData.size()), SQLITE_TRANSIENT);

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

	std::optional<std::vector<std::uint8_t>>
		TorrentStateManager::LoadSessionStateData()
	{
		std::lock_guard lk(m_dbMutex);
		if (!m_db) return std::nullopt;

		try
		{
			const char* sql = "SELECT state_data FROM session_state WHERE id = 1;";
			sqlite3_stmt* stmt = nullptr;
			int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
			if (rc != SQLITE_OK) return std::nullopt;

			rc = sqlite3_step(stmt);
			if (rc == SQLITE_ROW)
			{
				const void* data = sqlite3_column_blob(stmt, 0);
				int size = sqlite3_column_bytes(stmt, 0);

				if (data && size > 0)
				{
					std::vector<std::uint8_t> result(
						static_cast<std::uint8_t const*>(data),
						static_cast<std::uint8_t const*>(data) + size);
					sqlite3_finalize(stmt);
					return result;
				}
			}

			sqlite3_finalize(stmt);
			return std::nullopt;
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("LoadSessionStateData error: " + std::string(ex.what()) + "\n").c_str());
			return std::nullopt;
		}
	}

	bool TorrentStateManager::SaveTaskResumeData(
		std::string const& taskId,
		std::vector<std::uint8_t> const& resumeData)
	{
		std::lock_guard lk(m_dbMutex);
		if (!m_db) return false;

		try
		{
			const char* sql = "UPDATE tasks SET resume_data = ?, updated_timestamp = ? WHERE task_id = ?;";
			sqlite3_stmt* stmt = nullptr;
			int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
			if (rc != SQLITE_OK) return false;

			sqlite3_bind_blob(stmt, 1, resumeData.data(),
							  static_cast<int>(resumeData.size()), SQLITE_TRANSIENT);
			auto const updatedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			sqlite3_bind_int64(stmt, 2, updatedTimestamp);
			sqlite3_bind_text(stmt, 3, taskId.c_str(), -1, SQLITE_TRANSIENT);

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

	std::optional<std::vector<std::uint8_t>>
		TorrentStateManager::LoadTaskResumeData(std::string const& taskId)
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
					std::vector<std::uint8_t> result(
						static_cast<std::uint8_t const*>(data),
						static_cast<std::uint8_t const*>(data) + size);
					sqlite3_finalize(stmt);
					return result;
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
		if (!m_db || metadata.taskId.empty()) return false;

		try
		{
			auto* const database = static_cast<sqlite3*>(m_db);
			if (sqlite3_exec(database, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr)
				!= SQLITE_OK)
				return false;
			auto rollback = [database]()
			{
				sqlite3_exec(database, "ROLLBACK;", nullptr, nullptr, nullptr);
			};

			const char* sql = R"(
				INSERT INTO tasks (
					task_id, magnet_uri, save_path, name, added_timestamp,
					total_size, downloaded_size, uploaded_size, completed_timestamp,
					updated_timestamp, status, info_hash_v1, info_hash_v2,
					error_message, queue_position, resume_data)
				VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
				ON CONFLICT(task_id) DO UPDATE SET
					magnet_uri = excluded.magnet_uri,
					save_path = excluded.save_path,
					name = excluded.name,
					total_size = excluded.total_size,
					downloaded_size = excluded.downloaded_size,
					uploaded_size = excluded.uploaded_size,
					completed_timestamp = excluded.completed_timestamp,
					updated_timestamp = excluded.updated_timestamp,
					status = excluded.status,
					info_hash_v1 = excluded.info_hash_v1,
					info_hash_v2 = excluded.info_hash_v2,
					error_message = excluded.error_message,
					queue_position = CASE
						WHEN excluded.queue_position >= 0 THEN excluded.queue_position
						ELSE tasks.queue_position END,
					resume_data = COALESCE(excluded.resume_data, tasks.resume_data);
			)";

			sqlite3_stmt* stmt = nullptr;
			int rc = sqlite3_prepare_v2(database, sql, -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				rollback();
				return false;
			}

			sqlite3_bind_text(stmt, 1, metadata.taskId.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, metadata.magnetUri.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, metadata.savePath.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, metadata.name.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 5, metadata.addedTimestamp);
			sqlite3_bind_int64(stmt, 6, metadata.totalSize);
			sqlite3_bind_int64(stmt, 7, metadata.downloadedSize);
			sqlite3_bind_int64(stmt, 8, metadata.uploadedSize);
			sqlite3_bind_int64(stmt, 9, metadata.completedTimestamp);
			auto const updatedTimestamp = metadata.updatedTimestamp > 0 ? metadata.updatedTimestamp : std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			sqlite3_bind_int64(stmt, 10, updatedTimestamp);
			sqlite3_bind_int(stmt, 11, metadata.status);
			sqlite3_bind_text(stmt, 12, metadata.infoHashV1.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 13, metadata.infoHashV2.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 14, metadata.errorMessage.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 15, metadata.queuePosition);

			if (metadata.resumeData.empty())
			{
				sqlite3_bind_null(stmt, 16);
			}
			else
			{
				sqlite3_bind_blob(stmt, 16, metadata.resumeData.data(),
								  static_cast<int>(metadata.resumeData.size()), SQLITE_TRANSIENT);
			}

			rc = sqlite3_step(stmt);
			sqlite3_finalize(stmt);
			if (rc != SQLITE_DONE)
			{
				rollback();
				return false;
			}

			sqlite3_stmt* deleteAliases = nullptr;
			if (sqlite3_prepare_v2(
				database,
				"DELETE FROM torrent_hash_aliases WHERE task_id = ?;",
				-1, &deleteAliases, nullptr) != SQLITE_OK)
			{
				rollback();
				return false;
			}
			sqlite3_bind_text(deleteAliases, 1, metadata.taskId.c_str(), -1, SQLITE_TRANSIENT);
			rc = sqlite3_step(deleteAliases);
			sqlite3_finalize(deleteAliases);
			if (rc != SQLITE_DONE)
			{
				rollback();
				return false;
			}

			auto insertAlias = [&](int const version, std::string const& hash)
			{
				if (hash.empty()) return true;
				sqlite3_stmt* statement = nullptr;
				if (sqlite3_prepare_v2(
					database,
					"INSERT INTO torrent_hash_aliases(task_id, version, hash) VALUES (?, ?, lower(?));",
					-1, &statement, nullptr) != SQLITE_OK)
					return false;
				sqlite3_bind_text(statement, 1, metadata.taskId.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(statement, 2, version);
				sqlite3_bind_text(statement, 3, hash.c_str(), -1, SQLITE_TRANSIENT);
				auto const succeeded = sqlite3_step(statement) == SQLITE_DONE;
				sqlite3_finalize(statement);
				return succeeded;
			};
			if (!insertAlias(1, metadata.infoHashV1)
				|| !insertAlias(2, metadata.infoHashV2))
			{
				rollback();
				return false;
			}

			if (sqlite3_exec(database, "COMMIT;", nullptr, nullptr, nullptr)
				!= SQLITE_OK)
			{
				rollback();
				return false;
			}
			return true;
		}
		catch (std::exception const& ex)
		{
			sqlite3_exec(static_cast<sqlite3*>(m_db), "ROLLBACK;", nullptr, nullptr, nullptr);
			OutputDebugStringA(("SaveTaskMetadata error: " + std::string(ex.what()) + "\n").c_str());
			return false;
		}
	}

	std::optional<std::string> TorrentStateManager::FindTaskIdByInfoHashes(
		std::string const& infoHashV1,
		std::string const& infoHashV2)
	{
		std::lock_guard lock(m_dbMutex);
		if (!m_db || (infoHashV1.empty() && infoHashV2.empty()))
			return std::nullopt;

		constexpr char query[] = R"(
			SELECT task_id FROM torrent_hash_aliases
			WHERE (version = 1 AND hash = lower(?))
			   OR (version = 2 AND hash = lower(?))
			ORDER BY CASE WHEN version = 1 THEN 0 ELSE 1 END
			LIMIT 1;
		)";
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(
			static_cast<sqlite3*>(m_db), query, -1, &statement, nullptr)
			!= SQLITE_OK)
			return std::nullopt;
		sqlite3_bind_text(statement, 1, infoHashV1.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(statement, 2, infoHashV2.c_str(), -1, SQLITE_TRANSIENT);
		std::optional<std::string> result;
		if (sqlite3_step(statement) == SQLITE_ROW)
		{
			auto const value = sqlite3_column_text(statement, 0);
			if (value)
				result = reinterpret_cast<char const*>(value);
		}
		sqlite3_finalize(statement);
		return result;
	}

	std::optional<TaskMetadata> TorrentStateManager::LoadTaskMetadata(std::string const& taskId)
	{
		std::lock_guard lk(m_dbMutex);
		if (!m_db) return std::nullopt;

		try
		{
			const char* sql = R"(
				SELECT task_id, magnet_uri, save_path, name, added_timestamp,
					   total_size, downloaded_size, uploaded_size, completed_timestamp,
					   updated_timestamp, status, info_hash_v1, info_hash_v2,
					   error_message, queue_position, resume_data
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
				auto col0 = sqlite3_column_text(stmt, 0);
				auto col1 = sqlite3_column_text(stmt, 1);
				auto col2 = sqlite3_column_text(stmt, 2);
				metadata.taskId = col0 ? reinterpret_cast<const char*>(col0) : "";
				metadata.magnetUri = col1 ? reinterpret_cast<const char*>(col1) : "";
				metadata.savePath = col2 ? reinterpret_cast<const char*>(col2) : "";

				auto namePtr = sqlite3_column_text(stmt, 3);
				metadata.name = namePtr ? reinterpret_cast<const char*>(namePtr) : "";

				metadata.addedTimestamp = sqlite3_column_int64(stmt, 4);
				metadata.totalSize = sqlite3_column_int64(stmt, 5);
				metadata.downloadedSize = sqlite3_column_int64(stmt, 6);
				metadata.uploadedSize = sqlite3_column_int64(stmt, 7);
				metadata.completedTimestamp = sqlite3_column_int64(stmt, 8);
				metadata.updatedTimestamp = sqlite3_column_int64(stmt, 9);
				metadata.status = sqlite3_column_int(stmt, 10);
				auto const hashV1 = sqlite3_column_text(stmt, 11);
				auto const hashV2 = sqlite3_column_text(stmt, 12);
				auto const errorText = sqlite3_column_text(stmt, 13);
				metadata.infoHashV1 = hashV1 ? reinterpret_cast<char const*>(hashV1) : "";
				metadata.infoHashV2 = hashV2 ? reinterpret_cast<char const*>(hashV2) : "";
				metadata.errorMessage = errorText ? reinterpret_cast<char const*>(errorText) : "";
				metadata.queuePosition = sqlite3_column_int(stmt, 14);

				const void* blobData = sqlite3_column_blob(stmt, 15);
				int blobSize = sqlite3_column_bytes(stmt, 15);
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
					   total_size, downloaded_size, uploaded_size, completed_timestamp,
					   updated_timestamp, status, info_hash_v1, info_hash_v2,
					   error_message, queue_position, resume_data
				FROM tasks ORDER BY added_timestamp DESC;
			)";

			sqlite3_stmt* stmt = nullptr;
			int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
			if (rc != SQLITE_OK) return tasks;

			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				TaskMetadata metadata;
				auto col0 = sqlite3_column_text(stmt, 0);
				auto col1 = sqlite3_column_text(stmt, 1);
				auto col2 = sqlite3_column_text(stmt, 2);
				metadata.taskId = col0 ? reinterpret_cast<const char*>(col0) : "";
				metadata.magnetUri = col1 ? reinterpret_cast<const char*>(col1) : "";
				metadata.savePath = col2 ? reinterpret_cast<const char*>(col2) : "";

				auto namePtr = sqlite3_column_text(stmt, 3);
				metadata.name = namePtr ? reinterpret_cast<const char*>(namePtr) : "";

				metadata.addedTimestamp = sqlite3_column_int64(stmt, 4);
				metadata.totalSize = sqlite3_column_int64(stmt, 5);
				metadata.downloadedSize = sqlite3_column_int64(stmt, 6);
				metadata.uploadedSize = sqlite3_column_int64(stmt, 7);
				metadata.completedTimestamp = sqlite3_column_int64(stmt, 8);
				metadata.updatedTimestamp = sqlite3_column_int64(stmt, 9);
				metadata.status = sqlite3_column_int(stmt, 10);
				auto const hashV1 = sqlite3_column_text(stmt, 11);
				auto const hashV2 = sqlite3_column_text(stmt, 12);
				auto const errorText = sqlite3_column_text(stmt, 13);
				metadata.infoHashV1 = hashV1 ? reinterpret_cast<char const*>(hashV1) : "";
				metadata.infoHashV2 = hashV2 ? reinterpret_cast<char const*>(hashV2) : "";
				metadata.errorMessage = errorText ? reinterpret_cast<char const*>(errorText) : "";
				metadata.queuePosition = sqlite3_column_int(stmt, 14);

				const void* blobData = sqlite3_column_blob(stmt, 15);
				int blobSize = sqlite3_column_bytes(stmt, 15);
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

	bool TorrentStateManager::SaveTaskSettings(TaskSettingsMetadata const& settings)
	{
		std::lock_guard lock(m_dbMutex);
		if (!m_db || settings.taskId.empty()) return false;
		constexpr char sql[] = R"(
			INSERT INTO task_settings (
				task_id, download_limit, upload_limit, minimum_upload_rate,
				max_connections, max_uploads, enable_dht, enable_lsd, enable_pex,
				apply_ip_filter, sequential_download, super_seeding, force_start,
				upload_mode, share_mode, share_ratio_limit, seeding_time_limit,
				inactive_seeding_time_limit, share_limit_match_all,
				share_limit_action, completion_action)
			VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
			ON CONFLICT(task_id) DO UPDATE SET
				download_limit = excluded.download_limit,
				upload_limit = excluded.upload_limit,
				minimum_upload_rate = excluded.minimum_upload_rate,
				max_connections = excluded.max_connections,
				max_uploads = excluded.max_uploads,
				enable_dht = excluded.enable_dht,
				enable_lsd = excluded.enable_lsd,
				enable_pex = excluded.enable_pex,
				apply_ip_filter = excluded.apply_ip_filter,
				sequential_download = excluded.sequential_download,
				super_seeding = excluded.super_seeding,
				force_start = excluded.force_start,
				upload_mode = excluded.upload_mode,
				share_mode = excluded.share_mode,
				share_ratio_limit = excluded.share_ratio_limit,
				seeding_time_limit = excluded.seeding_time_limit,
				inactive_seeding_time_limit = excluded.inactive_seeding_time_limit,
				share_limit_match_all = excluded.share_limit_match_all,
				share_limit_action = excluded.share_limit_action,
				completion_action = excluded.completion_action;
		)";
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &statement, nullptr) != SQLITE_OK) return false;
		sqlite3_bind_text(statement, 1, settings.taskId.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(statement, 2, settings.downloadLimit);
		sqlite3_bind_int(statement, 3, settings.uploadLimit);
		sqlite3_bind_int(statement, 4, settings.minimumUploadRate);
		sqlite3_bind_int(statement, 5, settings.maxConnections);
		sqlite3_bind_int(statement, 6, settings.maxUploads);
		sqlite3_bind_int(statement, 7, settings.enableDht);
		sqlite3_bind_int(statement, 8, settings.enableLsd);
		sqlite3_bind_int(statement, 9, settings.enablePex);
		sqlite3_bind_int(statement, 10, settings.applyIpFilter);
		sqlite3_bind_int(statement, 11, settings.sequentialDownload);
		sqlite3_bind_int(statement, 12, settings.superSeeding);
		sqlite3_bind_int(statement, 13, settings.forceStart);
		sqlite3_bind_int(statement, 14, settings.uploadMode);
		sqlite3_bind_int(statement, 15, settings.shareMode);
		sqlite3_bind_double(statement, 16, settings.shareRatioLimit);
		sqlite3_bind_int(statement, 17, settings.seedingTimeLimit);
		sqlite3_bind_int(statement, 18, settings.inactiveSeedingTimeLimit);
		sqlite3_bind_int(statement, 19, settings.shareLimitMatchAll);
		sqlite3_bind_int(statement, 20, settings.shareLimitAction);
		sqlite3_bind_int(statement, 21, settings.completionAction);
		auto const result = sqlite3_step(statement) == SQLITE_DONE;
		sqlite3_finalize(statement);
		return result;
	}

	std::optional<TaskSettingsMetadata> TorrentStateManager::LoadTaskSettings(std::string const& taskId)
	{
		std::lock_guard lock(m_dbMutex);
		if (!m_db || taskId.empty()) return std::nullopt;
		constexpr char sql[] = R"(
			SELECT download_limit, upload_limit, minimum_upload_rate,
				   max_connections, max_uploads, enable_dht, enable_lsd, enable_pex,
				   apply_ip_filter, sequential_download, super_seeding, force_start,
				   upload_mode, share_mode, share_ratio_limit,
				   seeding_time_limit, inactive_seeding_time_limit,
				   share_limit_match_all, share_limit_action, completion_action
			FROM task_settings WHERE task_id = ?;
		)";
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &statement, nullptr) != SQLITE_OK) return std::nullopt;
		sqlite3_bind_text(statement, 1, taskId.c_str(), -1, SQLITE_TRANSIENT);
		if (sqlite3_step(statement) != SQLITE_ROW)
		{
			sqlite3_finalize(statement);
			return std::nullopt;
		}
		TaskSettingsMetadata settings;
		settings.taskId = taskId;
		settings.downloadLimit = sqlite3_column_int(statement, 0);
		settings.uploadLimit = sqlite3_column_int(statement, 1);
		settings.minimumUploadRate = sqlite3_column_int(statement, 2);
		settings.maxConnections = sqlite3_column_int(statement, 3);
		settings.maxUploads = sqlite3_column_int(statement, 4);
		settings.enableDht = sqlite3_column_int(statement, 5) != 0;
		settings.enableLsd = sqlite3_column_int(statement, 6) != 0;
		settings.enablePex = sqlite3_column_int(statement, 7) != 0;
		settings.applyIpFilter = sqlite3_column_int(statement, 8) != 0;
		settings.sequentialDownload = sqlite3_column_int(statement, 9) != 0;
		settings.superSeeding = sqlite3_column_int(statement, 10) != 0;
		settings.forceStart = sqlite3_column_int(statement, 11) != 0;
		settings.uploadMode = sqlite3_column_int(statement, 12) != 0;
		settings.shareMode = sqlite3_column_int(statement, 13) != 0;
		settings.shareRatioLimit = sqlite3_column_double(statement, 14);
		settings.seedingTimeLimit = sqlite3_column_int(statement, 15);
		settings.inactiveSeedingTimeLimit = sqlite3_column_int(statement, 16);
		settings.shareLimitMatchAll = sqlite3_column_int(statement, 17) != 0;
		settings.shareLimitAction = sqlite3_column_int(statement, 18);
		settings.completionAction = sqlite3_column_int(statement, 19);
		sqlite3_finalize(statement);
		return settings;
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
			const char* sql = "UPDATE tasks SET status = ?, updated_timestamp = ? WHERE task_id = ?;";
			sqlite3_stmt* stmt = nullptr;
			int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
			if (rc != SQLITE_OK) return false;

			sqlite3_bind_int(stmt, 1, status);
			auto const updatedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			sqlite3_bind_int64(stmt, 2, updatedTimestamp);
			sqlite3_bind_text(stmt, 3, taskId.c_str(), -1, SQLITE_TRANSIENT);

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

	bool TorrentStateManager::UpdateTaskProgress(std::string const& taskId, std::int64_t const downloadedSize, std::int64_t const uploadedSize, std::int64_t const completedTimestamp)
	{
		std::lock_guard lk(m_dbMutex);
		if (!m_db) return false;

		try
		{
			const char* sql = "UPDATE tasks SET downloaded_size = ?, uploaded_size = ?, completed_timestamp = CASE WHEN ? > 0 THEN ? ELSE completed_timestamp END, updated_timestamp = ? WHERE task_id = ?;";
			sqlite3_stmt* stmt = nullptr;
			int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
			if (rc != SQLITE_OK) return false;

			sqlite3_bind_int64(stmt, 1, downloadedSize);
			sqlite3_bind_int64(stmt, 2, uploadedSize);
			sqlite3_bind_int64(stmt, 3, completedTimestamp);
			sqlite3_bind_int64(stmt, 4, completedTimestamp);
			auto const updatedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			sqlite3_bind_int64(stmt, 5, updatedTimestamp);
			sqlite3_bind_text(stmt, 6, taskId.c_str(), -1, SQLITE_TRANSIENT);

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

	bool TorrentStateManager::UpdateTaskName(std::string const& taskId, std::string const& name)
	{
		std::lock_guard lk(m_dbMutex);
		if (!m_db) return false;

		try
		{
			const char* sql = "UPDATE tasks SET name = ?, updated_timestamp = ? WHERE task_id = ?;";
			sqlite3_stmt* stmt = nullptr;
			int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
			if (rc != SQLITE_OK) return false;

			sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
			auto const updatedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			sqlite3_bind_int64(stmt, 2, updatedTimestamp);
			sqlite3_bind_text(stmt, 3, taskId.c_str(), -1, SQLITE_TRANSIENT);

			rc = sqlite3_step(stmt);
			sqlite3_finalize(stmt);

			return rc == SQLITE_DONE;
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("UpdateTaskName error: " + std::string(ex.what()) + "\n").c_str());
			return false;
		}
	}

	bool TorrentStateManager::UpdateTaskSavePath(std::string const& taskId, std::string const& savePath)
	{
		std::lock_guard lk(m_dbMutex);
		if (!m_db) return false;

		try
		{
			constexpr char sql[] = "UPDATE tasks SET save_path = ?, updated_timestamp = ? WHERE task_id = ?;";
			sqlite3_stmt* statement = nullptr;
			int result = sqlite3_prepare_v2(
				static_cast<sqlite3*>(m_db), sql, -1, &statement, nullptr);
			if (result != SQLITE_OK) return false;

			sqlite3_bind_text(
				statement, 1, savePath.c_str(), -1, SQLITE_TRANSIENT);
			auto const updatedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			sqlite3_bind_int64(statement, 2, updatedTimestamp);
			sqlite3_bind_text(
				statement, 3, taskId.c_str(), -1, SQLITE_TRANSIENT);
			result = sqlite3_step(statement);
			sqlite3_finalize(statement);
			return result == SQLITE_DONE;
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA((
				"UpdateTaskSavePath error: " + std::string(ex.what()) + "\n").c_str());
			return false;
		}
	}

	bool TorrentStateManager::UpdateTaskQueuePosition(std::string const& taskId, int const queuePosition)
	{
		std::lock_guard lock(m_dbMutex);
		if (!m_db || taskId.empty()) return false;
		constexpr char sql[] = R"(
			UPDATE tasks
			SET queue_position = ?, updated_timestamp = ?
			WHERE task_id = ?;
		)";
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(
			static_cast<sqlite3*>(m_db), sql, -1, &statement, nullptr)
			!= SQLITE_OK)
			return false;
		sqlite3_bind_int(statement, 1, queuePosition);
		sqlite3_bind_int64(statement, 2,
						   std::chrono::duration_cast<std::chrono::seconds>(
							   std::chrono::system_clock::now().time_since_epoch()).count());
		sqlite3_bind_text(statement, 3, taskId.c_str(), -1, SQLITE_TRANSIENT);
		auto const result = sqlite3_step(statement) == SQLITE_DONE;
		sqlite3_finalize(statement);
		return result;
	}

	bool TorrentStateManager::ExportToFile(std::wstring const& filePath)
	{
		// Don't hold lock while doing I/O - load data first, then write
		std::vector<TaskMetadata> tasks;

		// Load all tasks with lock held
		{
			std::lock_guard lk(m_dbMutex);
			if (!m_db) return false;

			// Inline query to avoid recursive locking
			const char* sql = R"(
				SELECT task_id, magnet_uri, save_path, name, added_timestamp,
					   total_size, downloaded_size, uploaded_size, completed_timestamp,
					   updated_timestamp, status, info_hash_v1, info_hash_v2,
					   error_message, queue_position, resume_data
				FROM tasks ORDER BY added_timestamp DESC;
			)";

			sqlite3_stmt* stmt = nullptr;
			int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr);
			if (rc != SQLITE_OK) return false;

			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				TaskMetadata metadata;
				auto col0 = sqlite3_column_text(stmt, 0);
				auto col1 = sqlite3_column_text(stmt, 1);
				auto col2 = sqlite3_column_text(stmt, 2);
				metadata.taskId = col0 ? reinterpret_cast<const char*>(col0) : "";
				metadata.magnetUri = col1 ? reinterpret_cast<const char*>(col1) : "";
				metadata.savePath = col2 ? reinterpret_cast<const char*>(col2) : "";

				auto namePtr = sqlite3_column_text(stmt, 3);
				metadata.name = namePtr ? reinterpret_cast<const char*>(namePtr) : "";

				metadata.addedTimestamp = sqlite3_column_int64(stmt, 4);
				metadata.totalSize = sqlite3_column_int64(stmt, 5);
				metadata.downloadedSize = sqlite3_column_int64(stmt, 6);
				metadata.uploadedSize = sqlite3_column_int64(stmt, 7);
				metadata.completedTimestamp = sqlite3_column_int64(stmt, 8);
				metadata.updatedTimestamp = sqlite3_column_int64(stmt, 9);
				metadata.status = sqlite3_column_int(stmt, 10);
				auto const hashV1 = sqlite3_column_text(stmt, 11);
				auto const hashV2 = sqlite3_column_text(stmt, 12);
				auto const errorText = sqlite3_column_text(stmt, 13);
				metadata.infoHashV1 = hashV1 ? reinterpret_cast<char const*>(hashV1) : "";
				metadata.infoHashV2 = hashV2 ? reinterpret_cast<char const*>(hashV2) : "";
				metadata.errorMessage = errorText ? reinterpret_cast<char const*>(errorText) : "";
				metadata.queuePosition = sqlite3_column_int(stmt, 14);

				const void* blobData = sqlite3_column_blob(stmt, 15);
				int blobSize = sqlite3_column_bytes(stmt, 15);
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

		// Now write to file without lock
		try
		{
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
				taskEntry["uploaded_size"] = task.uploadedSize;
				taskEntry["completed_timestamp"] = task.completedTimestamp;
				taskEntry["updated_timestamp"] = task.updatedTimestamp;
				taskEntry["status"] = task.status;
				taskEntry["info_hash_v1"] = task.infoHashV1;
				taskEntry["info_hash_v2"] = task.infoHashV2;
				taskEntry["error_message"] = task.errorMessage;
				taskEntry["queue_position"] = task.queuePosition;

				if (!task.resumeData.empty())
				{
					taskEntry["resume_data"] = std::string(
						reinterpret_cast<const char*>(task.resumeData.data()),
						task.resumeData.size()
					);
				}
				if (auto const settings = LoadTaskSettings(task.taskId))
				{
					auto& values = taskEntry["task_settings"];
					values["download_limit"] = settings->downloadLimit;
					values["upload_limit"] = settings->uploadLimit;
					values["minimum_upload_rate"] = settings->minimumUploadRate;
					values["max_connections"] = settings->maxConnections;
					values["max_uploads"] = settings->maxUploads;
					values["enable_dht"] = settings->enableDht;
					values["enable_lsd"] = settings->enableLsd;
					values["enable_pex"] = settings->enablePex;
					values["apply_ip_filter"] = settings->applyIpFilter;
					values["sequential_download"] = settings->sequentialDownload;
					values["super_seeding"] = settings->superSeeding;
					values["force_start"] = settings->forceStart;
					values["upload_mode"] = settings->uploadMode;
					values["share_mode"] = settings->shareMode;
					values["share_ratio_limit"] = std::to_string(settings->shareRatioLimit);
					values["seeding_time_limit"] = settings->seedingTimeLimit;
					values["inactive_seeding_time_limit"] = settings->inactiveSeedingTimeLimit;
					values["share_limit_match_all"] = settings->shareLimitMatchAll;
					values["share_limit_action"] = settings->shareLimitAction;
					values["completion_action"] = settings->completionAction;
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
		// Read file first without lock, then save to database
		std::vector<TaskMetadata> tasksToImport;
		std::vector<TaskSettingsMetadata> settingsToImport;

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
				metadata.taskId = std::string(taskNode.dict_find_string_value("task_id"));
				metadata.magnetUri = std::string(taskNode.dict_find_string_value("magnet_uri"));
				metadata.savePath = std::string(taskNode.dict_find_string_value("save_path"));
				metadata.name = std::string(taskNode.dict_find_string_value("name"));
				metadata.addedTimestamp = taskNode.dict_find_int_value("added_timestamp");
				metadata.totalSize = taskNode.dict_find_int_value("total_size");
				metadata.downloadedSize = taskNode.dict_find_int_value("downloaded_size");
				metadata.uploadedSize = taskNode.dict_find_int_value("uploaded_size");
				metadata.completedTimestamp = taskNode.dict_find_int_value("completed_timestamp");
				metadata.updatedTimestamp = taskNode.dict_find_int_value("updated_timestamp");
				metadata.status = static_cast<int>(taskNode.dict_find_int_value("status"));
				metadata.infoHashV1 = std::string(taskNode.dict_find_string_value("info_hash_v1"));
				metadata.infoHashV2 = std::string(taskNode.dict_find_string_value("info_hash_v2"));
				metadata.errorMessage = std::string(taskNode.dict_find_string_value("error_message"));
				metadata.queuePosition = static_cast<int>(taskNode.dict_find_int_value("queue_position", -1));

				auto resumeStr = taskNode.dict_find_string_value("resume_data");
				if (!resumeStr.empty())
				{
					metadata.resumeData.assign(
						reinterpret_cast<const uint8_t*>(resumeStr.data()),
						reinterpret_cast<const uint8_t*>(resumeStr.data()) + resumeStr.size()
					);
				}

				// Generate new task ID if empty
				if (metadata.taskId.empty())
				{
					metadata.taskId = GenerateTaskId();
				}
				auto const settingsNode = taskNode.dict_find_dict("task_settings");
				if (settingsNode)
				{
					TaskSettingsMetadata settings;
					settings.taskId = metadata.taskId;
					settings.downloadLimit = static_cast<int>(settingsNode.dict_find_int_value("download_limit"));
					settings.uploadLimit = static_cast<int>(settingsNode.dict_find_int_value("upload_limit"));
					settings.minimumUploadRate = static_cast<int>(settingsNode.dict_find_int_value("minimum_upload_rate"));
					settings.maxConnections = static_cast<int>(settingsNode.dict_find_int_value("max_connections", -1));
					settings.maxUploads = static_cast<int>(settingsNode.dict_find_int_value("max_uploads", -1));
					settings.enableDht = settingsNode.dict_find_int_value("enable_dht", 1) != 0;
					settings.enableLsd = settingsNode.dict_find_int_value("enable_lsd", 1) != 0;
					settings.enablePex = settingsNode.dict_find_int_value("enable_pex", 1) != 0;
					settings.applyIpFilter = settingsNode.dict_find_int_value("apply_ip_filter", 1) != 0;
					settings.sequentialDownload = settingsNode.dict_find_int_value("sequential_download") != 0;
					settings.superSeeding = settingsNode.dict_find_int_value("super_seeding") != 0;
					settings.forceStart = settingsNode.dict_find_int_value("force_start") != 0;
					settings.uploadMode = settingsNode.dict_find_int_value("upload_mode") != 0;
					settings.shareMode = settingsNode.dict_find_int_value("share_mode") != 0;
					auto const shareRatioText = settingsNode.dict_find_string_value("share_ratio_limit");
					if (!shareRatioText.empty())
					{
						try
						{
							settings.shareRatioLimit = std::stod(std::string{ shareRatioText });
						}
						catch (...)
						{
							settings.shareRatioLimit = -2.0;
						}
					}
					settings.seedingTimeLimit = static_cast<int>(settingsNode.dict_find_int_value("seeding_time_limit", -2));
					settings.inactiveSeedingTimeLimit = static_cast<int>(settingsNode.dict_find_int_value("inactive_seeding_time_limit", -2));
					settings.shareLimitMatchAll = settingsNode.dict_find_int_value("share_limit_match_all") != 0;
					settings.shareLimitAction = static_cast<int>(settingsNode.dict_find_int_value("share_limit_action", -1));
					settings.completionAction = static_cast<int>(settingsNode.dict_find_int_value("completion_action", -1));
					settingsToImport.push_back(std::move(settings));
				}

				tasksToImport.push_back(std::move(metadata));
			}
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("ImportFromFile read error: " + std::string(ex.what()) + "\n").c_str());
			return false;
		}

		// Resolve imports by content identity. A backup task ID is retained when
		// possible, but it may never overwrite an unrelated local task.
		std::unordered_map<std::string, std::string> resolvedTaskIds;
		std::unordered_set<std::string> preserveLocalSettings;
		for (auto metadata : tasksToImport)
		{
			auto const importedTaskId = metadata.taskId;
			if (auto const existing = FindTaskIdByInfoHashes(
				metadata.infoHashV1, metadata.infoHashV2))
			{
				metadata.taskId = *existing;
				preserveLocalSettings.insert(importedTaskId);
				if (auto local = LoadTaskMetadata(*existing))
				{
					if (local->resumeData.empty() && !metadata.resumeData.empty())
						local->resumeData = metadata.resumeData;
					if (local->magnetUri.empty()) local->magnetUri = metadata.magnetUri;
					if (local->name.empty()) local->name = metadata.name;
					if (local->infoHashV1.empty()) local->infoHashV1 = metadata.infoHashV1;
					if (local->infoHashV2.empty()) local->infoHashV2 = metadata.infoHashV2;
					metadata = std::move(*local);
				}
			}
			else if (auto const sameId = LoadTaskMetadata(metadata.taskId);
					 sameId
					 && (sameId->infoHashV1 != metadata.infoHashV1
						 || sameId->infoHashV2 != metadata.infoHashV2))
			{
				metadata.taskId = GenerateTaskId();
			}
			if (!SaveTaskMetadata(metadata))
				return false;
			resolvedTaskIds.insert_or_assign(importedTaskId, metadata.taskId);
		}
		for (auto settings : settingsToImport)
		{
			if (preserveLocalSettings.contains(settings.taskId))
				continue;
			if (auto const resolved = resolvedTaskIds.find(settings.taskId);
				resolved != resolvedTaskIds.end())
			{
				settings.taskId = resolved->second;
			}
			if (!SaveTaskSettings(settings))
				return false;
		}

		return true;
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
