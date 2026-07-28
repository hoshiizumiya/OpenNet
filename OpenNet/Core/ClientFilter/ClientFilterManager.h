/*
 * PROJECT:   OpenNet
 * FILE:      Core/ClientFilter/ClientFilterManager.h
 * PURPOSE:   Persistent BitTorrent peer-client filtering and hit history.
 *
 * LICENSE:   The MIT License
 */

#pragma once

import std;

struct sqlite3;

namespace OpenNet::Core
{
	enum class ClientMatchType : std::int32_t
	{
		Contains = 0,
		Exact = 1,
		Wildcard = 2,
		Regex = 3,
	};

	struct ClientFilterRule
	{
		std::int64_t id{};
		std::string pattern;
		ClientMatchType matchType{ ClientMatchType::Contains };
		bool caseSensitive{};
		bool enabled{ true };
		std::string description;
		std::int64_t hitCount{};
		std::int64_t lastHitTimestamp{};
	};

	struct ClientFilterHit
	{
		std::int64_t id{};
		std::int64_t ruleId{};
		std::string pattern;
		std::string client;
		std::string ip;
		std::string taskName;
		std::int64_t timestamp{};
	};

	struct ClientPeerObservation
	{
		std::string client;
		std::string ip;
		std::string taskName;
	};

	class ClientFilterManager
	{
	public:
		static ClientFilterManager& Instance();

		bool Initialize();
		void Close();

		bool AddRule(std::string const& pattern, ClientMatchType matchType,
					 bool caseSensitive, std::string const& description = {});
		bool UpdateRule(std::int64_t id, std::string const& pattern,
						ClientMatchType matchType, bool caseSensitive, bool enabled,
						std::string const& description);
		void RemoveRule(std::int64_t id);
		void SetRuleEnabled(std::int64_t id, bool enabled);
		void ClearRules();

		std::vector<ClientFilterRule> GetRules() const;
		std::int32_t GetRuleCount() const;

		std::vector<ClientFilterHit> GetRecentHits(std::size_t limit = 200) const;
		void ClearHitHistory();

		bool IsEnabled() const;
		void SetEnabled(bool enabled);

		static bool ValidatePattern(std::string const& pattern,
									ClientMatchType matchType, std::string* error = nullptr);
		std::optional<ClientFilterRule> MatchClient(
			std::string const& client) const;

		/// Evaluate a peer snapshot. Newly matched IP addresses are added to
		/// the session's transient IP filter and existing connections are closed
		/// by libtorrent when that filter is applied.
		void EvaluatePeers(std::vector<ClientPeerObservation> const& peers);
		void ResetRuntimeBlocks();
		std::size_t RuntimeBlockedCount() const;

		/// JSON is used for full-fidelity import/export. A plain text fallback
		/// treats every non-comment line as a case-insensitive "contains" rule.
		std::string ExportRules() const;
		std::int32_t ImportRules(std::string const& content, bool replaceExisting);

	private:
		ClientFilterManager() = default;
		~ClientFilterManager();
		ClientFilterManager(ClientFilterManager const&) = delete;
		ClientFilterManager& operator=(ClientFilterManager const&) = delete;

		void CreateTables();
		void LoadRulesLocked();
		std::optional<ClientFilterRule> MatchClientLocked(
			std::string const& client) const;
		void ApplyRuntimeBlocks();
		static bool PatternMatches(ClientFilterRule const& rule,
								   std::string const& client);
		static bool WildcardMatches(std::string_view pattern,
									std::string_view value);

		mutable std::mutex m_mutex;
		std::mutex m_applyMutex;
		sqlite3* m_db{};
		bool m_initialized{};
		std::vector<ClientFilterRule> m_rules;
		std::unordered_set<std::string> m_runtimeBlockedAddresses;
	};
}
