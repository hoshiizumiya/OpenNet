#include "pch.h"
#include "TrackerManager.h"
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Foundation.h>
#include <sstream>
#include <chrono>

namespace OpenNet::Core::Torrent
{
    using namespace winrt;
    using namespace winrt::Windows::Storage;
    using namespace winrt::Windows::Data::Json;
    using namespace winrt::Windows::Web::Http;
    using namespace winrt::Windows::Foundation;

    TrackerManager& TrackerManager::Instance()
    {
        static TrackerManager instance;
        return instance;
    }

    TrackerManager::TrackerManager()
    {
    }

    TrackerManager::~TrackerManager()
    {
    }

    winrt::Windows::Foundation::IAsyncAction TrackerManager::InitializeAsync()
    {
        try
        {
            auto localFolder = ApplicationData::Current().LocalFolder();
            m_configPath = std::wstring(localFolder.Path().c_str()) + L"\\trackers.json";
            co_await LoadTrackersAsync();
        }
        catch (...)
        {
            // Handle initialization errors
        }
    }

    bool TrackerManager::AddTracker(const TrackerInfo& tracker)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if already exists
        for (const auto& t : m_trackers)
        {
            if (t.id == tracker.id)
            {
                return false;  // Already exists
            }
        }

        m_trackers.push_back(tracker);
        SaveTrackers();
        return true;
    }

    bool TrackerManager::RemoveTracker(const std::wstring& trackerId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::find_if(m_trackers.begin(), m_trackers.end(),
            [&trackerId](const TrackerInfo& t) { return t.id == trackerId; });

        if (it != m_trackers.end())
        {
            m_trackers.erase(it);
            SaveTrackers();
            return true;
        }

        return false;
    }

    bool TrackerManager::UpdateTracker(const TrackerInfo& tracker)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::find_if(m_trackers.begin(), m_trackers.end(),
            [&tracker](const TrackerInfo& t) { return t.id == tracker.id; });

        if (it != m_trackers.end())
        {
            *it = tracker;
            SaveTrackers();
            return true;
        }

        return false;
    }

    std::vector<TrackerInfo> TrackerManager::GetAllTrackers() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_trackers;
    }

    std::vector<TrackerInfo> TrackerManager::GetEnabledTrackers() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<TrackerInfo> enabled;
        for (const auto& tracker : m_trackers)
        {
            if (tracker.enabled)
            {
                enabled.push_back(tracker);
            }
        }

        return enabled;
    }

    winrt::Windows::Foundation::IAsyncAction TrackerManager::SubscribeToTrackerListAsync(
        const std::wstring& subscriptionUrl,
        const std::wstring& subscriptionName)
    {
        try
        {
            // Generate subscription ID
            auto id = std::wstring(L"sub_") + std::to_wstring(
                std::chrono::system_clock::now().time_since_epoch().count());

            // Download tracker list
            HttpClient client;
            auto uri = Uri(subscriptionUrl);
            auto response = co_await client.GetAsync(uri);

            if (!response.IsSuccessStatusCode())
            {
                co_return;
            }

            auto contentStr = co_await response.Content().ReadAsStringAsync();

            // Parse tracker list (one URL per line)
            std::wistringstream stream(contentStr.c_str());
            std::wstring line;
            int addedCount = 0;

            while (std::getline(stream, line))
            {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(L" \t\r\n"));
                line.erase(line.find_last_not_of(L" \t\r\n") + 1);

                if (line.empty() || line[0] == L'#')
                {
                    continue;  // Skip empty lines and comments
                }

                // Create tracker info
                TrackerInfo info;
                info.id = id + L"_" + std::to_wstring(addedCount);
                info.name = subscriptionName + L" - Tracker " + std::to_wstring(addedCount + 1);
                info.url = line;
                info.category = subscriptionName;
                info.enabled = true;
                info.addedTime = std::chrono::system_clock::now().time_since_epoch().count();

                AddTracker(info);
                addedCount++;
            }

            // Save subscription
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_subscriptions.push_back({ id, subscriptionUrl });
            }

            // Save trackers
            SaveTrackers();
        }
        catch (...)
        {
            // Handle error
        }
    }

    std::vector<std::pair<std::wstring, std::wstring>> TrackerManager::GetSubscriptions() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_subscriptions;
    }

    bool TrackerManager::RemoveSubscription(const std::wstring& subscriptionId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::find_if(m_subscriptions.begin(), m_subscriptions.end(),
            [&subscriptionId](const auto& sub) { return sub.first == subscriptionId; });

        if (it != m_subscriptions.end())
        {
            m_subscriptions.erase(it);

            // Also remove all trackers from this subscription
            m_trackers.erase(
                std::remove_if(m_trackers.begin(), m_trackers.end(),
                    [&subscriptionId](const TrackerInfo& t) { 
                        return t.id.find(subscriptionId) == 0; 
                    }),
                m_trackers.end());

            SaveTrackers();
            return true;
        }

        return false;
    }

    void TrackerManager::SaveTrackers()
    {
        try
        {
            JsonArray trackersArray;

            for (const auto& tracker : m_trackers)
            {
                JsonObject obj;
                obj.SetNamedValue(L"id", JsonValue::CreateStringValue(tracker.id));
                obj.SetNamedValue(L"name", JsonValue::CreateStringValue(tracker.name));
                obj.SetNamedValue(L"url", JsonValue::CreateStringValue(tracker.url));
                obj.SetNamedValue(L"category", JsonValue::CreateStringValue(tracker.category));
                obj.SetNamedValue(L"enabled", JsonValue::CreateBooleanValue(tracker.enabled));
                obj.SetNamedValue(L"addedTime", JsonValue::CreateNumberValue(static_cast<double>(tracker.addedTime)));
                trackersArray.Append(obj);
            }

            // Also save subscriptions
            JsonArray subsArray;
            for (const auto& [id, url] : m_subscriptions)
            {
                JsonObject obj;
                obj.SetNamedValue(L"id", JsonValue::CreateStringValue(id));
                obj.SetNamedValue(L"url", JsonValue::CreateStringValue(url));
                subsArray.Append(obj);
            }

            JsonObject root;
            root.SetNamedValue(L"trackers", trackersArray);
            root.SetNamedValue(L"subscriptions", subsArray);

            // Write to file
            [](std::wstring path, hstring content) -> fire_and_forget
            {
                try
                {
                    auto folder = ApplicationData::Current().LocalFolder();
                    auto file = co_await folder.CreateFileAsync(L"trackers.json", CreationCollisionOption::ReplaceExisting);
                    co_await FileIO::WriteTextAsync(file, content);
                }
                catch (...) {}
            }(m_configPath, root.Stringify());
        }
        catch (...) {}
    }

    winrt::Windows::Foundation::IAsyncAction TrackerManager::LoadTrackersAsync()
    {
        try
        {
            auto folder = ApplicationData::Current().LocalFolder();
            auto item = co_await folder.TryGetItemAsync(L"trackers.json");

            if (!item)
            {
                co_return;
            }

            auto file = item.as<StorageFile>();
            if (!file)
            {
                co_return;
            }

            auto content = co_await FileIO::ReadTextAsync(file);
            JsonObject root;

            if (!JsonObject::TryParse(content, root))
            {
                co_return;
            }

            std::lock_guard<std::mutex> lock(m_mutex);

            // Load trackers
            if (root.HasKey(L"trackers"))
            {
                auto trackersArray = root.GetNamedArray(L"trackers");
                for (uint32_t i = 0; i < trackersArray.Size(); ++i)
                {
                    auto obj = trackersArray.GetAt(i).GetObject();

                    TrackerInfo info;
                    info.id = obj.GetNamedString(L"id").c_str();
                    info.name = obj.GetNamedString(L"name").c_str();
                    info.url = obj.GetNamedString(L"url").c_str();
                    info.category = obj.GetNamedString(L"category", L"").c_str();
                    info.enabled = obj.GetNamedBoolean(L"enabled", true);
                    info.addedTime = static_cast<long long>(obj.GetNamedNumber(L"addedTime", 0));

                    m_trackers.push_back(info);
                }
            }

            // Load subscriptions
            if (root.HasKey(L"subscriptions"))
            {
                auto subsArray = root.GetNamedArray(L"subscriptions");
                for (uint32_t i = 0; i < subsArray.Size(); ++i)
                {
                    auto obj = subsArray.GetAt(i).GetObject();

                    std::wstring id = obj.GetNamedString(L"id").c_str();
                    std::wstring url = obj.GetNamedString(L"url").c_str();

                    m_subscriptions.push_back({ id, url });
                }
            }
        }
        catch (...)
        {
            // Handle load errors
        }
    }
}
