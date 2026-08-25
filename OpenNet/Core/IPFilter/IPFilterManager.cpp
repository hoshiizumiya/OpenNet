/*
 * PROJECT:   OpenNet
 * FILE:      Core/IPFilter/IPFilterManager.cpp
 * PURPOSE:   IP filter management implementation.
 *
 * LICENSE:   The MIT License
 */
#include "LibtorrentIncludeGuard.h"
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/address.hpp>
#include "LibtorrentIncludeRestore.h"
#include <sqlite3.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <utility>
#include "Core/IPFilter/IPFilterManager.h"

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.IO.FileSystem;
import OpenNet.Core.P2PManager;

namespace lt = libtorrent;

namespace OpenNet::Core
{
	// ---------------------------------------------------------------
	//  Singleton
	// ---------------------------------------------------------------

	IPFilterManager& IPFilterManager::Instance()
	{
		static IPFilterManager s_instance;
		return s_instance;
	}

	IPFilterManager::~IPFilterManager()
	{
		Close();
	}

	// ---------------------------------------------------------------
	//  Lifecycle
	// ---------------------------------------------------------------

	bool IPFilterManager::Initialize()
	{
		{
			std::lock_guard lk(m_mutex);
			if (!m_initialized)
			{
				try
				{
					auto dbPath = std::wstring(winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW()) + L"\\ipfilter.db";
					int rc = sqlite3_open16(dbPath.c_str(), &m_db);
					if (rc != SQLITE_OK)
					{
						OutputDebugStringA(("IPFilterManager: Failed to open database: " +
											std::string(sqlite3_errmsg(m_db)) + "\n").c_str());
						return false;
					}

					sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
					sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

					CreateTables();
					m_initialized = true;
				}
				catch (std::exception const& ex)
				{
					OutputDebugStringA(("IPFilterManager::Initialize error: " +
										std::string(ex.what()) + "\n").c_str());
					return false;
				}
			}
		}

		// File I/O and bulk imports must run outside m_mutex because
		// ImportFromText acquires the same mutex.
		std::call_once(m_seedOnce, [this]()
		{
			SeedBundledRules();
		});
		return true;
	}

	void IPFilterManager::Close()
	{
		std::lock_guard lk(m_mutex);
		if (m_db)
		{
			sqlite3_close(m_db);
			m_db = nullptr;
		}
		m_initialized = false;
	}

	void IPFilterManager::CreateTables()
	{
		const char* sql = R"(
			CREATE TABLE IF NOT EXISTS ip_rules (
				id          INTEGER PRIMARY KEY AUTOINCREMENT,
				first_ip    TEXT NOT NULL,
				last_ip     TEXT NOT NULL,
				flags       INTEGER NOT NULL DEFAULT 1,
				description TEXT DEFAULT ''
			);
			CREATE INDEX IF NOT EXISTS idx_ip_rules_first ON ip_rules(first_ip);
			DELETE FROM ip_rules
			WHERE id NOT IN (
				SELECT MIN(id) FROM ip_rules GROUP BY first_ip, last_ip, flags
			);
			CREATE UNIQUE INDEX IF NOT EXISTS idx_ip_rules_unique
				ON ip_rules(first_ip, last_ip, flags);
			CREATE TABLE IF NOT EXISTS ip_bans (
				id          INTEGER PRIMARY KEY AUTOINCREMENT,
				ip          TEXT NOT NULL,
				port        INTEGER NOT NULL DEFAULT 0,
				task_id     TEXT NOT NULL DEFAULT '',
				client      TEXT NOT NULL DEFAULT '',
				source      TEXT NOT NULL,
				reason      TEXT NOT NULL DEFAULT '',
				created_at  INTEGER NOT NULL,
				expires_at  INTEGER NOT NULL DEFAULT 0
			);
			CREATE INDEX IF NOT EXISTS idx_ip_bans_ip
				ON ip_bans(ip);
			CREATE INDEX IF NOT EXISTS idx_ip_bans_expiry
				ON ip_bans(expires_at);
			CREATE TABLE IF NOT EXISTS ip_filter_subscriptions (
				id           INTEGER PRIMARY KEY AUTOINCREMENT,
				url          TEXT NOT NULL UNIQUE COLLATE NOCASE,
				enabled      INTEGER NOT NULL DEFAULT 1,
				last_updated INTEGER NOT NULL DEFAULT 0,
				last_status  TEXT NOT NULL DEFAULT '',
				rule_count   INTEGER NOT NULL DEFAULT 0,
				last_error   TEXT NOT NULL DEFAULT ''
			);
			CREATE INDEX IF NOT EXISTS idx_ip_filter_subscriptions_enabled
				ON ip_filter_subscriptions(enabled);
		)";

		char* errMsg = nullptr;
		int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
		if (rc != SQLITE_OK)
		{
			OutputDebugStringA(("IPFilterManager: CreateTables error: " +
								std::string(errMsg ? errMsg : "unknown") + "\n").c_str());
			sqlite3_free(errMsg);
		}
	}

	void IPFilterManager::SeedBundledRules()
	{
		try
		{
			auto const appData = std::filesystem::path(
				winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW());
			if (appData.empty())
				return;

			wchar_t executablePath[MAX_PATH]{};
			auto const length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
			if (length == 0 || length == MAX_PATH)
				return;

			auto const sourceFolder =
				std::filesystem::path(executablePath).parent_path() / L"Assets" / L"Rules";
			auto const destinationFolder = appData / L"Rules";
			std::filesystem::create_directories(destinationFolder);

			std::array<std::wstring_view, 2> const ruleFiles
			{
				L"ipfilter.txt",
				L"ipfilter2.txt",
			};

			bool availableAny = false;
			for (auto const fileName : ruleFiles)
			{
				auto const source = sourceFolder / fileName;
				auto const destination = destinationFolder / fileName;
				if (!std::filesystem::exists(destination) &&
					std::filesystem::exists(source))
				{
					// The AppData copy is user-editable. Seed it once and never
					// overwrite later edits on subsequent application starts.
					std::filesystem::copy_file(
						source,
						destination,
						std::filesystem::copy_options::skip_existing);
				}
				availableAny = availableAny || std::filesystem::exists(destination);
			}

			if (!availableAny)
			{
				OutputDebugStringA("IPFilterManager: Bundled rule files were not found\n");
				return;
			}

			auto& settings = AppSettingsDatabase::Instance();
			if (settings.GetBool(
				AppSettingsDatabase::CAT_APP,
				"ipfilter_bundled_rules_imported").value_or(false))
			{
				return;
			}

			int imported = 0;
			for (auto const fileName : ruleFiles)
			{
				auto const path = destinationFolder / fileName;
				std::ifstream stream(path, std::ios::binary);
				if (!stream)
					continue;

				std::string const text{
					std::istreambuf_iterator<char>{ stream },
					std::istreambuf_iterator<char>{} };
				imported += ImportFromText(text);
			}

			settings.SetBool(
				AppSettingsDatabase::CAT_APP,
				"ipfilter_bundled_rules_imported",
				true);
			OutputDebugStringA((
				"IPFilterManager: Seeded " + std::to_string(imported) +
				" bundled IP filter rules\n").c_str());
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA((
				"IPFilterManager::SeedBundledRules error: " +
				std::string(ex.what()) + "\n").c_str());
		}
	}

	// ---------------------------------------------------------------
	//  Rule CRUD
	// ---------------------------------------------------------------

	void IPFilterManager::AddRule(std::string const& firstIp, std::string const& lastIp,
								  uint32_t flags, std::string const& description)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		const char* sql = "INSERT OR IGNORE INTO ip_rules(first_ip, last_ip, flags, description) VALUES(?, ?, ?, ?);";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(stmt, 1, firstIp.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, lastIp.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 3, static_cast<int>(flags));
			sqlite3_bind_text(stmt, 4, description.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	bool IPFilterManager::UpdateRule(int64_t id, std::string const& firstIp,
									 std::string const& lastIp, uint32_t flags,
									 std::string const& description)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return false;

		const char* sql = R"(
			UPDATE OR IGNORE ip_rules
			SET first_ip = ?, last_ip = ?, flags = ?, description = ?
			WHERE id = ?;
		)";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
			return false;

		sqlite3_bind_text(stmt, 1, firstIp.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, lastIp.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, static_cast<int>(flags));
		sqlite3_bind_text(stmt, 4, description.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 5, id);
		auto const result = sqlite3_step(stmt);
		auto const changed = result == SQLITE_DONE && sqlite3_changes(m_db) > 0;
		sqlite3_finalize(stmt);
		return changed;
	}

	void IPFilterManager::RemoveRule(int64_t id)
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;

		const char* sql = "DELETE FROM ip_rules WHERE id = ?;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int64(stmt, 1, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	std::vector<IPRule> IPFilterManager::GetAllRules() const
	{
		std::lock_guard lk(m_mutex);
		std::vector<IPRule> result;
		if (!m_db) return result;

		const char* sql = "SELECT id, first_ip, last_ip, flags, description FROM ip_rules ORDER BY id;";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				IPRule rule;
				rule.id = sqlite3_column_int64(stmt, 0);
				auto f = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
				auto l = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
				rule.flags = static_cast<uint32_t>(sqlite3_column_int(stmt, 3));
				auto d = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
				if (f) rule.firstIp = f;
				if (l) rule.lastIp = l;
				if (d) rule.description = d;
				result.push_back(std::move(rule));
			}
			sqlite3_finalize(stmt);
		}
		return result;
	}

	int IPFilterManager::GetRuleCount() const
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return 0;

		const char* sql = "SELECT COUNT(*) FROM ip_rules;";
		sqlite3_stmt* stmt = nullptr;
		int count = 0;
		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			if (sqlite3_step(stmt) == SQLITE_ROW)
				count = sqlite3_column_int(stmt, 0);
			sqlite3_finalize(stmt);
		}
		return count;
	}

	void IPFilterManager::ClearAllRules()
	{
		std::lock_guard lk(m_mutex);
		if (!m_db) return;
		sqlite3_exec(m_db, "DELETE FROM ip_rules;", nullptr, nullptr, nullptr);
	}

	std::int64_t IPFilterManager::AddSubscription(std::string const& url)
	{
		if (!Initialize())
			return 0;
		auto const first = url.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return 0;
		auto const last = url.find_last_not_of(" \t\r\n");
		auto const normalized = url.substr(first, last - first + 1);

		std::lock_guard lock(m_mutex);
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(
			m_db,
			"INSERT OR IGNORE INTO ip_filter_subscriptions(url, enabled) "
			"VALUES(?, 1);",
			-1, &statement, nullptr) != SQLITE_OK)
		{
			return 0;
		}
		sqlite3_bind_text(
			statement, 1, normalized.c_str(), -1, SQLITE_TRANSIENT);
		auto const inserted = sqlite3_step(statement) == SQLITE_DONE
			&& sqlite3_changes(m_db) > 0;
		sqlite3_finalize(statement);
		return inserted ? sqlite3_last_insert_rowid(m_db) : 0;
	}

	bool IPFilterManager::UpdateSubscription(
		std::int64_t id, std::string const& url, bool enabled)
	{
		if (!Initialize())
			return false;
		auto const first = url.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return false;
		auto const last = url.find_last_not_of(" \t\r\n");
		auto const normalized = url.substr(first, last - first + 1);

		std::lock_guard lock(m_mutex);
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(
			m_db,
			"UPDATE OR IGNORE ip_filter_subscriptions "
			"SET url = ?, enabled = ?, last_updated = 0, "
			"last_status = '', rule_count = 0, last_error = '' WHERE id = ?;",
			-1, &statement, nullptr) != SQLITE_OK)
		{
			return false;
		}
		sqlite3_bind_text(
			statement, 1, normalized.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(statement, 2, enabled ? 1 : 0);
		sqlite3_bind_int64(statement, 3, id);
		auto const changed = sqlite3_step(statement) == SQLITE_DONE
			&& sqlite3_changes(m_db) > 0;
		sqlite3_finalize(statement);
		return changed;
	}

	void IPFilterManager::RemoveSubscription(std::int64_t id)
	{
		if (!Initialize())
			return;
		std::lock_guard lock(m_mutex);
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(
			m_db,
			"DELETE FROM ip_filter_subscriptions WHERE id = ?;",
			-1, &statement, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int64(statement, 1, id);
			sqlite3_step(statement);
			sqlite3_finalize(statement);
		}
	}

	void IPFilterManager::SetSubscriptionEnabled(
		std::int64_t id, bool enabled)
	{
		if (!Initialize())
			return;
		std::lock_guard lock(m_mutex);
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(
			m_db,
			"UPDATE ip_filter_subscriptions SET enabled = ? WHERE id = ?;",
			-1, &statement, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int(statement, 1, enabled ? 1 : 0);
			sqlite3_bind_int64(statement, 2, id);
			sqlite3_step(statement);
			sqlite3_finalize(statement);
		}
	}

	std::vector<IPFilterSubscription> IPFilterManager::GetSubscriptions() const
	{
		const_cast<IPFilterManager*>(this)->Initialize();
		std::vector<IPFilterSubscription> result;
		std::lock_guard lock(m_mutex);
		if (!m_db)
			return result;
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(
			m_db,
			"SELECT id, url, enabled, last_updated, last_status, "
			"rule_count, last_error FROM ip_filter_subscriptions "
			"ORDER BY id;",
			-1, &statement, nullptr) != SQLITE_OK)
		{
			return result;
		}
		while (sqlite3_step(statement) == SQLITE_ROW)
		{
			auto text = [&](int column)
			{
				auto const value = reinterpret_cast<char const*>(
					sqlite3_column_text(statement, column));
				return std::string(value ? value : "");
			};
			IPFilterSubscription subscription;
			subscription.id = sqlite3_column_int64(statement, 0);
			subscription.url = text(1);
			subscription.enabled = sqlite3_column_int(statement, 2) != 0;
			subscription.lastUpdated = sqlite3_column_int64(statement, 3);
			subscription.lastStatus = text(4);
			subscription.ruleCount = sqlite3_column_int(statement, 5);
			subscription.lastError = text(6);
			result.push_back(std::move(subscription));
		}
		sqlite3_finalize(statement);
		return result;
	}

	void IPFilterManager::SetSubscriptionUpdateResult(
		std::int64_t id,
		std::int64_t updatedAt,
		bool succeeded,
		std::int32_t ruleCount,
		std::string const& error)
	{
		if (!Initialize())
			return;
		std::lock_guard lock(m_mutex);
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(
			m_db,
			"UPDATE ip_filter_subscriptions SET last_updated = ?, "
			"last_status = ?, rule_count = ?, last_error = ? WHERE id = ?;",
			-1, &statement, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int64(statement, 1, updatedAt);
			sqlite3_bind_text(
				statement, 2, succeeded ? "success" : "failed",
				-1, SQLITE_STATIC);
			sqlite3_bind_int(statement, 3, ruleCount);
			sqlite3_bind_text(
				statement, 4, error.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(statement, 5, id);
			sqlite3_step(statement);
			sqlite3_finalize(statement);
		}
	}

	bool IPFilterManager::SubscriptionAutoUpdateEnabled() const
	{
		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		return settings.GetBool("ip_filter_subscription", "auto_update")
			.value_or(true);
	}

	void IPFilterManager::SubscriptionAutoUpdateEnabled(bool value)
	{
		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		settings.SetBool("ip_filter_subscription", "auto_update", value);
	}

	bool IPFilterManager::SubscriptionReplaceExisting() const
	{
		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		return settings.GetBool("ip_filter_subscription", "replace_existing")
			.value_or(false);
	}

	void IPFilterManager::SubscriptionReplaceExisting(bool value)
	{
		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		settings.SetBool("ip_filter_subscription", "replace_existing", value);
	}

	std::int32_t IPFilterManager::SubscriptionUpdateIntervalHours() const
	{
		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		return static_cast<std::int32_t>(std::clamp<std::int64_t>(
			settings.GetInt("ip_filter_subscription", "interval_hours")
			.value_or(24),
			1,
			168));
	}

	void IPFilterManager::SubscriptionUpdateIntervalHours(std::int32_t value)
	{
		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		settings.SetInt(
			"ip_filter_subscription", "interval_hours",
			std::clamp(value, 1, 168));
	}

	std::int64_t IPFilterManager::SubscriptionLastUpdate() const
	{
		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		return settings.GetInt("ip_filter_subscription", "last_update")
			.value_or(0);
	}

	std::string IPFilterManager::SubscriptionLastResult() const
	{
		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		return settings.GetString("ip_filter_subscription", "last_result")
			.value_or("");
	}

	void IPFilterManager::SetSubscriptionLastResult(std::int64_t updatedAt, std::string const& result)
	{
		auto& settings = AppSettingsDatabase::Instance();
		settings.Initialize();
		settings.SetBatch({
			{ "last_update", std::to_string(updatedAt), "ip_filter_subscription" },
			{ "last_result", result, "ip_filter_subscription" },
						  });
	}

	bool IPFilterManager::IsSubscriptionUpdateDue(std::int64_t now) const
	{
		if (!SubscriptionAutoUpdateEnabled())
			return false;
		auto const last = SubscriptionLastUpdate();
		if (last <= 0)
			return true;
		auto const interval = static_cast<std::int64_t>(
			SubscriptionUpdateIntervalHours()) * 60 * 60;
		return now >= last + interval;
	}

	std::int64_t IPFilterManager::AddBan(
		std::string const& ip, std::int32_t port,
		std::string const& taskId, std::string const& client,
		std::string const& source, std::string const& reason,
		std::int64_t expiresAt)
	{
		if (!Initialize())
			return 0;
		boost::system::error_code error;
		auto const address = lt::make_address(ip, error);
		if (error)
			return 0;
		auto const normalizedIp = address.to_string();
		auto const now = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		std::lock_guard lock(m_mutex);
		sqlite3_stmt* remove = nullptr;
		if (sqlite3_prepare_v2(
			m_db,
			"DELETE FROM ip_bans WHERE ip = ? AND source = ?;",
			-1, &remove, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_text(remove, 1, normalizedIp.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(remove, 2, source.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(remove);
			sqlite3_finalize(remove);
		}

		sqlite3_stmt* insert = nullptr;
		if (sqlite3_prepare_v2(
			m_db,
			"INSERT INTO ip_bans("
			"ip, port, task_id, client, source, reason, created_at, expires_at) "
			"VALUES(?, ?, ?, ?, ?, ?, ?, ?);",
			-1, &insert, nullptr) != SQLITE_OK)
		{
			return 0;
		}
		sqlite3_bind_text(insert, 1, normalizedIp.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(insert, 2, port);
		sqlite3_bind_text(insert, 3, taskId.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insert, 4, client.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insert, 5, source.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(insert, 6, reason.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(insert, 7, now);
		sqlite3_bind_int64(insert, 8, expiresAt);
		auto const success = sqlite3_step(insert) == SQLITE_DONE;
		sqlite3_finalize(insert);
		return success ? sqlite3_last_insert_rowid(m_db) : 0;
	}

	bool IPFilterManager::RemoveBan(std::string const& ip, std::string const& source)
	{
		if (!Initialize())
			return false;
		boost::system::error_code error;
		auto const address = lt::make_address(ip, error);
		if (error)
			return false;

		bool changed = false;
		{
			std::lock_guard lock(m_mutex);
			auto const sql = source.empty()
				? "DELETE FROM ip_bans WHERE ip = ?;"
				: "DELETE FROM ip_bans WHERE ip = ? AND source = ?;";
			sqlite3_stmt* statement = nullptr;
			if (sqlite3_prepare_v2(
				m_db, sql, -1, &statement, nullptr) == SQLITE_OK)
			{
				auto const normalizedIp = address.to_string();
				sqlite3_bind_text(
					statement, 1, normalizedIp.c_str(), -1, SQLITE_TRANSIENT);
				if (!source.empty())
				{
					sqlite3_bind_text(
						statement, 2, source.c_str(), -1, SQLITE_TRANSIENT);
				}
				changed = sqlite3_step(statement) == SQLITE_DONE
					&& sqlite3_changes(m_db) > 0;
				sqlite3_finalize(statement);
			}
		}
		if (changed)
			ApplyToSession();
		return changed;
	}

	std::vector<IPBanEntry> IPFilterManager::GetActiveBansLocked(std::string const& taskId) const
	{
		std::vector<IPBanEntry> result;
		if (!m_db)
			return result;
		auto const now = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		constexpr char const* sql = R"(
			SELECT id, ip, port, task_id, client, source, reason,
				created_at, expires_at
			FROM ip_bans
			WHERE (expires_at = 0 OR expires_at > ?)
				AND (? = '' OR task_id = '' OR task_id = ?)
			ORDER BY id DESC;
		)";
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
			return result;
		sqlite3_bind_int64(statement, 1, now);
		sqlite3_bind_text(statement, 2, taskId.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(statement, 3, taskId.c_str(), -1, SQLITE_TRANSIENT);
		while (sqlite3_step(statement) == SQLITE_ROW)
		{
			IPBanEntry ban;
			ban.id = sqlite3_column_int64(statement, 0);
			auto text = [&](int column)
			{
				auto value = reinterpret_cast<char const*>(
					sqlite3_column_text(statement, column));
				return std::string(value ? value : "");
			};
			ban.ip = text(1);
			ban.port = sqlite3_column_int(statement, 2);
			ban.taskId = text(3);
			ban.client = text(4);
			ban.source = text(5);
			ban.reason = text(6);
			ban.createdAt = sqlite3_column_int64(statement, 7);
			ban.expiresAt = sqlite3_column_int64(statement, 8);
			result.push_back(std::move(ban));
		}
		sqlite3_finalize(statement);
		return result;
	}

	std::vector<IPBanEntry> IPFilterManager::GetActiveBans(std::string const& taskId) const
	{
		const_cast<IPFilterManager*>(this)->Initialize();
		std::lock_guard lock(m_mutex);
		return GetActiveBansLocked(taskId);
	}

	std::optional<IPBanEntry> IPFilterManager::FindActiveBan(std::string const& ip) const
	{
		auto const bans = GetActiveBans();
		auto const item = std::find_if(
			bans.begin(), bans.end(),
			[&](auto const& ban)
		{
			return ban.ip == ip;
		});
		return item == bans.end()
			? std::nullopt
			: std::optional<IPBanEntry>{ *item };
	}

	std::optional<IPRule> IPFilterManager::FindMatchingRule(std::string const& ip) const
	{
		auto matches = FindMatchingRules({ ip });
		return matches.empty() ? std::nullopt : std::move(matches.front());
	}

	std::vector<std::optional<IPRule>> IPFilterManager::FindMatchingRules(std::vector<std::string> const& addresses) const
	{
		std::vector<std::optional<IPRule>> result(addresses.size());
		if (addresses.empty())
			return result;
		if (!const_cast<IPFilterManager*>(this)->Initialize())
			return result;

		auto const rules = GetAllRules();
		lt::ip_filter lookup;
		std::vector<IPRule const*> taggedRules(1, nullptr);
		for (auto const& rule : rules)
		{
			if ((rule.flags & lt::ip_filter::blocked) == 0)
				continue;
			boost::system::error_code firstError;
			boost::system::error_code lastError;
			auto const first = lt::make_address(rule.firstIp, firstError);
			auto const last = lt::make_address(rule.lastIp, lastError);
			if (firstError || lastError)
				continue;
			auto const tag = static_cast<std::uint32_t>(taggedRules.size());
			lookup.add_rule(first, last, tag);
			taggedRules.push_back(&rule);
		}

		for (std::size_t index = 0; index < addresses.size(); ++index)
		{
			boost::system::error_code error;
			auto const address = lt::make_address(addresses[index], error);
			if (error)
				continue;
			auto const tag = lookup.access(address);
			if (tag > 0 && tag < taggedRules.size())
				result[index] = *taggedRules[tag];
		}
		return result;
	}

	void IPFilterManager::MaintainTemporaryBans()
	{
		if (!Initialize())
			return;
		auto const now = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		bool changed = false;
		{
			std::lock_guard lock(m_mutex);
			sqlite3_stmt* statement = nullptr;
			if (sqlite3_prepare_v2(
				m_db,
				"DELETE FROM ip_bans "
				"WHERE expires_at > 0 AND expires_at <= ?;",
				-1, &statement, nullptr) == SQLITE_OK)
			{
				sqlite3_bind_int64(statement, 1, now);
				changed = sqlite3_step(statement) == SQLITE_DONE &&
					sqlite3_changes(m_db) > 0;
				sqlite3_finalize(statement);
			}
		}
		if (changed)
			ApplyToSession();
	}

	// ---------------------------------------------------------------
	//  Parsing helpers
	// ---------------------------------------------------------------

	bool IPFilterManager::ParseIPOrCIDR(std::string const& input,
										std::string& outFirst,
										std::string& outLast)
	{
		if (input.empty()) return false;

		// ---- CIDR notation (e.g. "1.2.3.0/24" or "2400::/50") ----
		auto slashPos = input.find('/');
		if (slashPos != std::string::npos)
		{
			auto addrStr = input.substr(0, slashPos);
			int prefix = 0;
			try
			{
				prefix = std::stoi(input.substr(slashPos + 1));
			}
			catch (...)
			{
				return false;
			}

			boost::system::error_code ec;
			auto addr = lt::make_address(addrStr, ec);
			if (ec) return false;

			if (addr.is_v4())
			{
				if (prefix < 0 || prefix > 32) return false;
				auto bytes = addr.to_v4().to_bytes();

				auto firstBytes = bytes;
				auto lastBytes = bytes;
				for (int i = prefix; i < 32; ++i)
				{
					int byteIdx = i / 8;
					int bitIdx = 7 - (i % 8);
					firstBytes[byteIdx] &= static_cast<unsigned char>(~(1u << bitIdx));
					lastBytes[byteIdx] |= static_cast<unsigned char>(1u << bitIdx);
				}
				outFirst = lt::address_v4(firstBytes).to_string();
				outLast = lt::address_v4(lastBytes).to_string();
				return true;
			}
			else // IPv6
			{
				if (prefix < 0 || prefix > 128) return false;
				auto bytes = addr.to_v6().to_bytes();

				auto firstBytes = bytes;
				auto lastBytes = bytes;
				for (int i = prefix; i < 128; ++i)
				{
					int byteIdx = i / 8;
					int bitIdx = 7 - (i % 8);
					firstBytes[byteIdx] &= static_cast<unsigned char>(~(1u << bitIdx));
					lastBytes[byteIdx] |= static_cast<unsigned char>(1u << bitIdx);
				}
				outFirst = lt::address_v6(firstBytes).to_string();
				outLast = lt::address_v6(lastBytes).to_string();
				return true;
			}
		}

		// ---- Address range notation (IPv4 or IPv6) ----
		auto dashPos = input.find('-');
		if (dashPos != std::string::npos)
		{
			auto firstStr = input.substr(0, dashPos);
			auto lastStr = input.substr(dashPos + 1);
			auto trim = [](std::string& value)
			{
				auto const first = value.find_first_not_of(" \t");
				if (first == std::string::npos)
				{
					value.clear();
					return;
				}
				auto const last = value.find_last_not_of(" \t");
				value = value.substr(first, last - first + 1);
			};
			trim(firstStr);
			trim(lastStr);

			boost::system::error_code ec1, ec2;
			auto first = lt::make_address(firstStr, ec1);
			auto last = lt::make_address(lastStr, ec2);
			if (ec1 || ec2) return false;
			if (first.is_v4() != last.is_v4()) return false;

			outFirst = first.to_string();
			outLast = last.to_string();
			return true;
		}

		// ---- Single IP address ----
		boost::system::error_code ec;
		auto addr = lt::make_address(input, ec);
		if (ec) return false;

		outFirst = addr.to_string();
		outLast = outFirst;
		return true;
	}

	// ---------------------------------------------------------------
	//  Bulk import
	// ---------------------------------------------------------------

	namespace
	{
		struct ParsedTextRule
		{
			std::string first;
			std::string last;
			std::string description;
		};

		std::string TrimRuleText(std::string value)
		{
			auto const first = value.find_first_not_of(" \t\r\n");
			if (first == std::string::npos)
				return {};
			auto const last = value.find_last_not_of(" \t\r\n");
			return value.substr(first, last - first + 1);
		}

		std::optional<ParsedTextRule> ParseTextRule(std::string line)
		{
			line = TrimRuleText(std::move(line));
			if (line.empty() || line[0] == '#' || line[0] == ';')
				return std::nullopt;

			auto tryCandidate = [&](std::string candidate)
				-> std::optional<ParsedTextRule>
			{
				candidate = TrimRuleText(std::move(candidate));
				ParsedTextRule rule;
				if (!IPFilterManager::ParseIPOrCIDR(
					candidate, rule.first, rule.last))
				{
					return std::nullopt;
				}
				rule.description = line;
				return rule;
			};

			if (auto direct = tryCandidate(line))
				return direct;

			// eMule-style lists commonly use "range, level, name".
			if (auto const comma = line.find(','); comma != std::string::npos)
			{
				if (auto candidate = tryCandidate(line.substr(0, comma)))
					return candidate;
			}

			// PeerGuardian/P2P lists commonly use "label:range". Try each
			// colon so IPv6 ranges remain supported as well.
			for (auto colon = line.find(':'); colon != std::string::npos;
				 colon = line.find(':', colon + 1))
			{
				if (auto candidate = tryCandidate(line.substr(colon + 1)))
					return candidate;
			}
			return std::nullopt;
		}

		std::vector<ParsedTextRule> ParseTextRules(std::string const& text)
		{
			std::vector<ParsedTextRule> parsed;
			std::istringstream stream(text);
			std::string line;
			std::string source;
			while (std::getline(stream, line))
			{
				auto const trimmed = TrimRuleText(line);
				constexpr std::string_view sourceMarker = "# OpenNet-Source:";
				if (trimmed.starts_with(sourceMarker))
				{
					source = TrimRuleText(
						trimmed.substr(sourceMarker.size()));
					continue;
				}
				if (auto rule = ParseTextRule(std::move(line)))
				{
					if (!source.empty())
						rule->description = source + " | " + rule->description;
					parsed.push_back(std::move(*rule));
				}
			}
			return parsed;
		}
	}

	std::int32_t IPFilterManager::CountRulesInText(std::string const& text)
	{
		return static_cast<std::int32_t>(ParseTextRules(text).size());
	}

	int IPFilterManager::ImportFromText(
		std::string const& text, bool replaceExisting)
	{
		auto parsed = ParseTextRules(text);

		if (parsed.empty()) return 0;
		bool databaseReady = false;
		{
			std::lock_guard lock(m_mutex);
			databaseReady = m_db != nullptr;
		}
		if (!databaseReady && !Initialize()) return 0;

		// ---- Phase 2: bulk insert inside transaction ----
		std::lock_guard lk(m_mutex);
		if (!m_db) return 0;

		sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
		if (replaceExisting)
			sqlite3_exec(m_db, "DELETE FROM ip_rules;", nullptr, nullptr, nullptr);

		const char* sql = "INSERT OR IGNORE INTO ip_rules(first_ip, last_ip, flags, description) VALUES(?, ?, ?, ?);";
		sqlite3_stmt* stmt = nullptr;
		int count = 0;

		if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
		{
			for (auto const& rule : parsed)
			{
				sqlite3_reset(stmt);
				sqlite3_clear_bindings(stmt);
				sqlite3_bind_text(stmt, 1, rule.first.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, 2, rule.last.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(stmt, 3, static_cast<int>(lt::ip_filter::blocked));
				sqlite3_bind_text(
					stmt, 4, rule.description.c_str(), -1, SQLITE_TRANSIENT);
				if (sqlite3_step(stmt) == SQLITE_DONE
					&& sqlite3_changes(m_db) > 0)
				{
					++count;
				}
			}
			sqlite3_finalize(stmt);
		}
		else
		{
			sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
			return 0;
		}

		sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
		return count;
	}

	// ---------------------------------------------------------------
	//  Session integration
	// ---------------------------------------------------------------

	lt::ip_filter IPFilterManager::BuildFilter() const
	{
		lt::ip_filter filter;
		auto rules = GetAllRules(); // acquires m_mutex internally

		for (auto const& rule : rules)
		{
			boost::system::error_code ec1, ec2;
			auto first = lt::make_address(rule.firstIp, ec1);
			auto last = lt::make_address(rule.lastIp, ec2);
			if (!ec1 && !ec2)
			{
				filter.add_rule(first, last, rule.flags);
			}
		}
		return filter;
	}

	lt::ip_filter IPFilterManager::BuildSessionFilter() const
	{
		lt::ip_filter filter;
		if (IsEnabled())
		{
			filter = BuildFilter();
		}

		for (auto const& ban : GetActiveBans())
		{
			boost::system::error_code error;
			auto const address = lt::make_address(ban.ip, error);
			if (!error)
				filter.add_rule(address, address, lt::ip_filter::blocked);
		}

		std::vector<std::string> clientBlocked;
		{
			std::lock_guard lock(m_clientBlockedMutex);
			clientBlocked.assign(
				m_clientBlockedAddresses.begin(),
				m_clientBlockedAddresses.end());
		}
		for (auto const& ip : clientBlocked)
		{
			boost::system::error_code error;
			auto const address = lt::make_address(ip, error);
			if (!error)
				filter.add_rule(address, address, lt::ip_filter::blocked);
		}
		return filter;
	}

	void IPFilterManager::ApplyToSession()
	{
		// Keep independently triggered IP-rule and client-rule updates ordered,
		// so a slower older snapshot cannot overwrite a newer client block set.
		std::lock_guard applyLock(m_applyMutex);
		auto* core = P2PManager::Instance().TorrentCore();
		if (!core || !core->IsRunning()) return;
		core->ReloadIpFilter();
	}

	bool IPFilterManager::IsEnabled() const
	{
		return AppSettingsDatabase::Instance()
			.GetBool(AppSettingsDatabase::CAT_APP, "ipfilter_enabled")
			.value_or(false);
	}

	void IPFilterManager::SetEnabled(bool enabled)
	{
		AppSettingsDatabase::Instance()
			.SetBool(AppSettingsDatabase::CAT_APP, "ipfilter_enabled", enabled);
	}

	void IPFilterManager::SetClientBlockedAddresses(
		std::vector<std::string> const& addresses)
	{
		std::lock_guard lock(m_clientBlockedMutex);
		m_clientBlockedAddresses.clear();
		m_clientBlockedAddresses.insert(addresses.begin(), addresses.end());
	}

} // namespace OpenNet::Core
