#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace OpenNet::Core
{
    class GeoIPDatabase;

    class GeoIPManager
    {
    public:
        static GeoIPManager& Instance();

        void Initialize(std::wstring const& assetsDir = L"");

        std::string LookupCountryCode(std::string const& ipAddress) const;
        std::string LookupCountryName(std::string const& ipAddress) const;

        bool IsLoaded() const { return m_loaded; }

    private:
        GeoIPManager() = default;
        ~GeoIPManager() = default;
        GeoIPManager(GeoIPManager const&) = delete;
        GeoIPManager& operator=(GeoIPManager const&) = delete;

        bool LoadMultilingual(std::wstring const& filePath);

        std::unique_ptr<GeoIPDatabase> m_database;
        std::unordered_map<std::string, std::string> m_countryNames;
        bool m_loaded{ false };
        mutable std::mutex m_mutex;
    };
}
