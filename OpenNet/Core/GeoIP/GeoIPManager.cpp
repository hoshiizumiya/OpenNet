#include "pch.h"
#include "GeoIPManager.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
#include <filesystem>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Globalization.h>

namespace OpenNet::Core
{
    GeoIPManager& GeoIPManager::Instance()
    {
        static GeoIPManager instance;
        return instance;
    }

    void GeoIPManager::Initialize(std::wstring const& assetsDir)
    {
        std::lock_guard lk(m_mutex);
        if (m_loaded) return;

        std::wstring dir = assetsDir;
        if (dir.empty())
        {
            try
            {
                auto installPath = winrt::Windows::ApplicationModel::Package::Current().InstalledLocation().Path();
                dir = std::wstring(installPath.c_str()) + L"\\Assets\\IP2Location";
            }
            catch (...)
            {
                OutputDebugStringA("GeoIPManager::Initialize: Failed to get installed location\n");
                return;
            }
        }

        auto db1Path = dir + L"\\IP2LOCATION-LITE-DB1.CSV";
        auto multiPath = dir + L"\\IP2LOCATION-COUNTRY-MULTILINGUAL.CSV";

        // Load multilingual first (optional)
        LoadMultilingual(multiPath);

        // Load DB1 (required for lookups)
        if (LoadDB1(db1Path))
        {
            m_loaded = true;
            OutputDebugStringW((L"GeoIPManager: Loaded " + std::to_wstring(m_ranges.size()) + L" IP ranges\n").c_str());
        }
        else
        {
            OutputDebugStringA("GeoIPManager: IP2LOCATION-LITE-DB1.CSV not found or empty. GeoIP lookups disabled.\n");
        }
    }

    // Parse a CSV-quoted field: "value" or unquoted value
    static std::string ParseCSVField(std::istringstream& ss)
    {
        std::string result;
        char c;
        if (ss.peek() == '"')
        {
            ss.get(); // consume opening "
            std::getline(ss, result, '"');
            if (ss.peek() == ',') ss.get(); // consume trailing comma
        }
        else
        {
            std::getline(ss, result, ',');
        }
        return result;
    }

    bool GeoIPManager::LoadDB1(std::wstring const& filePath)
    {
        // Check file exists
        if (!std::filesystem::exists(filePath))
            return false;

        std::ifstream file(filePath);
        if (!file.is_open())
            return false;

        std::string line;
        m_ranges.clear();
        m_ranges.reserve(200000); // typical DB1 has ~200k entries

        while (std::getline(file, line))
        {
            if (line.empty()) continue;

            // Format: "ip_from","ip_to","country_code","country_name"
            std::istringstream ss(line);
            auto ipFromStr = ParseCSVField(ss);
            auto ipToStr = ParseCSVField(ss);
            auto countryCode = ParseCSVField(ss);
            auto countryName = ParseCSVField(ss);

            if (ipFromStr.empty() || ipToStr.empty() || countryCode.empty())
                continue;

            // Skip header row
            if (ipFromStr == "ip_from" || ipFromStr == "0")
                continue;

            try
            {
                IPRange range;
                range.ipFrom = static_cast<uint32_t>(std::stoul(ipFromStr));
                range.ipTo = static_cast<uint32_t>(std::stoul(ipToStr));
                range.countryCode = countryCode;
                range.countryName = countryName;
                m_ranges.push_back(std::move(range));
            }
            catch (...) { continue; }
        }

        // Sort by ipFrom for binary search
        std::sort(m_ranges.begin(), m_ranges.end(),
            [](IPRange const& a, IPRange const& b) { return a.ipFrom < b.ipFrom; });

        return !m_ranges.empty();
    }

    bool GeoIPManager::LoadMultilingual(std::wstring const& filePath)
    {
        if (!std::filesystem::exists(filePath))
            return false;

        std::ifstream file(filePath);
        if (!file.is_open())
            return false;

        // Determine preferred language code (e.g. "EN", "ZH")
        std::string preferredLang = "EN";
        try
        {
            auto languages = winrt::Windows::Globalization::ApplicationLanguages::Languages();
            if (languages.Size() > 0)
            {
                auto lang = winrt::to_string(languages.GetAt(0));
                // Extract first 2 chars and uppercase
                if (lang.size() >= 2)
                {
                    preferredLang = lang.substr(0, 2);
                    std::transform(preferredLang.begin(), preferredLang.end(), preferredLang.begin(), ::toupper);
                }
            }
        }
        catch (...) {}

        // Also accept Chinese variant codes
        bool isZH = (preferredLang == "ZH");

        std::string line;
        bool headerSkipped = false;

        while (std::getline(file, line))
        {
            if (line.empty()) continue;
            if (!headerSkipped)
            {
                headerSkipped = true;
                continue; // skip header
            }

            // Format: "LANG","LANG_NAME","COUNTRY_ALPHA2_CODE","COUNTRY_ALPHA3_CODE","COUNTRY_NUMERIC_CODE","COUNTRY_NAME"
            std::istringstream ss(line);
            auto lang = ParseCSVField(ss);
            auto langName = ParseCSVField(ss);
            auto alpha2 = ParseCSVField(ss);
            auto alpha3 = ParseCSVField(ss);
            auto numericCode = ParseCSVField(ss);
            auto countryName = ParseCSVField(ss);

            // Match preferred language
            std::string langUpper = lang;
            std::transform(langUpper.begin(), langUpper.end(), langUpper.begin(), ::toupper);

            if (langUpper == preferredLang || (isZH && langUpper == "ZH"))
            {
                if (!alpha2.empty() && !countryName.empty())
                {
                    m_countryNames[alpha2] = countryName;
                }
            }
        }

        return !m_countryNames.empty();
    }

    uint32_t GeoIPManager::IPv4ToUint32(std::string const& ip)
    {
        // Handle IPv6-mapped IPv4 (e.g. "::ffff:192.168.1.1")
        std::string ipv4 = ip;
        auto pos = ipv4.find("::ffff:");
        if (pos != std::string::npos)
        {
            ipv4 = ipv4.substr(pos + 7);
        }

        // Parse dotted-decimal IPv4
        uint32_t result = 0;
        int shift = 24;
        std::istringstream ss(ipv4);
        std::string octet;
        while (std::getline(ss, octet, '.') && shift >= 0)
        {
            try
            {
                result |= (static_cast<uint32_t>(std::stoi(octet)) << shift);
            }
            catch (...) { return 0; }
            shift -= 8;
        }
        return result;
    }

    GeoIPManager::IPRange const* GeoIPManager::FindRange(uint32_t ipNum) const
    {
        if (m_ranges.empty() || ipNum == 0) return nullptr;

        // Binary search: find the last range where ipFrom <= ipNum
        auto it = std::upper_bound(m_ranges.begin(), m_ranges.end(), ipNum,
            [](uint32_t val, IPRange const& range) { return val < range.ipFrom; });

        if (it == m_ranges.begin()) return nullptr;
        --it;

        if (ipNum >= it->ipFrom && ipNum <= it->ipTo)
            return &(*it);

        return nullptr;
    }

    std::string GeoIPManager::LookupCountryCode(std::string const& ipAddress) const
    {
        if (!m_loaded) return {};

        // Skip IPv6 (non-mapped) for now
        if (ipAddress.find(':') != std::string::npos && ipAddress.find('.') == std::string::npos)
            return {};

        uint32_t ipNum = IPv4ToUint32(ipAddress);
        auto* range = FindRange(ipNum);
        if (!range || range->countryCode == "-")
            return {};

        return range->countryCode;
    }

    std::string GeoIPManager::LookupCountryName(std::string const& ipAddress) const
    {
        if (!m_loaded) return {};

        auto code = LookupCountryCode(ipAddress);
        if (code.empty()) return {};

        // Try multilingual name first
        auto it = m_countryNames.find(code);
        if (it != m_countryNames.end())
            return it->second;

        // Fall back to English name from DB1
        uint32_t ipNum = IPv4ToUint32(ipAddress);
        auto* range = FindRange(ipNum);
        if (range)
            return range->countryName;

        return code;
    }
}
