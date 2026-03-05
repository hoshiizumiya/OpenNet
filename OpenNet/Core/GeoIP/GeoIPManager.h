#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <unordered_map>

namespace OpenNet::Core
{
    /// Lightweight IP2Location-based GeoIP manager.
    /// Loads the free IP2Location LITE DB1 CSV (ip_from, ip_to, country_code, country_name)
    /// and the multilingual CSV (for translated country names).
    ///
    /// Expected file layout:
    ///   Assets/IP2Location/IP2LOCATION-LITE-DB1.CSV        (IP range → country)
    ///   Assets/IP2Location/IP2LOCATION-COUNTRY-MULTILINGUAL.CSV  (country name translations)
    ///
    /// If the DB1 CSV is not present, all lookups return empty string.
    class GeoIPManager
    {
    public:
        static GeoIPManager& Instance();

        /// Initialize: load CSV files from the given asset directory.
        /// @param assetsDir  absolute path to the directory containing the CSV files
        ///                   (e.g. "Assets/IP2Location" inside the package)
        void Initialize(std::wstring const& assetsDir = L"");

        /// Look up country code (e.g. "US") for an IPv4 address string.
        /// Returns empty string if not found or database not loaded.
        std::string LookupCountryCode(std::string const& ipAddress) const;

        /// Look up display country name for an IPv4 address string.
        /// Uses multilingual CSV if available, otherwise falls back to English name from DB1.
        std::string LookupCountryName(std::string const& ipAddress) const;

        bool IsLoaded() const { return m_loaded; }

    private:
        GeoIPManager() = default;
        ~GeoIPManager() = default;
        GeoIPManager(GeoIPManager const&) = delete;
        GeoIPManager& operator=(GeoIPManager const&) = delete;

        struct IPRange
        {
            uint32_t ipFrom{};
            uint32_t ipTo{};
            std::string countryCode;    // 2-letter code
            std::string countryName;    // English name from DB1
        };

        bool LoadDB1(std::wstring const& filePath);
        bool LoadMultilingual(std::wstring const& filePath);

        static uint32_t IPv4ToUint32(std::string const& ip);
        IPRange const* FindRange(uint32_t ipNum) const;

        std::vector<IPRange> m_ranges;  // sorted by ipFrom
        // countryCode → translated name (for preferred language)
        std::unordered_map<std::string, std::string> m_countryNames;
        bool m_loaded{ false };
        mutable std::mutex m_mutex;
    };
}
