/*
 * PROJECT:   OpenNet
 * FILE:      Core/AppSettingsDatabase.h
 * PURPOSE:   Unified SQLite-based application settings storage.
 *            Replaces separate JSON files for torrent settings,
 *            HTTP state, and application preferences.
 *
 * SCHEMA:    settings(key TEXT PRIMARY KEY, value TEXT, category TEXT)
 *            Stores all key-value settings with an optional category tag.
 *
 * LICENSE:   The MIT License
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;

namespace OpenNet::Core
{
    /// A single setting entry
    struct SettingEntry
    {
        std::string key;
        std::string value;
        std::string category;
    };

    /// Unified application settings database (SQLite).
    /// Thread-safe singleton. All settings are stored as key-value pairs
    /// grouped by category for logical separation.
    class AppSettingsDatabase
    {
    public:
        static AppSettingsDatabase& Instance();

        /// Initialize the database (creates tables if needed)
        bool Initialize();

        /// Close the database connection
        void Close();

        // ---------------------------------------------------------------
        //  Generic key-value operations
        // ---------------------------------------------------------------

        /// Set a string setting
        void SetString(std::string const& category, std::string const& key, std::string const& value);

        /// Get a string setting
        std::optional<std::string> GetString(std::string const& category, std::string const& key) const;

        /// Set an integer setting
        void SetInt(std::string const& category, std::string const& key, int64_t value);

        /// Get an integer setting
        std::optional<int64_t> GetInt(std::string const& category, std::string const& key) const;

        /// Get an integer setting with default value
        int64_t GetInt(std::string const& category, std::string const& key, int64_t defaultValue) const
        {
            return GetInt(category, key).value_or(defaultValue);
        }

        /// Set a double setting
        void SetDouble(std::string const& category, std::string const& key, double value);

        /// Get a double setting
        std::optional<double> GetDouble(std::string const& category, std::string const& key) const;

        /// Set a boolean setting
        void SetBool(std::string const& category, std::string const& key, bool value);

        /// Get a boolean setting
        std::optional<bool> GetBool(std::string const& category, std::string const& key) const;

        /// Delete a setting
        void Delete(std::string const& category, std::string const& key);

        /// Get all settings in a category
        std::vector<SettingEntry> GetCategory(std::string const& category) const;

        /// Delete all settings in a category
        void DeleteCategory(std::string const& category);

        // ---------------------------------------------------------------
        //  Batch operations (within a transaction for performance)
        // ---------------------------------------------------------------

        /// Set multiple settings atomically
        void SetBatch(std::vector<SettingEntry> const& entries);

        /// Get all settings as a map grouped by "category.key"
        std::unordered_map<std::string, std::string> GetAll() const;

        // ---------------------------------------------------------------
        //  Wide-string convenience methods (UTF-16)
        // ---------------------------------------------------------------

        /// Set a wide-string setting
        void SetStringW(std::string const& category, std::string const& key, std::wstring_view const& value);

        /// Get a wide-string setting
        std::optional<std::wstring_view> GetStringW(std::string const& category, std::string const& key) const;

        // ---------------------------------------------------------------
        //  Category constants for well-known setting groups
        // ---------------------------------------------------------------
        static constexpr const char* CAT_TORRENT      = "torrent";
        static constexpr const char* CAT_CONNECTION    = "connection";
        static constexpr const char* CAT_DOWNLOAD      = "download";
        static constexpr const char* CAT_PROXY         = "proxy";
        static constexpr const char* CAT_ENCRYPTION    = "encryption";
        static constexpr const char* CAT_SEEDING       = "seeding";
        static constexpr const char* CAT_DISCOVERY     = "discovery";
        static constexpr const char* CAT_TRACKER       = "tracker";
        static constexpr const char* CAT_SPEED_LIMIT   = "speed_limit";
        static constexpr const char* CAT_DISK_IO       = "disk_io";
        static constexpr const char* CAT_IDENTITY      = "identity";
        static constexpr const char* CAT_UI            = "ui";
        static constexpr const char* CAT_APP           = "app";
        static constexpr const char* CAT_COLUMN_WIDTH  = "column_width";
        static constexpr const char* CAT_RSS           = "rss";

    private:
        AppSettingsDatabase() = default;
        ~AppSettingsDatabase();
        AppSettingsDatabase(AppSettingsDatabase const&) = delete;
        AppSettingsDatabase& operator=(AppSettingsDatabase const&) = delete;

        void CreateTables();
        bool EnsureInitialized();

        mutable std::recursive_mutex m_mutex;
        sqlite3* m_db{ nullptr };
        bool m_initialized{ false };
    };

} // namespace OpenNet::Core
