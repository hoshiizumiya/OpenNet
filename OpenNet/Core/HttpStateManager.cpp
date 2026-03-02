/*
 * PROJECT:   OpenNet
 * FILE:      Core/HttpStateManager.cpp
 * PURPOSE:   Persistence for HTTP/HTTPS/FTP download records (Aria2-based)
 *
 * LICENSE:   The MIT License
 */

#include "pch.h"
#include "Core/HttpStateManager.h"

#include <nlohmann/json.hpp>
#include <winrt/Windows.Storage.h>

#include <chrono>
#include <fstream>
#include <random>

using json = nlohmann::json;

namespace OpenNet::Core
{
    // ------------------------------------------------------------------
    //  JSON helpers
    // ------------------------------------------------------------------
    static void to_json(json& j, HttpDownloadRecord const& r)
    {
        j = json{
            {"recordId",       r.recordId},
            {"url",            r.url},
            {"savePath",       r.savePath},
            {"fileName",       r.fileName},
            {"name",           r.name},
            {"addedTimestamp",  r.addedTimestamp},
            {"totalSize",      r.totalSize},
            {"completedSize",  r.completedSize},
            {"status",         r.status},
            {"lastGid",        r.lastGid},
        };
    }

    static void from_json(json const& j, HttpDownloadRecord& r)
    {
        r.recordId       = j.value("recordId",      std::string{});
        r.url            = j.value("url",            std::string{});
        r.savePath       = j.value("savePath",       std::string{});
        r.fileName       = j.value("fileName",       std::string{});
        r.name           = j.value("name",           std::string{});
        r.addedTimestamp  = j.value("addedTimestamp", int64_t{0});
        r.totalSize      = j.value("totalSize",      int64_t{0});
        r.completedSize  = j.value("completedSize",  int64_t{0});
        r.status         = j.value("status",         0);
        r.lastGid        = j.value("lastGid",        std::string{});
    }

    // ------------------------------------------------------------------
    //  Singleton
    // ------------------------------------------------------------------
    HttpStateManager& HttpStateManager::Instance()
    {
        static HttpStateManager s_instance;
        return s_instance;
    }

    // ------------------------------------------------------------------
    //  Lifecycle
    // ------------------------------------------------------------------
    void HttpStateManager::Initialize()
    {
        std::lock_guard lock(m_mutex);
        if (m_initialized) return;

        try
        {
            auto localFolder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            m_filePath = std::wstring(localFolder.Path().c_str()) + L"\\http_downloads.json";
        }
        catch (...)
        {
            m_filePath = L"http_downloads.json";
        }

        m_initialized = true;
        // Release lock before calling Load() which acquires its own lock
        // Actually Load is private and we know the state — just read directly.
        LoadNoLock();
    }

    // ------------------------------------------------------------------
    //  Record CRUD
    // ------------------------------------------------------------------
    std::string HttpStateManager::AddRecord(std::string const& url, std::string const& savePath, std::string const& fileName)
    {
        std::lock_guard lock(m_mutex);

        HttpDownloadRecord rec;
        rec.recordId = GenerateRecordId();
        rec.url = url;
        rec.savePath = savePath;
        rec.fileName = fileName;
        rec.name = fileName.empty() ? url : fileName;
        rec.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        rec.status = 0; // pending

        m_records.push_back(rec);
        SaveNoLock();
        return rec.recordId;
    }

    void HttpStateManager::UpdateRecordGid(std::string const& recordId, std::string const& gid)
    {
        std::lock_guard lock(m_mutex);
        for (auto& r : m_records)
        {
            if (r.recordId == recordId) { r.lastGid = gid; SaveNoLock(); return; }
        }
    }

    void HttpStateManager::UpdateRecordName(std::string const& recordId, std::string const& name)
    {
        std::lock_guard lock(m_mutex);
        for (auto& r : m_records)
        {
            if (r.recordId == recordId) { r.name = name; SaveNoLock(); return; }
        }
    }

    void HttpStateManager::UpdateRecordProgress(std::string const& recordId, int64_t completedSize, int64_t totalSize)
    {
        std::lock_guard lock(m_mutex);
        for (auto& r : m_records)
        {
            if (r.recordId == recordId)
            {
                r.completedSize = completedSize;
                r.totalSize = totalSize;
                // Don't save on every progress tick; caller can batch with Save()
                return;
            }
        }
    }

    void HttpStateManager::UpdateRecordStatus(std::string const& recordId, int status)
    {
        std::lock_guard lock(m_mutex);
        for (auto& r : m_records)
        {
            if (r.recordId == recordId) { r.status = status; SaveNoLock(); return; }
        }
    }

    void HttpStateManager::DeleteRecord(std::string const& recordId)
    {
        std::lock_guard lock(m_mutex);
        std::erase_if(m_records, [&](auto& r) { return r.recordId == recordId; });
        SaveNoLock();
    }

    // ------------------------------------------------------------------
    //  Lookup
    // ------------------------------------------------------------------
    std::optional<HttpDownloadRecord> HttpStateManager::FindByGid(std::string const& gid) const
    {
        std::lock_guard lock(m_mutex);
        for (auto const& r : m_records)
        {
            if (r.lastGid == gid) return r;
        }
        return std::nullopt;
    }

    std::optional<HttpDownloadRecord> HttpStateManager::FindByRecordId(std::string const& recordId) const
    {
        std::lock_guard lock(m_mutex);
        for (auto const& r : m_records)
        {
            if (r.recordId == recordId) return r;
        }
        return std::nullopt;
    }

    std::vector<HttpDownloadRecord> HttpStateManager::LoadAllRecords() const
    {
        std::lock_guard lock(m_mutex);
        return m_records;
    }

    void HttpStateManager::Save()
    {
        std::lock_guard lock(m_mutex);
        SaveNoLock();
    }

    // ------------------------------------------------------------------
    //  Internal I/O (must hold m_mutex)
    // ------------------------------------------------------------------
    void HttpStateManager::LoadNoLock()
    {
        m_records.clear();
        try
        {
            std::ifstream ifs(m_filePath);
            if (!ifs.is_open()) return;

            json j = json::parse(ifs, nullptr, false);
            if (j.is_discarded() || !j.is_array()) return;

            for (auto const& item : j)
            {
                HttpDownloadRecord rec = item.get<HttpDownloadRecord>();
                m_records.push_back(std::move(rec));
            }
        }
        catch (...) {}
    }

    void HttpStateManager::SaveNoLock()
    {
        try
        {
            json j = json::array();
            for (auto const& r : m_records)
            {
                json item;
                to_json(item, r);
                j.push_back(std::move(item));
            }

            std::ofstream ofs(m_filePath, std::ios::trunc);
            if (ofs.is_open())
            {
                ofs << j.dump(2);
            }
        }
        catch (...) {}
    }

    // ------------------------------------------------------------------
    //  ID generation
    // ------------------------------------------------------------------
    std::string HttpStateManager::GenerateRecordId()
    {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

        static std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, 0xFFFF);

        char buf[32];
        snprintf(buf, sizeof(buf), "%013llx%04x",
                 static_cast<unsigned long long>(ms), dist(rng));
        return std::string(buf);
    }
}
