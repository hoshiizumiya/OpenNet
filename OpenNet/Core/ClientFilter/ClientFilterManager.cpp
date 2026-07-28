#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <sqlite3.h>
#include <nlohmann/json.hpp>

#include "Core/ClientFilter/ClientFilterManager.h"
#include "Core/IPFilter/IPFilterManager.h"

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.IO.FileSystem;

namespace OpenNet::Core
{
	namespace
	{
		constexpr char const* EnabledSetting = "clientfilter_enabled";

		std::string TrimCopy(std::string value)
		{
			auto const first = value.find_first_not_of(" \t\r\n");
			if (first == std::string::npos)
				return {};
			auto const last = value.find_last_not_of(" \t\r\n");
			return value.substr(first, last - first + 1);
		}

		std::string LowerAscii(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(),
						   [](unsigned char ch)
			{
				return static_cast<char>(std::tolower(ch));
			});
			return value;
		}

		ClientMatchType NormalizeMatchType(int value)
		{
			if (value < static_cast<int>(ClientMatchType::Contains) ||
				value > static_cast<int>(ClientMatchType::Regex))
			{
				return ClientMatchType::Contains;
			}
			return static_cast<ClientMatchType>(value);
		}
	}

	ClientFilterManager& ClientFilterManager::Instance()
	{
		static ClientFilterManager instance;
		return instance;
	}

	ClientFilterManager::~ClientFilterManager()
	{
		Close();
	}

	bool ClientFilterManager::Initialize()
	{
		std::lock_guard lock(m_mutex);
		if (m_initialized)
			return true;

		try
		{
			auto const path =
				std::filesystem::path(
					winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW()) /
				L"clientfilter.db";
			if (sqlite3_open16(path.c_str(), &m_db) != SQLITE_OK)
			{
				OutputDebugStringA("ClientFilterManager: failed to open database\n");
				if (m_db)
				{
					sqlite3_close(m_db);
					m_db = nullptr;
				}
				return false;
			}
			sqlite3_busy_timeout(m_db, 1000);
			sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
			sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
			CreateTables();
			LoadRulesLocked();
			m_initialized = true;
			return true;
		}
		catch (std::exception const& error)
		{
			OutputDebugStringA((
				"ClientFilterManager::Initialize: " +
				std::string(error.what()) + "\n").c_str());
			if (m_db)
			{
				sqlite3_close(m_db);
				m_db = nullptr;
			}
			return false;
		}
	}

	void ClientFilterManager::Close()
	{
		std::lock_guard lock(m_mutex);
		if (m_db)
		{
			sqlite3_close(m_db);
			m_db = nullptr;
		}
		m_rules.clear();
		m_runtimeBlockedAddresses.clear();
		m_initialized = false;
	}

	void ClientFilterManager::CreateTables()
	{
		constexpr char const* sql = R"(
			CREATE TABLE IF NOT EXISTS client_filter_rules (
				id INTEGER PRIMARY KEY AUTOINCREMENT,
				pattern TEXT NOT NULL,
				match_type INTEGER NOT NULL DEFAULT 0,
				case_sensitive INTEGER NOT NULL DEFAULT 0,
				enabled INTEGER NOT NULL DEFAULT 1,
				description TEXT NOT NULL DEFAULT '',
				hit_count INTEGER NOT NULL DEFAULT 0,
				last_hit INTEGER NOT NULL DEFAULT 0
			);
			CREATE UNIQUE INDEX IF NOT EXISTS idx_client_filter_rule_unique
				ON client_filter_rules(pattern, match_type, case_sensitive);
			CREATE TABLE IF NOT EXISTS client_filter_hits (
				id INTEGER PRIMARY KEY AUTOINCREMENT,
				rule_id INTEGER NOT NULL,
				pattern TEXT NOT NULL DEFAULT '',
				client TEXT NOT NULL,
				ip TEXT NOT NULL,
				task_name TEXT NOT NULL DEFAULT '',
				timestamp INTEGER NOT NULL
			);
			CREATE INDEX IF NOT EXISTS idx_client_filter_hits_time
				ON client_filter_hits(timestamp DESC);
		)";
		char* error = nullptr;
		if (sqlite3_exec(m_db, sql, nullptr, nullptr, &error) != SQLITE_OK)
		{
			std::string message = error ? error : "unknown SQLite error";
			sqlite3_free(error);
			throw std::runtime_error(message);
		}

		// Migration for databases created by early development builds.
		sqlite3_exec(
			m_db,
			"ALTER TABLE client_filter_hits "
			"ADD COLUMN pattern TEXT NOT NULL DEFAULT '';",
			nullptr, nullptr, nullptr);
	}

	void ClientFilterManager::LoadRulesLocked()
	{
		m_rules.clear();
		if (!m_db)
			return;

		constexpr char const* sql = R"(
			SELECT id, pattern, match_type, case_sensitive, enabled,
				description, hit_count, last_hit
			FROM client_filter_rules
			ORDER BY id;
		)";
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
			return;

		while (sqlite3_step(statement) == SQLITE_ROW)
		{
			ClientFilterRule rule;
			rule.id = sqlite3_column_int64(statement, 0);
			auto const pattern = reinterpret_cast<char const*>(
				sqlite3_column_text(statement, 1));
			rule.pattern = pattern ? pattern : "";
			rule.matchType = NormalizeMatchType(sqlite3_column_int(statement, 2));
			rule.caseSensitive = sqlite3_column_int(statement, 3) != 0;
			rule.enabled = sqlite3_column_int(statement, 4) != 0;
			auto const description = reinterpret_cast<char const*>(
				sqlite3_column_text(statement, 5));
			rule.description = description ? description : "";
			rule.hitCount = sqlite3_column_int64(statement, 6);
			rule.lastHitTimestamp = sqlite3_column_int64(statement, 7);
			m_rules.push_back(std::move(rule));
		}
		sqlite3_finalize(statement);
	}

	bool ClientFilterManager::ValidatePattern(std::string const& pattern,
											  ClientMatchType matchType, std::string* error)
	{
		if (TrimCopy(pattern).empty())
		{
			if (error) *error = "Pattern cannot be empty";
			return false;
		}
		if (matchType == ClientMatchType::Regex)
		{
			try
			{
				std::regex expression(pattern);
				(void)expression;
			}
			catch (std::regex_error const& regexError)
			{
				if (error) *error = regexError.what();
				return false;
			}
		}
		return true;
	}

	bool ClientFilterManager::AddRule(std::string const& pattern,
									  ClientMatchType matchType, bool caseSensitive,
									  std::string const& description)
	{
		auto const normalizedPattern = TrimCopy(pattern);
		if (!ValidatePattern(normalizedPattern, matchType))
			return false;
		if (!Initialize())
			return false;

		bool changed = false;
		{
			std::lock_guard lock(m_mutex);
			constexpr char const* sql = R"(
				INSERT OR IGNORE INTO client_filter_rules(
					pattern, match_type, case_sensitive, enabled, description)
				VALUES(?, ?, ?, 1, ?);
			)";
			sqlite3_stmt* statement = nullptr;
			if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) == SQLITE_OK)
			{
				sqlite3_bind_text(statement, 1, normalizedPattern.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(statement, 2, static_cast<int>(matchType));
				sqlite3_bind_int(statement, 3, caseSensitive ? 1 : 0);
				sqlite3_bind_text(statement, 4, description.c_str(), -1, SQLITE_TRANSIENT);
				changed = sqlite3_step(statement) == SQLITE_DONE &&
					sqlite3_changes(m_db) > 0;
				sqlite3_finalize(statement);
			}
			if (changed)
				LoadRulesLocked();
		}
		if (changed)
			ResetRuntimeBlocks();
		return changed;
	}

	bool ClientFilterManager::UpdateRule(std::int64_t id,
										 std::string const& pattern, ClientMatchType matchType,
										 bool caseSensitive, bool enabled, std::string const& description)
	{
		auto const normalizedPattern = TrimCopy(pattern);
		if (!ValidatePattern(normalizedPattern, matchType))
			return false;
		if (!Initialize())
			return false;

		bool changed = false;
		{
			std::lock_guard lock(m_mutex);
			constexpr char const* sql = R"(
				UPDATE OR IGNORE client_filter_rules
				SET pattern = ?, match_type = ?, case_sensitive = ?,
					enabled = ?, description = ?
				WHERE id = ?;
			)";
			sqlite3_stmt* statement = nullptr;
			if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) == SQLITE_OK)
			{
				sqlite3_bind_text(statement, 1, normalizedPattern.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(statement, 2, static_cast<int>(matchType));
				sqlite3_bind_int(statement, 3, caseSensitive ? 1 : 0);
				sqlite3_bind_int(statement, 4, enabled ? 1 : 0);
				sqlite3_bind_text(statement, 5, description.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_int64(statement, 6, id);
				changed = sqlite3_step(statement) == SQLITE_DONE &&
					sqlite3_changes(m_db) > 0;
				sqlite3_finalize(statement);
			}
			if (changed)
				LoadRulesLocked();
		}
		if (changed)
			ResetRuntimeBlocks();
		return changed;
	}

	void ClientFilterManager::RemoveRule(std::int64_t id)
	{
		if (!Initialize())
			return;
		{
			std::lock_guard lock(m_mutex);
			sqlite3_stmt* statement = nullptr;
			if (sqlite3_prepare_v2(
				m_db, "DELETE FROM client_filter_rules WHERE id = ?;",
				-1, &statement, nullptr) == SQLITE_OK)
			{
				sqlite3_bind_int64(statement, 1, id);
				sqlite3_step(statement);
				sqlite3_finalize(statement);
			}
			LoadRulesLocked();
		}
		ResetRuntimeBlocks();
	}

	void ClientFilterManager::SetRuleEnabled(std::int64_t id, bool enabled)
	{
		if (!Initialize())
			return;
		{
			std::lock_guard lock(m_mutex);
			sqlite3_stmt* statement = nullptr;
			if (sqlite3_prepare_v2(
				m_db,
				"UPDATE client_filter_rules SET enabled = ? WHERE id = ?;",
				-1, &statement, nullptr) == SQLITE_OK)
			{
				sqlite3_bind_int(statement, 1, enabled ? 1 : 0);
				sqlite3_bind_int64(statement, 2, id);
				sqlite3_step(statement);
				sqlite3_finalize(statement);
			}
			LoadRulesLocked();
		}
		ResetRuntimeBlocks();
	}

	void ClientFilterManager::ClearRules()
	{
		if (!Initialize())
			return;
		{
			std::lock_guard lock(m_mutex);
			sqlite3_exec(m_db, "DELETE FROM client_filter_rules;", nullptr, nullptr, nullptr);
			m_rules.clear();
		}
		ResetRuntimeBlocks();
	}

	std::vector<ClientFilterRule> ClientFilterManager::GetRules() const
	{
		const_cast<ClientFilterManager*>(this)->Initialize();
		std::lock_guard lock(m_mutex);
		return m_rules;
	}

	std::int32_t ClientFilterManager::GetRuleCount() const
	{
		const_cast<ClientFilterManager*>(this)->Initialize();
		std::lock_guard lock(m_mutex);
		return static_cast<std::int32_t>(m_rules.size());
	}

	std::vector<ClientFilterHit> ClientFilterManager::GetRecentHits(
		std::size_t limit) const
	{
		const_cast<ClientFilterManager*>(this)->Initialize();
		std::lock_guard lock(m_mutex);
		std::vector<ClientFilterHit> result;
		if (!m_db || limit == 0)
			return result;

		constexpr char const* sql = R"(
			SELECT h.id, h.rule_id,
				COALESCE(NULLIF(h.pattern, ''), r.pattern, ''),
				h.client, h.ip, h.task_name, h.timestamp
			FROM client_filter_hits h
			LEFT JOIN client_filter_rules r ON r.id = h.rule_id
			ORDER BY h.id DESC
			LIMIT ?;
		)";
		sqlite3_stmt* statement = nullptr;
		if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
			return result;
		sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(limit));
		while (sqlite3_step(statement) == SQLITE_ROW)
		{
			ClientFilterHit hit;
			hit.id = sqlite3_column_int64(statement, 0);
			hit.ruleId = sqlite3_column_int64(statement, 1);
			auto text = [&](int column)
			{
				auto value = reinterpret_cast<char const*>(
					sqlite3_column_text(statement, column));
				return std::string(value ? value : "");
			};
			hit.pattern = text(2);
			hit.client = text(3);
			hit.ip = text(4);
			hit.taskName = text(5);
			hit.timestamp = sqlite3_column_int64(statement, 6);
			result.push_back(std::move(hit));
		}
		sqlite3_finalize(statement);
		return result;
	}

	void ClientFilterManager::ClearHitHistory()
	{
		if (!Initialize())
			return;
		std::lock_guard lock(m_mutex);
		sqlite3_exec(m_db, "DELETE FROM client_filter_hits;", nullptr, nullptr, nullptr);
		sqlite3_exec(
			m_db,
			"UPDATE client_filter_rules SET hit_count = 0, last_hit = 0;",
			nullptr, nullptr, nullptr);
		LoadRulesLocked();
	}

	bool ClientFilterManager::IsEnabled() const
	{
		return AppSettingsDatabase::Instance()
			.GetBool(AppSettingsDatabase::CAT_APP, EnabledSetting)
			.value_or(false);
	}

	void ClientFilterManager::SetEnabled(bool enabled)
	{
		AppSettingsDatabase::Instance().SetBool(
			AppSettingsDatabase::CAT_APP, EnabledSetting, enabled);
		ResetRuntimeBlocks();
	}

	bool ClientFilterManager::WildcardMatches(std::string_view pattern,
											  std::string_view value)
	{
		std::size_t patternIndex = 0;
		std::size_t valueIndex = 0;
		std::size_t starIndex = std::string_view::npos;
		std::size_t retryValueIndex = 0;

		while (valueIndex < value.size())
		{
			if (patternIndex < pattern.size() &&
				(pattern[patternIndex] == '?' ||
				 pattern[patternIndex] == value[valueIndex]))
			{
				++patternIndex;
				++valueIndex;
			}
			else if (patternIndex < pattern.size() &&
					 pattern[patternIndex] == '*')
			{
				starIndex = patternIndex++;
				retryValueIndex = valueIndex;
			}
			else if (starIndex != std::string_view::npos)
			{
				patternIndex = starIndex + 1;
				valueIndex = ++retryValueIndex;
			}
			else
			{
				return false;
			}
		}
		while (patternIndex < pattern.size() && pattern[patternIndex] == '*')
			++patternIndex;
		return patternIndex == pattern.size();
	}

	bool ClientFilterManager::PatternMatches(ClientFilterRule const& rule,
											 std::string const& client)
	{
		auto pattern = rule.pattern;
		auto value = client;
		if (!rule.caseSensitive)
		{
			pattern = LowerAscii(std::move(pattern));
			value = LowerAscii(std::move(value));
		}

		switch (rule.matchType)
		{
			case ClientMatchType::Exact:
				return value == pattern;
			case ClientMatchType::Wildcard:
				return WildcardMatches(pattern, value);
			case ClientMatchType::Regex:
				try
				{
					auto const flags = rule.caseSensitive
						? std::regex_constants::ECMAScript
						: std::regex_constants::ECMAScript |
						std::regex_constants::icase;
					return std::regex_search(client, std::regex(rule.pattern, flags));
				}
				catch (std::regex_error const&)
				{
					return false;
				}
			case ClientMatchType::Contains:
			default:
				return value.find(pattern) != std::string::npos;
		}
	}

	std::optional<ClientFilterRule> ClientFilterManager::MatchClientLocked(
		std::string const& client) const
	{
		for (auto const& rule : m_rules)
		{
			if (rule.enabled && PatternMatches(rule, client))
				return rule;
		}
		return std::nullopt;
	}

	std::optional<ClientFilterRule> ClientFilterManager::MatchClient(
		std::string const& client) const
	{
		const_cast<ClientFilterManager*>(this)->Initialize();
		std::lock_guard lock(m_mutex);
		return MatchClientLocked(client);
	}

	void ClientFilterManager::EvaluatePeers(
		std::vector<ClientPeerObservation> const& peers)
	{
		if (!IsEnabled() || peers.empty() || !Initialize())
			return;

		bool changed = false;
		{
			std::lock_guard lock(m_mutex);
			if (!IsEnabled())
				return;
			auto const now = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
			std::vector<std::pair<ClientPeerObservation, ClientFilterRule>>
				matches;

			for (auto const& peer : peers)
			{
				if (peer.client.empty() || peer.ip.empty())
					continue;
				auto const rule = MatchClientLocked(peer.client);
				if (!rule || !m_runtimeBlockedAddresses.insert(peer.ip).second)
					continue;

				changed = true;
				matches.emplace_back(peer, *rule);
			}

			if (changed)
			{
				sqlite3_exec(
					m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
				sqlite3_stmt* update = nullptr;
				sqlite3_stmt* insert = nullptr;
				sqlite3_prepare_v2(
					m_db,
					"UPDATE client_filter_rules "
					"SET hit_count = hit_count + 1, last_hit = ? WHERE id = ?;",
					-1, &update, nullptr);
				sqlite3_prepare_v2(
					m_db,
					"INSERT INTO client_filter_hits("
					"rule_id, pattern, client, ip, task_name, timestamp) "
					"VALUES(?, ?, ?, ?, ?, ?);",
					-1, &insert, nullptr);

				for (auto const& [peer, rule] : matches)
				{
					if (update)
					{
						sqlite3_reset(update);
						sqlite3_clear_bindings(update);
						sqlite3_bind_int64(update, 1, now);
						sqlite3_bind_int64(update, 2, rule.id);
						sqlite3_step(update);
					}
					if (insert)
					{
						sqlite3_reset(insert);
						sqlite3_clear_bindings(insert);
						sqlite3_bind_int64(insert, 1, rule.id);
						sqlite3_bind_text(insert, 2, rule.pattern.c_str(), -1, SQLITE_TRANSIENT);
						sqlite3_bind_text(insert, 3, peer.client.c_str(), -1, SQLITE_TRANSIENT);
						sqlite3_bind_text(insert, 4, peer.ip.c_str(), -1, SQLITE_TRANSIENT);
						sqlite3_bind_text(insert, 5, peer.taskName.c_str(), -1, SQLITE_TRANSIENT);
						sqlite3_bind_int64(insert, 6, now);
						sqlite3_step(insert);
					}
				}
				if (update)
					sqlite3_finalize(update);
				if (insert)
					sqlite3_finalize(insert);
				sqlite3_exec(
					m_db,
					"DELETE FROM client_filter_hits WHERE id NOT IN ("
					"SELECT id FROM client_filter_hits ORDER BY id DESC LIMIT 500);",
					nullptr, nullptr, nullptr);
				sqlite3_exec(
					m_db, "COMMIT;", nullptr, nullptr, nullptr);
				LoadRulesLocked();
			}
		}

		if (changed)
			ApplyRuntimeBlocks();
	}

	void ClientFilterManager::ResetRuntimeBlocks()
	{
		{
			std::lock_guard lock(m_mutex);
			m_runtimeBlockedAddresses.clear();
		}
		ApplyRuntimeBlocks();
	}

	void ClientFilterManager::ApplyRuntimeBlocks()
	{
		// Serialize filter application and take a fresh snapshot after acquiring
		// the apply lock. This prevents a late peer scan from restoring addresses
		// cleared by a simultaneous rule edit or global disable.
		std::lock_guard applyLock(m_applyMutex);
		std::vector<std::string> blockedAddresses;
		{
			std::lock_guard lock(m_mutex);
			blockedAddresses.assign(
				m_runtimeBlockedAddresses.begin(),
				m_runtimeBlockedAddresses.end());
		}
		auto& ipFilter = IPFilterManager::Instance();
		ipFilter.SetClientBlockedAddresses(blockedAddresses);
		ipFilter.ApplyToSession();
	}

	std::size_t ClientFilterManager::RuntimeBlockedCount() const
	{
		std::lock_guard lock(m_mutex);
		return m_runtimeBlockedAddresses.size();
	}

	std::string ClientFilterManager::ExportRules() const
	{
		auto const rules = GetRules();
		nlohmann::json root;
		root["version"] = 1;
		root["rules"] = nlohmann::json::array();
		for (auto const& rule : rules)
		{
			root["rules"].push_back({
				{ "pattern", rule.pattern },
				{ "match_type", static_cast<int>(rule.matchType) },
				{ "case_sensitive", rule.caseSensitive },
				{ "enabled", rule.enabled },
				{ "description", rule.description },
									});
		}
		return root.dump(2);
	}

	std::int32_t ClientFilterManager::ImportRules(
		std::string const& content, bool replaceExisting)
	{
		if (!Initialize())
			return 0;

		auto normalizedContent = content;
		if (normalizedContent.starts_with("\xEF\xBB\xBF"))
			normalizedContent.erase(0, 3);

		std::vector<ClientFilterRule> parsed;
		auto json = nlohmann::json::parse(
			normalizedContent, nullptr, false);
		if (!json.is_discarded())
		{
			auto const* rules = json.is_array()
				? &json
				: (json.is_object() && json.contains("rules") &&
				   json["rules"].is_array() ? &json["rules"] : nullptr);
			if (rules)
			{
				for (auto const& item : *rules)
				{
					if (!item.is_object())
						continue;
					ClientFilterRule rule;
					rule.pattern = TrimCopy(item.value("pattern", ""));
					rule.matchType = NormalizeMatchType(item.value("match_type", 0));
					rule.caseSensitive = item.value("case_sensitive", false);
					rule.enabled = item.value("enabled", true);
					rule.description = item.value("description", "");
					if (ValidatePattern(rule.pattern, rule.matchType))
						parsed.push_back(std::move(rule));
				}
			}
		}
		else
		{
			std::istringstream stream(normalizedContent);
			std::string line;
			while (std::getline(stream, line))
			{
				line = TrimCopy(std::move(line));
				if (line.empty() || line.front() == '#' || line.front() == ';')
					continue;
				ClientFilterRule rule;
				rule.pattern = std::move(line);
				if (ValidatePattern(rule.pattern, rule.matchType))
					parsed.push_back(std::move(rule));
			}
		}

		if (parsed.empty())
			return 0;

		std::int32_t imported = 0;
		{
			std::lock_guard lock(m_mutex);
			sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
			if (replaceExisting)
				sqlite3_exec(m_db, "DELETE FROM client_filter_rules;", nullptr, nullptr, nullptr);

			constexpr char const* sql = R"(
				INSERT OR IGNORE INTO client_filter_rules(
					pattern, match_type, case_sensitive, enabled, description)
				VALUES(?, ?, ?, ?, ?);
			)";
			sqlite3_stmt* statement = nullptr;
			if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) == SQLITE_OK)
			{
				for (auto const& rule : parsed)
				{
					sqlite3_reset(statement);
					sqlite3_clear_bindings(statement);
					sqlite3_bind_text(statement, 1, rule.pattern.c_str(), -1, SQLITE_TRANSIENT);
					sqlite3_bind_int(statement, 2, static_cast<int>(rule.matchType));
					sqlite3_bind_int(statement, 3, rule.caseSensitive ? 1 : 0);
					sqlite3_bind_int(statement, 4, rule.enabled ? 1 : 0);
					sqlite3_bind_text(statement, 5, rule.description.c_str(), -1, SQLITE_TRANSIENT);
					if (sqlite3_step(statement) == SQLITE_DONE &&
						sqlite3_changes(m_db) > 0)
					{
						++imported;
					}
				}
				sqlite3_finalize(statement);
			}
			sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
			LoadRulesLocked();
		}
		ResetRuntimeBlocks();
		return imported;
	}
}
