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

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct sqlite3;

namespace libtorrent { struct ip_filter; }

namespace OpenNet::Core
{
    /// A single IP filter rule persisted in the database.
    struct IPRule
    {
        int64_t  id{};
        std::string firstIp;       // start of range (inclusive)
        std::string lastIp;        // end of range (inclusive)
        uint32_t    flags{ 1 };    // 0 = allowed, 1 = blocked (ip_filter::blocked)
        std::string description;   // original input or user comment
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
                     uint32_t flags = 1, std::string const& description = "");

        void RemoveRule(int64_t id);

        std::vector<IPRule> GetAllRules() const;

        int  GetRuleCount() const;

        void ClearAllRules();

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
        int ImportFromText(std::string const& text);

        // ---------------------------------------------------------------
        //  Session integration
        // ---------------------------------------------------------------

        /// Build an lt::ip_filter from all stored rules.
        libtorrent::ip_filter BuildFilter() const;

        /// Build the filter and apply it to the running libtorrent session.
        /// If the filter is disabled, an empty (allow-all) filter is applied.
        void ApplyToSession();

        /// Whether the IP filter is globally enabled.
        bool IsEnabled() const;
        void SetEnabled(bool enabled);

    private:
        IPFilterManager() = default;
        ~IPFilterManager();
        IPFilterManager(IPFilterManager const&) = delete;
        IPFilterManager& operator=(IPFilterManager const&) = delete;

        void CreateTables();

        mutable std::mutex m_mutex;
        sqlite3* m_db{ nullptr };
        bool m_initialized{ false };
    };

} // namespace OpenNet::Core
