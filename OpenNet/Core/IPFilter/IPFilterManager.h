/*
 * PROJECT:   OpenNet
 * FILE:      Core/IPFilter/IPFilterManager.h
 * PURPOSE:   IP filter management using libtorrent's ip_filter.
 *            Stores rules in a dedicated SQLite database (ipfilter.db).
 *            Supports IPv4/IPv6 addresses, CIDR notation, and IP ranges.
 *
 * LICENSE:   The MIT License
 */

#pragma once

import std;

struct sqlite3;

namespace libtorrent
{
	struct ip_filter;
}

namespace OpenNet::Core
{
	/// A single IP filter rule persisted in the database.
	struct IPRule
	{
		std::int64_t id{};
		std::string firstIp;       // start of range (inclusive)
		std::string lastIp;        // end of range (inclusive)
		std::uint32_t flags{ 1 };    // 0 = allowed, 1 = blocked (ip_filter::blocked)
		std::string description;   // original input or user comment
	};

	struct IPBanEntry
	{
		std::int64_t id{};
		std::string ip;
		std::int32_t port{};
		std::string taskId;
		std::string client;
		std::string source;
		std::string reason;
		std::int64_t createdAt{};
		std::int64_t expiresAt{}; // 0 means permanent
	};

	struct IPFilterSubscription
	{
		std::int64_t id{};
		std::string url;
		bool enabled{ true };
		std::int64_t lastUpdated{};
		std::string lastStatus;
		std::int32_t ruleCount{};
		std::string lastError;
	};

	/// Manages IP filter rules stored in SQLite and applies them to
	/// the libtorrent session via ip_filter.
	/// Thread-safe singleton.
	class IPFilterManager
	{
	public:
		static IPFilterManager& Instance();

		/// Open / create ipfilter.db in LocalFolder.
		bool Initialize();

		/// Close the database connection.
		void Close();

		// ---------------------------------------------------------------
		//  Rule CRUD
		// ---------------------------------------------------------------

		void AddRule(std::string const& firstIp, std::string const& lastIp,
					 std::uint32_t flags = 1, std::string const& description = "");

		/// Update an existing persisted rule. Returns false if the rule does not
		/// exist or the new range duplicates another rule.
		bool UpdateRule(std::int64_t id, std::string const& firstIp,
						std::string const& lastIp, std::uint32_t flags,
						std::string const& description);

		void RemoveRule(std::int64_t id);

		std::vector<IPRule> GetAllRules() const;

		std::int32_t GetRuleCount() const;

		void ClearAllRules();

		// Subscription sources and their update metadata.
		std::int64_t AddSubscription(std::string const& url);
		bool UpdateSubscription(std::int64_t id, std::string const& url, bool enabled);
		void RemoveSubscription(std::int64_t id);
		void SetSubscriptionEnabled(std::int64_t id, bool enabled);
		std::vector<IPFilterSubscription> GetSubscriptions() const;
		void SetSubscriptionUpdateResult(
			std::int64_t id,
			std::int64_t updatedAt,
			bool succeeded,
			std::int32_t ruleCount,
			std::string const& error);

		bool SubscriptionAutoUpdateEnabled() const;
		void SubscriptionAutoUpdateEnabled(bool value);
		bool SubscriptionReplaceExisting() const;
		void SubscriptionReplaceExisting(bool value);
		std::int32_t SubscriptionUpdateIntervalHours() const;
		void SubscriptionUpdateIntervalHours(std::int32_t value);
		std::int64_t SubscriptionLastUpdate() const;
		std::string SubscriptionLastResult() const;
		void SetSubscriptionLastResult(
			std::int64_t updatedAt, std::string const& result);
		bool IsSubscriptionUpdateDue(std::int64_t now) const;

		/// Add or replace an address ban from the same source. Timed bans are
		/// stored separately from imported IP lists so expiry cannot destroy an
		/// overlapping list rule.
		std::int64_t AddBan(
			std::string const& ip,
			std::int32_t port,
			std::string const& taskId,
			std::string const& client,
			std::string const& source,
			std::string const& reason,
			std::int64_t expiresAt = 0);
		bool RemoveBan(
			std::string const& ip,
			std::string const& source = {});
		std::vector<IPBanEntry> GetActiveBans(
			std::string const& taskId = {}) const;
		std::optional<IPBanEntry> FindActiveBan(
			std::string const& ip) const;
		std::optional<IPRule> FindMatchingRule(
			std::string const& ip) const;
		std::vector<std::optional<IPRule>> FindMatchingRules(
			std::vector<std::string> const& addresses) const;
		void MaintainTemporaryBans();

		// ---------------------------------------------------------------
		//  Parsing helpers
		// ---------------------------------------------------------------

		/// Parse a single IP, CIDR block, or "first-last" range string
		/// into the first/last addresses of the range.
		/// Returns false if the input cannot be parsed.
		static bool ParseIPOrCIDR(std::string const& input,
								  std::string& outFirst,
								  std::string& outLast);

		// ---------------------------------------------------------------
		//  Bulk import
		// ---------------------------------------------------------------

		/// Import rules from multi-line text (one entry per line).
		/// Lines starting with '#' or ';' are treated as comments.
		/// Returns the number of rules successfully imported.
		std::int32_t ImportFromText(std::string const& text, bool replaceExisting = false);
		static std::int32_t CountRulesInText(std::string const& text);

		// ---------------------------------------------------------------
		//  Session integration
		// ---------------------------------------------------------------

		/// Build an lt::ip_filter from all stored rules.
		libtorrent::ip_filter BuildFilter() const;
		/// Build the complete runtime filter, including temporary bans and
		/// client-name blocks. This type remains in the conventional C++ layer.
		libtorrent::ip_filter BuildSessionFilter() const;

		/// Build the filter and apply it to the running libtorrent session.
		/// Persisted rules respect the IP-filter switch. Runtime addresses
		/// produced by the client filter are merged independently.
		void ApplyToSession();

		/// Whether the IP filter is globally enabled.
		bool IsEnabled() const;
		void SetEnabled(bool enabled);

		/// Replace the session-only addresses blocked by client-name rules.
		/// These addresses are deliberately not persisted in ipfilter.db.
		void SetClientBlockedAddresses(std::vector<std::string> const& addresses);

	private:
		IPFilterManager() = default;
		~IPFilterManager();
		IPFilterManager(IPFilterManager const&) = delete;
		IPFilterManager& operator=(IPFilterManager const&) = delete;

		void CreateTables();
		void SeedBundledRules();
		std::vector<IPBanEntry> GetActiveBansLocked(std::string const& taskId) const;
		struct RuleLookupSnapshot;
		std::shared_ptr<RuleLookupSnapshot const> GetRuleLookupSnapshot() const;
		void InvalidateRuleLookupLocked();

		mutable std::mutex m_mutex;
		mutable std::mutex m_clientBlockedMutex;
		std::mutex m_applyMutex;
		std::once_flag m_seedOnce;
		sqlite3* m_db{ nullptr };
		bool m_initialized{ false };
		mutable std::shared_ptr<RuleLookupSnapshot const> m_ruleLookupSnapshot;
		std::unordered_set<std::string> m_clientBlockedAddresses;
	};

} // namespace OpenNet::Core
