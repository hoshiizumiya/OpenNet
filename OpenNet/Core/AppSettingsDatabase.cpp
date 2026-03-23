/*
 * PROJECT:   OpenNet
 * FILE:      Core/AppSettingsDatabase.cpp
 * PURPOSE:   Unified SQLite-based application settings storage implementation.
 *
 * LICENSE:   The MIT License
 */

#include "pch.h"
#include "Core/AppSettingsDatabase.h"
#include "ThirdParty/Sqlite/sqlite3.h"
#include "Core/IO/FileSystem.h"

#include <filesystem>
#include <sstream>

namespace OpenNet::Core
{
	AppSettingsDatabase& AppSettingsDatabase::Instance()
	{
		static AppSettingsDatabase s_instance;
		return s_instance;
	}

	AppSettingsDatabase::~AppSettingsDatabase()
	{
		Close();
	}

	bool AppSettingsDatabase::Initialize()
	{
		std::lock_guard lk(m_mutex);
		if (m_initialized) return true;

		try
		{
			int rc = sqlite3_open16((std::filesystem::path(winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW()) / "app_settings.db").c_str(), &m_db);
			if (rc != SQLITE_OK)
			{
				OutputDebugStringA(("AppSettingsDatabase: Failed to open database: " +
									std::string(sqlite3_errmsg(m_db)) + "\n").c_str());
				return false;
			}

			// Enable WAL mode for better concurrent read/write performance
			sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
			sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

			CreateTables();
			m_initialized = true;
			return true;
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("AppSettingsDatabase::Initialize error: " +
								std::string(ex.what()) + "\n").c_str());
			return false;
		}
	}

	void AppSettingsDatabase::Close()
	{
		std::lock_guard lk(m_mutex);
		if (m_db)
		{
			sqlite3_close(m_db);
			m_db = nullptr;
		}
		m_initialized = false;
	}

	void AppSettingsDatabase::CreateTables()
	{
		const char* sql = R"(
            CREATE TABLE IF NOT EXISTS settings (
                category TEXT NOT NULL,
                key      TEXT NOT NULL,
                value    TEXT,
                PRIMARY KEY (category, key)
            );
            CREATE INDEX IF NOT EXISTS idx_settings_category ON settings(category);
        )";

		char* errMsg = nullptr;
		int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
		if (rc != SQLITE_OK)
		{
			OutputDebugStringA(("AppSettingsDatabase: CreateTables error: " +
								std::string(errMsg ? errMsg : "unknown") + "\n").c_str());
			sqlite3_free(errMsg);
		}
	}

	// ---------------------------------------------------------------
	//  Auto-initialize helper (called from Set/Get methods)
	// ---------------------------------------------------------------
	bool AppSettingsDatabase::EnsureInitialized()
	{
		if (m_initialized && m_db) return true;
		return Initialize();
	}

	// ---------------------------------------------------------------
	//  Set / Get operations
	// ---------------------------------------------------------------

	void AppSettingsDatabase::SetString(std::string const& category,
										std::string const& key,
										std::string const& value)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db && !EnsureInitialized()) return;

		const char* sql = "INSERT OR REPLACE INTO settings(category, key, value) VALUES(?, ?, ?);";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, value.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	std::optional<std::string> AppSettingsDatabase::GetString(std::string const& category,
															  std::string const& key) const
	{
		std::lock_guard lk(m_mutex);
		if (!m_db && !const_cast<AppSettingsDatabase*>(this)->EnsureInitialized()) return std::nullopt;

		const char* sql = "SELECT value FROM settings WHERE category = ? AND key = ?;";
		sqlite3_stmt* stmt = nullptr;
		std::optional<std::string> result;

		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);

			if (sqlite3_step(stmt) == SQLITE_ROW)
			{
				auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
				if (text) result = std::string(text);
			}
			sqlite3_finalize(stmt);
		}
		return result;
	}

	void AppSettingsDatabase::SetInt(std::string const& category,
									 std::string const& key,
									 int64_t value)
	{
		SetString(category, key, std::to_string(value));
	}

	std::optional<int64_t> AppSettingsDatabase::GetInt(std::string const& category,
													   std::string const& key) const
	{
		auto str = GetString(category, key);
		if (!str) return std::nullopt;
		try
		{
			return std::stoll(*str);
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	void AppSettingsDatabase::SetDouble(std::string const& category,
										std::string const& key,
										double value)
	{
		std::ostringstream oss;
		oss.precision(15);
		oss << value;
		SetString(category, key, oss.str());
	}

	std::optional<double> AppSettingsDatabase::GetDouble(std::string const& category,
														 std::string const& key) const
	{
		auto str = GetString(category, key);
		if (!str) return std::nullopt;
		try
		{
			return std::stod(*str);
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	void AppSettingsDatabase::SetBool(std::string const& category,
									  std::string const& key,
									  bool value)
	{
		SetString(category, key, value ? "1" : "0");
	}

	std::optional<bool> AppSettingsDatabase::GetBool(std::string const& category,
													 std::string const& key) const
	{
		auto val = GetInt(category, key);
		if (!val) return std::nullopt;
		return *val != 0;
	}

	void AppSettingsDatabase::Delete(std::string const& category,
									 std::string const& key)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		const char* sql = "DELETE FROM settings WHERE category = ? AND key = ?;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	std::vector<SettingEntry> AppSettingsDatabase::GetCategory(std::string const& category) const
	{
		std::lock_guard lk(m_mutex);
		std::vector<SettingEntry> result;
		if (!m_db && !const_cast<AppSettingsDatabase*>(this)->EnsureInitialized()) return result;

		const char* sql = "SELECT key, value FROM settings WHERE category = ?;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);

			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				SettingEntry entry;
				entry.category = category;
				auto k = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
				auto v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
				if (k) entry.key = k;
				if (v) entry.value = v;
				result.push_back(std::move(entry));
			}
			sqlite3_finalize(stmt);
		}
		return result;
	}

	void AppSettingsDatabase::DeleteCategory(std::string const& category)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		const char* sql = "DELETE FROM settings WHERE category = ?;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	void AppSettingsDatabase::SetBatch(std::vector<SettingEntry> const& entries)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

		const char* sql = "INSERT OR REPLACE INTO settings(category, key, value) VALUES(?, ?, ?);";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			for (auto const& entry : entries)
			{
				sqlite3_reset(stmt);
				sqlite3_bind_text(stmt, 1, entry.category.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, 2, entry.key.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, 3, entry.value.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_step(stmt);
			}
			sqlite3_finalize(stmt);
		}

		sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
	}

	std::unordered_map<std::string, std::string> AppSettingsDatabase::GetAll() const
	{
		std::lock_guard lk(m_mutex);
		std::unordered_map<std::string, std::string> result;
		if (!m_db) return result;

		const char* sql = "SELECT category, key, value FROM settings;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				auto cat = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
				auto key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
				auto val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
				if (cat && key)
				{
					std::string compositeKey = std::string(cat) + "." + key;
					result[compositeKey] = val ? val : "";
				}
			}
			sqlite3_finalize(stmt);
		}
		return result;
	}

	// ---------------------------------------------------------------
	//  Wide-string convenience methods (UTF-16 direct to SQLite)
	// ---------------------------------------------------------------

	void AppSettingsDatabase::SetStringW(std::string const& category,
										 std::string const& key,
										 std::wstring_view const& value)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db && !const_cast<AppSettingsDatabase*>(this)->EnsureInitialized()) return;

		const char* sql = "INSERT OR REPLACE INTO settings(category, key, value) VALUES(?, ?, ?);";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text16(stmt, 3, value.data(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	std::optional<std::wstring> AppSettingsDatabase::GetStringW(std::string const& category,
																	 std::string const& key) const
	{
		std::lock_guard lk(m_mutex);
		if (!m_db && !const_cast<AppSettingsDatabase*>(this)->EnsureInitialized()) return std::nullopt;

		const char* sql = "SELECT value FROM settings WHERE category = ? AND key = ?;";
		sqlite3_stmt* stmt = nullptr;
		std::optional<std::wstring> result;

		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);

			if (sqlite3_step(stmt) == SQLITE_ROW)
			{
				auto text = reinterpret_cast<const wchar_t*>(sqlite3_column_text16(stmt, 0));
				if (text)
				{
					result = std::wstring(text);
				}
			}
			sqlite3_finalize(stmt);
		}
		return result;
	}

} // namespace OpenNet::Core
