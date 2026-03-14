#include "pch.h"
#include "GeoIPManager.h"

#include "GeoIPDatabase.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Globalization.h>

namespace OpenNet::Core
{
    namespace
    {
        std::string ParseCSVField(std::istringstream& ss)
        {
            std::string result;
            if (ss.peek() == '"')
            {
                ss.get();
                std::getline(ss, result, '"');
                if (ss.peek() == ',') ss.get();
            }
            else
            {
                std::getline(ss, result, ',');
            }
            return result;
        }
    }

    GeoIPManager& GeoIPManager::Instance()
    {
        static GeoIPManager instance;
        return instance;
    }

    void GeoIPManager::Initialize(std::wstring const& assetsDir)
    {
        std::lock_guard lk(m_mutex);
        if (m_loaded)
            return;

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

        auto mmdbPath = dir + L"\\dbip-country-lite.mmdb";
        auto multiPath = dir + L"\\IP2LOCATION-COUNTRY-MULTILINGUAL.CSV";

        m_countryNames.clear();
        LoadMultilingual(multiPath);

        auto db = std::make_unique<GeoIPDatabase>();
        if (db->Load(mmdbPath))
        {
            m_database = std::move(db);
            m_loaded = true;
            OutputDebugStringA("GeoIPManager: MMDB loaded\n");
        }
        else
        {
            OutputDebugStringA("GeoIPManager: dbip-country-lite.mmdb not found or invalid. GeoIP lookups disabled.\n");
        }
    }

    bool GeoIPManager::LoadMultilingual(std::wstring const& filePath)
    {
        if (!std::filesystem::exists(filePath))
            return false;

        std::ifstream file(filePath);
        if (!file.is_open())
            return false;

        std::string preferredLang = "EN";
        try
        {
            auto languages = winrt::Windows::Globalization::ApplicationLanguages::Languages();
            if (languages.Size() > 0)
            {
                auto lang = winrt::to_string(languages.GetAt(0));
                if (lang.size() >= 2)
                {
                    preferredLang = lang.substr(0, 2);
                    std::transform(preferredLang.begin(), preferredLang.end(), preferredLang.begin(), ::toupper);
                }
            }
        }
        catch (...) {}

        bool const isZH = (preferredLang == "ZH");

        std::string line;
        bool headerSkipped = false;

        while (std::getline(file, line))
        {
            if (line.empty()) continue;
            if (!headerSkipped)
            {
                headerSkipped = true;
                continue;
            }

            std::istringstream ss(line);
            auto lang = ParseCSVField(ss);
            ParseCSVField(ss);
            auto alpha2 = ParseCSVField(ss);
            ParseCSVField(ss);
            ParseCSVField(ss);
            auto countryName = ParseCSVField(ss);

            std::transform(lang.begin(), lang.end(), lang.begin(), ::toupper);
            std::transform(alpha2.begin(), alpha2.end(), alpha2.begin(), ::toupper);
            if (lang == preferredLang || (isZH && lang == "ZH"))
            {
                if (!alpha2.empty() && !countryName.empty())
                    m_countryNames[alpha2] = countryName;
            }
        }

        return !m_countryNames.empty();
    }

    std::string GeoIPManager::LookupCountryCode(std::string const& ipAddress) const
    {
        std::lock_guard lk(m_mutex);
        if (!m_loaded || !m_database)
            return {};

        return m_database->LookupCountryCode(ipAddress);
    }

    std::string GeoIPManager::LookupCountryName(std::string const& ipAddress) const
    {
        std::lock_guard lk(m_mutex);
        if (!m_loaded || !m_database)
            return {};

        auto code = m_database->LookupCountryCode(ipAddress);
        if (code.empty())
            return {};

        auto it = m_countryNames.find(code);
        if (it != m_countryNames.end())
            return it->second;

        auto english = m_database->LookupCountryName(ipAddress);
        if (!english.empty())
            return english;

        return code;
    }
}
