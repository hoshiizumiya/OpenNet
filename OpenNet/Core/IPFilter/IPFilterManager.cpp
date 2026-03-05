/*
 * PROJECT:   OpenNet
 * FILE:      Core/IPFilter/IPFilterManager.cpp
 * PURPOSE:   IP filter management implementation.
 *
 * LICENSE:   The MIT License
 */

#include "pch.h"
#include "Core/IPFilter/IPFilterManager.h"
#include "Core/AppSettingsDatabase.h"
#include "Core/P2PManager.h"
#include "ThirdParty/Sqlite/sqlite3.h"

#include <libtorrent/ip_filter.hpp>
#include <libtorrent/address.hpp>

#include <winrt/Windows.Storage.h>

#include <sstream>
#include <algorithm>

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
        std::lock_guard lk(m_mutex);
        if (m_initialized) return true;

        try
        {
            auto localFolder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            auto dbPath = std::wstring(localFolder.Path().c_str()) + L"\\ipfilter.db";

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
            return true;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("IPFilterManager::Initialize error: " +
                std::string(ex.what()) + "\n").c_str());
            return false;
        }
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

    // ---------------------------------------------------------------
    //  Rule CRUD
    // ---------------------------------------------------------------

    void IPFilterManager::AddRule(std::string const& firstIp, std::string const& lastIp,
                                  uint32_t flags, std::string const& description)
    {
        std::lock_guard lk(m_mutex);
        if (!m_db) return;

        const char* sql = "INSERT INTO ip_rules(first_ip, last_ip, flags, description) VALUES(?, ?, ?, ?);";
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
                rule.id    = sqlite3_column_int64(stmt, 0);
                auto f = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                auto l = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                rule.flags = static_cast<uint32_t>(sqlite3_column_int(stmt, 3));
                auto d = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                if (f) rule.firstIp = f;
                if (l) rule.lastIp  = l;
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
            try { prefix = std::stoi(input.substr(slashPos + 1)); }
            catch (...) { return false; }

            boost::system::error_code ec;
            auto addr = lt::make_address(addrStr, ec);
            if (ec) return false;

            if (addr.is_v4())
            {
                if (prefix < 0 || prefix > 32) return false;
                auto bytes = addr.to_v4().to_bytes();

                auto firstBytes = bytes;
                auto lastBytes  = bytes;
                for (int i = prefix; i < 32; ++i)
                {
                    int byteIdx = i / 8;
                    int bitIdx  = 7 - (i % 8);
                    firstBytes[byteIdx] &= static_cast<unsigned char>(~(1u << bitIdx));
                    lastBytes[byteIdx]  |= static_cast<unsigned char>(1u << bitIdx);
                }
                outFirst = lt::address_v4(firstBytes).to_string();
                outLast  = lt::address_v4(lastBytes).to_string();
                return true;
            }
            else // IPv6
            {
                if (prefix < 0 || prefix > 128) return false;
                auto bytes = addr.to_v6().to_bytes();

                auto firstBytes = bytes;
                auto lastBytes  = bytes;
                for (int i = prefix; i < 128; ++i)
                {
                    int byteIdx = i / 8;
                    int bitIdx  = 7 - (i % 8);
                    firstBytes[byteIdx] &= static_cast<unsigned char>(~(1u << bitIdx));
                    lastBytes[byteIdx]  |= static_cast<unsigned char>(1u << bitIdx);
                }
                outFirst = lt::address_v6(firstBytes).to_string();
                outLast  = lt::address_v6(lastBytes).to_string();
                return true;
            }
        }

        // ---- IPv4 range notation (e.g. "1.2.3.0-1.2.3.255") ----
        // Only consider dash as range separator when there are no colons
        // (to avoid confusing IPv6 notation).
        auto dashPos = input.find('-');
        if (dashPos != std::string::npos && input.find(':') == std::string::npos)
        {
            auto firstStr = input.substr(0, dashPos);
            auto lastStr  = input.substr(dashPos + 1);

            boost::system::error_code ec1, ec2;
            auto first = lt::make_address(firstStr, ec1);
            auto last  = lt::make_address(lastStr, ec2);
            if (ec1 || ec2) return false;
            if (first.is_v4() != last.is_v4()) return false;

            outFirst = first.to_string();
            outLast  = last.to_string();
            return true;
        }

        // ---- Single IP address ----
        boost::system::error_code ec;
        auto addr = lt::make_address(input, ec);
        if (ec) return false;

        outFirst = addr.to_string();
        outLast  = outFirst;
        return true;
    }

    // ---------------------------------------------------------------
    //  Bulk import
    // ---------------------------------------------------------------

    int IPFilterManager::ImportFromText(std::string const& text)
    {
        // ---- Phase 1: parse on current thread (no lock) ----
        struct ParsedRule { std::string first, last, desc; };
        std::vector<ParsedRule> parsed;

        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line))
        {
            // Trim
            auto s = line.find_first_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            auto e = line.find_last_not_of(" \t\r\n");
            line = line.substr(s, e - s + 1);

            // Skip comments / empty
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            ParsedRule r;
            r.desc = line;
            if (ParseIPOrCIDR(line, r.first, r.last))
                parsed.push_back(std::move(r));
        }

        if (parsed.empty()) return 0;

        // ---- Phase 2: bulk insert inside transaction ----
        std::lock_guard lk(m_mutex);
        if (!m_db) return 0;

        sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        const char* sql = "INSERT INTO ip_rules(first_ip, last_ip, flags, description) VALUES(?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        int count = 0;

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
        {
            for (auto const& r : parsed)
            {
                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, r.first.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, r.last.c_str(),  -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt, 3, static_cast<int>(lt::ip_filter::blocked));
                sqlite3_bind_text(stmt, 4, r.desc.c_str(),  -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_DONE) ++count;
            }
            sqlite3_finalize(stmt);
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
            auto last  = lt::make_address(rule.lastIp, ec2);
            if (!ec1 && !ec2)
            {
                filter.add_rule(first, last, rule.flags);
            }
        }
        return filter;
    }

    void IPFilterManager::ApplyToSession()
    {
        auto* core = P2PManager::Instance().TorrentCore();
        if (!core || !core->IsRunning()) return;

        if (!IsEnabled())
        {
            // Apply an empty (allow-all) filter.
            core->SetIpFilter(lt::ip_filter{});
            return;
        }

        auto filter = BuildFilter();
        core->SetIpFilter(filter);
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

} // namespace OpenNet::Core
