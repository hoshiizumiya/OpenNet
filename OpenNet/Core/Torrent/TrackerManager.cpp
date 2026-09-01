module OpenNet.Core.Torrent.TrackerManager;

import winrt.Windows.Storage;
import winrt.Windows.Data.Json;
import winrt.Windows.Web.Http;
import winrt.Windows.Foundation;
import winrt.Microsoft.Windows.Storage;
import OpenNet.Core.AppSettingsDatabase;
import std;

namespace OpenNet::Core::Torrent
{
    using namespace winrt;
    using namespace winrt::Windows::Storage;
    using namespace winrt::Windows::Data::Json;
    using namespace winrt::Windows::Web::Http;
    using namespace winrt::Windows::Foundation;

    namespace
    {
        constexpr wchar_t DefaultSubscriptionUrl[] =
            L"https://cf.trackerslist.com/best.txt";
        constexpr wchar_t DefaultSubscriptionName[] = L"XIU2 best trackers";

        template<typename TAsync>
        IAsyncOperation<bool> WaitForTrackerRequestAsync(
            TAsync const& operation,
            std::chrono::seconds timeout)
        {
            auto const deadline = std::chrono::steady_clock::now() + timeout;
            while (operation.Status() == AsyncStatus::Started)
            {
                if (std::chrono::steady_clock::now() >= deadline)
                {
                    operation.Cancel();
                    co_return false;
                }
                co_await winrt::resume_after(std::chrono::milliseconds(50));
            }
            co_return operation.Status() == AsyncStatus::Completed;
        }
    }

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
        std::shared_ptr<std::promise<void>> completionSource;
        std::shared_future<void> completion;
        bool ownsInitialization = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_initialized)
            {
                co_return;
            }
            if (m_initializing)
            {
                completion = m_initializationCompletion;
            }
            else
            {
                completionSource = std::make_shared<std::promise<void>>();
                completion = completionSource->get_future().share();
                m_initializationCompletion = completion;
                m_initializing = true;
                ownsInitialization = true;
            }
        }

        if (!ownsInitialization)
        {
            co_await winrt::resume_background();
            completion.get();
            co_return;
        }

        try
        {
            auto localFolder = ApplicationData::Current().LocalFolder();
            m_configPath = std::wstring(localFolder.Path().c_str()) + L"\\trackers.json";
            bool const configurationExisted = co_await LoadTrackersAsync();
            bool retryEmptySubscription = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                retryEmptySubscription = configurationExisted
                    && m_trackers.empty() && !m_subscriptions.empty();
            }
            if (!configurationExisted)
            {
                // First install: seed the same maintained list offered by the
                // Network settings page. Persisting the subscription before the
                // download also makes an offline first launch recoverable.
                co_await SubscribeToTrackerListAsync(
                    DefaultSubscriptionUrl, DefaultSubscriptionName);
            }
            else if (retryEmptySubscription)
            {
                std::pair<std::wstring, std::wstring> subscription;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    subscription = m_subscriptions.front();
                }
                co_await SubscribeToTrackerListAsync(
                    subscription.second, L"Tracker subscription");
            }
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_initialized = true;
                m_initializing = false;
            }
            completionSource->set_value();
        }
        catch (...)
        {
            auto error = std::current_exception();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_initialized = false;
                m_initializing = false;
            }
            completionSource->set_exception(error);
            std::rethrow_exception(error);
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

    bool TrackerManager::AutoAddToNewTorrents() const
    {
        auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
        db.Initialize();
        return db.GetBool(
            ::OpenNet::Core::AppSettingsDatabase::CAT_TRACKER,
            "autoAddCustomTrackers").value_or(true);
    }

    void TrackerManager::AutoAddToNewTorrents(bool value)
    {
        auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
        db.Initialize();
        db.SetBool(
            ::OpenNet::Core::AppSettingsDatabase::CAT_TRACKER,
            "autoAddCustomTrackers",
            value);
    }

    winrt::Windows::Foundation::IAsyncAction TrackerManager::SubscribeToTrackerListAsync(
        const std::wstring& subscriptionUrl,
        const std::wstring& subscriptionName)
    {
        try
        {
            std::wstring id;
            bool addedSubscription = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto existing = std::find_if(
                    m_subscriptions.begin(), m_subscriptions.end(),
                    [&subscriptionUrl](auto const& item)
                    {
                        return item.second == subscriptionUrl;
                    });
                if (existing != m_subscriptions.end())
                {
                    id = existing->first;
                }
                else
                {
                    id = std::wstring(L"sub_") + std::to_wstring(
                        std::chrono::system_clock::now().time_since_epoch().count());
                    m_subscriptions.push_back({ id, subscriptionUrl });
                    addedSubscription = true;
                }
            }
            // Save the source before touching the network. An unavailable list
            // is still an active subscription and will be retried next launch.
            if (addedSubscription)
                SaveTrackers();

            // Download tracker list
            HttpClient client;
            auto uri = Uri(subscriptionUrl);
            auto request = client.GetAsync(uri);
            if (!co_await WaitForTrackerRequestAsync(request, std::chrono::seconds(8)))
                co_return;
            auto response = request.GetResults();

            if (!response.IsSuccessStatusCode())
            {
                co_return;
            }

            auto contentRequest = response.Content().ReadAsStringAsync();
            if (!co_await WaitForTrackerRequestAsync(
                contentRequest, std::chrono::seconds(3)))
            {
                co_return;
            }
            auto contentStr = contentRequest.GetResults();

            // Parse tracker list (one URL per line)
            std::wistringstream stream(contentStr.c_str());
            std::wstring line;
            int addedCount = 0;

            std::vector<TrackerInfo> downloaded;
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

                downloaded.push_back(std::move(info));
                addedCount++;
            }

            if (downloaded.empty())
                co_return;

            // Replace one subscription atomically only after a valid list was
            // received, so a temporary network failure never erases the last
            // known-good trackers.
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                std::erase_if(m_trackers, [&id](TrackerInfo const& tracker)
                {
                    return tracker.id.starts_with(id + L"_");
                });
                m_trackers.insert(
                    m_trackers.end(),
                    std::make_move_iterator(downloaded.begin()),
                    std::make_move_iterator(downloaded.end()));
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

    winrt::Windows::Foundation::IAsyncOperation<bool> TrackerManager::LoadTrackersAsync()
    {
        try
        {
            auto folder = ApplicationData::Current().LocalFolder();
            auto item = co_await folder.TryGetItemAsync(L"trackers.json");

            if (!item)
            {
                co_return false;
            }

            auto file = item.as<StorageFile>();
            if (!file)
            {
                co_return false;
            }

            auto content = co_await FileIO::ReadTextAsync(file);
            JsonObject root;

            if (!JsonObject::TryParse(content, root))
            {
                co_return true;
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
                    info.addedTime = static_cast<std::int64_t>(obj.GetNamedNumber(L"addedTime", 0));

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
            co_return true;
        }
        catch (...)
        {
            // Handle load errors
            co_return false;
        }
    }
}
