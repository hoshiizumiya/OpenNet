#include "pch.h"
#include "RSSManager.h"
#include "RSSDatabase.h"
#include "Core/AppSettingsDatabase.h"
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Data.Json.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <unordered_set>

namespace OpenNet::Core::RSS
{
    using namespace winrt;
    using namespace winrt::Windows::Web::Http;
    using namespace winrt::Windows::Storage;
    using namespace winrt::Windows::Data::Json;

    RSSManager& RSSManager::Instance()
    {
        static RSSManager instance;
        return instance;
    }

    RSSManager::RSSManager()
    {
    }

    RSSManager::~RSSManager()
    {
        Stop();
    }

    winrt::Windows::Foundation::IAsyncAction RSSManager::InitializeAsync()
    {
        if (m_initialized.exchange(true))
        {
            co_return;  // Already initialized
        }

        try
        {
            auto localFolder = ApplicationData::Current().LocalFolder();
            m_configPath = std::wstring(localFolder.Path().c_str()) + L"\\rss_subscriptions.json";

            auto& db = RSSDatabase::Instance();

            if (db.HasFeeds())
            {
                // Load from SQLite
                auto feeds = db.LoadAllFeeds();
                std::lock_guard<std::mutex> lock(m_feedsMutex);
                for (auto& feed : feeds)
                {
                    m_feeds[feed.id] = std::move(feed);
                }
            }
            else
            {
                // Try legacy JSON migration
                co_await LoadSubscriptionsFromLegacyJsonAsync();

                if (!m_feeds.empty())
                {
                    // Persist migrated data to SQLite
                    std::lock_guard<std::mutex> lock(m_feedsMutex);
                    for (auto const& [id, feed] : m_feeds)
                    {
                        db.UpsertFeed(feed);
                        if (!feed.items.empty())
                        {
                            db.InsertItems(feed.id, feed.items);
                        }
                    }
                    OutputDebugStringA("RSSManager: migrated feeds from JSON to SQLite\n");
                }
            }
        }
        catch (...)
        {
            m_initialized = false;  // Allow retry on failure
        }
    }

    void RSSManager::Start()
    {
        if (m_running.exchange(true))
        {
            return;  // Already running
        }

        m_updateThread = std::thread(&RSSManager::UpdateLoop, this);
    }

    void RSSManager::Stop()
    {
        m_running = false;
        if (m_updateThread.joinable())
        {
            m_updateThread.join();
        }
    }

    void RSSManager::UpdateLoop()
    {
        while (m_running)
        {
            auto now = std::chrono::system_clock::now();
            std::vector<std::wstring> feedsToUpdate;

            {
                std::lock_guard<std::mutex> lock(m_feedsMutex);
                for (auto& [id, feed] : m_feeds)
                {
                    if (!feed.enabled) continue;

                    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - feed.lastUpdated);
                    if (elapsed >= feed.updateInterval)
                    {
                        feedsToUpdate.push_back(id);
                    }
                }
            }

            // Fetch outside the lock to avoid deadlock
            for (const auto& id : feedsToUpdate)
            {
                FetchFeed(id);
            }

            // Sleep for 1 minute between checks
            for (int i = 0; i < 60 && m_running; ++i)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    bool RSSManager::AddSubscription(const RSSSubscription& subscription)
    {
        std::wstring feedId;
        {
            std::lock_guard<std::mutex> lock(m_feedsMutex);

            RSSFeed feed;
            feed.id = subscription.id.empty() ? GenerateFeedId() : subscription.id;
            feed.url = subscription.url;
            feed.title = subscription.name;
            feed.savePath = subscription.savePath;
            feed.updateInterval = subscription.updateInterval;
            feed.autoDownload = subscription.autoDownload;
            feed.filterPattern = subscription.filterPattern;
            feed.enabled = subscription.enabled;
            feed.lastUpdated = std::chrono::system_clock::time_point{};  // Force immediate update

            feedId = feed.id;
            m_feeds[feed.id] = feed;

            // Persist to SQLite
            RSSDatabase::Instance().UpsertFeed(feed);
        }

        // Trigger immediate fetch (outside the lock to avoid deadlock)
        FetchFeed(feedId);

        return true;
    }

    bool RSSManager::RemoveSubscription(const std::wstring& feedId)
    {
        std::lock_guard<std::mutex> lock(m_feedsMutex);

        auto it = m_feeds.find(feedId);
        if (it != m_feeds.end())
        {
            m_feeds.erase(it);

            // Delete from SQLite (CASCADE removes items too)
            RSSDatabase::Instance().DeleteFeed(feedId);

            return true;
        }
        return false;
    }

    bool RSSManager::UpdateSubscription(const RSSSubscription& subscription)
    {
        std::lock_guard<std::mutex> lock(m_feedsMutex);

        auto it = m_feeds.find(subscription.id);
        if (it != m_feeds.end())
        {
            it->second.url = subscription.url;
            it->second.title = subscription.name;
            it->second.savePath = subscription.savePath;
            it->second.updateInterval = subscription.updateInterval;
            it->second.autoDownload = subscription.autoDownload;
            it->second.filterPattern = subscription.filterPattern;
            it->second.enabled = subscription.enabled;

            // Persist to SQLite
            RSSDatabase::Instance().UpsertFeed(it->second);
            return true;
        }
        return false;
    }

    std::vector<RSSFeed> RSSManager::GetAllFeeds() const
    {
        std::lock_guard<std::mutex> lock(m_feedsMutex);

        std::vector<RSSFeed> result;
        result.reserve(m_feeds.size());
        for (const auto& [id, feed] : m_feeds)
        {
            result.push_back(feed);
        }
        return result;
    }

    std::optional<RSSFeed> RSSManager::GetFeed(const std::wstring& feedId) const
    {
        std::lock_guard<std::mutex> lock(m_feedsMutex);

        auto it = m_feeds.find(feedId);
        if (it != m_feeds.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    void RSSManager::RefreshFeed(const std::wstring& feedId)
    {
        FetchFeed(feedId);
    }

    void RSSManager::RefreshAllFeeds()
    {
        std::vector<std::wstring> feedIds;
        {
            std::lock_guard<std::mutex> lock(m_feedsMutex);
            for (const auto& [id, feed] : m_feeds)
            {
                if (feed.enabled)
                {
                    feedIds.push_back(id);
                }
            }
        }

        // Fetch outside the lock to avoid deadlock
        for (const auto& id : feedIds)
        {
            FetchFeed(id);
        }
    }

    void RSSManager::FetchFeed(const std::wstring& feedId)
    {
        std::wstring url;
        {
            std::lock_guard<std::mutex> lock(m_feedsMutex);
            auto it = m_feeds.find(feedId);
            if (it == m_feeds.end()) return;
            url = it->second.url;
        }

        // Async fetch
        [](RSSManager* self, std::wstring feedId, std::wstring feedUrl) -> winrt::fire_and_forget
        {
            try
            {
                auto content = co_await self->FetchFeedContentAsync(feedUrl);

                auto parsedFeed = RSSParser::Parse(std::wstring(content.c_str()), feedUrl);
                if (parsedFeed)
                {
                    FeedUpdatedCallback callbackCopy;
                    {
                        std::lock_guard<std::mutex> lock(self->m_feedsMutex);
                        auto it = self->m_feeds.find(feedId);
                        if (it != self->m_feeds.end())
                        {
                            // Keep existing settings, update content
                            auto& existingFeed = it->second;
                            if (existingFeed.title.empty())
                            {
                                existingFeed.title = parsedFeed->title;
                            }
                            existingFeed.description = parsedFeed->description;

                            // Process new items (persists to SQLite internally)
                            self->ProcessNewItems(existingFeed, parsedFeed->items);
                            existingFeed.lastUpdated = std::chrono::system_clock::now();

                            // Persist updated feed metadata to SQLite
                            auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
                                             existingFeed.lastUpdated.time_since_epoch())
                                             .count();
                            RSSDatabase::Instance().UpdateFeedMeta(
                                feedId, existingFeed.title, existingFeed.description, epoch);
                        }

                        // Copy callback outside the feed lock
                        std::lock_guard<std::mutex> cbLock(self->m_callbackMutex);
                        callbackCopy = self->m_feedUpdatedCallback;
                    }

                    // Notify callback outside the lock
                    if (callbackCopy)
                    {
                        callbackCopy(feedId);
                    }
                }
            }
            catch (winrt::hresult_error const& ex)
            {
                ErrorCallback errorCallbackCopy;
                {
                    std::lock_guard<std::mutex> lock(self->m_callbackMutex);
                    errorCallbackCopy = self->m_errorCallback;
                }
                if (errorCallbackCopy)
                {
                    errorCallbackCopy(feedId, ex.message().c_str());
                }
            }
        }(this, feedId, url);
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> RSSManager::FetchFeedContentAsync(const std::wstring& url)
    {
        HttpClient client;
        
        // Add user agent to avoid being blocked
        client.DefaultRequestHeaders().UserAgent().ParseAdd(L"OpenNet/1.0 RSS Reader");
        
        auto response = co_await client.GetAsync(winrt::Windows::Foundation::Uri(url));
        response.EnsureSuccessStatusCode();
        
        auto content = co_await response.Content().ReadAsStringAsync();
        co_return content;
    }

    void RSSManager::ProcessNewItems(RSSFeed& feed, const std::vector<RSSItem>& newItems)
    {
        auto& db = RSSDatabase::Instance();

        // Insert new items into SQLite (skips duplicates via UNIQUE constraint)
        int insertedCount = db.InsertItems(feed.id, newItems);

        // If first load (no items in memory), populate from DB
        if (feed.items.empty())
        {
            feed.items = db.LoadItems(feed.id);

            // Notify callback for each item on first load
            for (const auto& item : feed.items)
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                if (m_newItemCallback)
                {
                    m_newItemCallback(feed.id, item);
                }
            }
            return;
        }

        // Find truly new items (not in memory yet)
        if (insertedCount > 0)
        {
            std::unordered_set<std::wstring> existingGuids;
            for (const auto& item : feed.items)
            {
                std::wstring itemId = item.guid.empty() ? item.link : item.guid;
                existingGuids.insert(itemId);
            }

            for (const auto& newItem : newItems)
            {
                std::wstring itemId = newItem.guid.empty() ? newItem.link : newItem.guid;
                if (existingGuids.find(itemId) == existingGuids.end())
                {
                    feed.items.push_back(newItem);

                    // Notify new item callback
                    {
                        std::lock_guard<std::mutex> lock(m_callbackMutex);
                        if (m_newItemCallback)
                        {
                            m_newItemCallback(feed.id, newItem);
                        }
                    }

                    // Auto-download if enabled and matches filter
                    if (feed.autoDownload && RSSParser::MatchesFilter(newItem, feed.filterPattern))
                    {
                        DownloadItem(feed.id, newItem);
                    }
                }
            }
        }

        // Prune old items – limit is configurable via AppSettingsDatabase (default 100)
        auto& settingsDb = AppSettingsDatabase::Instance();
        int maxItems = static_cast<int>(settingsDb.GetInt(AppSettingsDatabase::CAT_RSS, "max_items_per_feed", 100));
        if (maxItems < 10) maxItems = 10; // sanity floor
        auto limit = static_cast<size_t>(maxItems);
        if (feed.items.size() > limit)
        {
            feed.items.erase(feed.items.begin(), feed.items.begin() + (feed.items.size() - limit));
        }
        db.PruneItems(feed.id, maxItems);
    }

    void RSSManager::MarkItemAsDownloaded(const std::wstring& feedId, const std::wstring& itemGuid)
    {
        std::lock_guard<std::mutex> lock(m_feedsMutex);

        auto it = m_feeds.find(feedId);
        if (it != m_feeds.end())
        {
            for (auto& item : it->second.items)
            {
                if (item.guid == itemGuid)
                {
                    item.isDownloaded = true;
                    break;
                }
            }
        }

        // Persist to SQLite
        RSSDatabase::Instance().MarkItemDownloaded(feedId, itemGuid);
    }

    void RSSManager::DownloadItem(const std::wstring& feedId, const RSSItem& item)
    {
        auto torrentLink = RSSParser::ExtractTorrentLink(item);
        if (torrentLink.empty()) return;

        std::wstring savePath;
        {
            std::lock_guard<std::mutex> lock(m_feedsMutex);
            auto it = m_feeds.find(feedId);
            if (it != m_feeds.end())
            {
                savePath = it->second.savePath;
            }
        }

        // TODO: Integrate with LibtorrentHandle to add the torrent
        // This would call into the existing torrent infrastructure
        // For now, mark as downloaded
        MarkItemAsDownloaded(feedId, item.guid);
    }

    void RSSManager::SetFeedUpdatedCallback(FeedUpdatedCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_feedUpdatedCallback = std::move(callback);
    }

    void RSSManager::SetNewItemCallback(NewItemCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_newItemCallback = std::move(callback);
    }

    void RSSManager::SetErrorCallback(ErrorCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_errorCallback = std::move(callback);
    }

    // ---------------------------------------------------------------
    //  Legacy JSON persistence (kept only for one-time migration)
    // ---------------------------------------------------------------

    void RSSManager::SaveSubscriptions()
    {
        // No longer used – persistence is via SQLite now.
        // Kept as stub in case anything still calls it.
    }

    void RSSManager::SaveFeedItems(const std::wstring& /*feedId*/, const std::vector<RSSItem>& /*items*/)
    {
        // No longer used – persistence is via SQLite now.
    }

    winrt::Windows::Foundation::IAsyncAction RSSManager::LoadSubscriptionsFromLegacyJsonAsync()
    {
        try
        {
            auto folder = ApplicationData::Current().LocalFolder();

            auto item = co_await folder.TryGetItemAsync(L"rss_subscriptions.json");
            if (!item) co_return;

            auto file = item.as<StorageFile>();
            if (!file) co_return;

            auto content = co_await FileIO::ReadTextAsync(file);

            JsonArray jsonArray;
            if (JsonArray::TryParse(content, jsonArray))
            {
                std::lock_guard<std::mutex> lock(m_feedsMutex);

                for (uint32_t i = 0; i < jsonArray.Size(); ++i)
                {
                    auto obj = jsonArray.GetAt(i).GetObject();

                    RSSFeed feed;
                    feed.id = obj.GetNamedString(L"id").c_str();
                    feed.url = obj.GetNamedString(L"url").c_str();
                    feed.title = obj.GetNamedString(L"title", L"").c_str();
                    feed.savePath = obj.GetNamedString(L"savePath", L"").c_str();
                    feed.updateInterval = std::chrono::minutes(static_cast<int>(obj.GetNamedNumber(L"updateInterval", 30)));
                    feed.autoDownload = obj.GetNamedBoolean(L"autoDownload", false);
                    feed.filterPattern = obj.GetNamedString(L"filterPattern", L"").c_str();
                    feed.enabled = obj.GetNamedBoolean(L"enabled", true);

                    // Load persisted items for this feed from legacy JSON
                    auto items = std::make_shared<std::vector<RSSItem>>();
                    co_await LoadFeedItemsFromLegacyJsonAsync(feed.id, items);
                    feed.items = std::move(*items);

                    m_feeds[feed.id] = std::move(feed);
                }
            }
        }
        catch (...) 
        {
            // Silently ignore errors during legacy load
        }
    }

    winrt::Windows::Foundation::IAsyncAction RSSManager::LoadFeedItemsFromLegacyJsonAsync(
        const std::wstring& feedId, std::shared_ptr<std::vector<RSSItem>> items)
    {
        try
        {
            auto folder = ApplicationData::Current().LocalFolder();
            
            auto folderItem = co_await folder.TryGetItemAsync(L"rss_data");
            if (!folderItem) co_return;

            auto rssFolder = folderItem.try_as<StorageFolder>();
            if (!rssFolder) co_return;

            std::wstring filename = L"rss_" + feedId + L".json";
            
            try
            {
                auto fileItem = co_await rssFolder.TryGetItemAsync(filename);
                if (!fileItem) co_return;

                auto file = fileItem.try_as<StorageFile>();
                if (!file) co_return;

                auto content = co_await FileIO::ReadTextAsync(file);

                JsonArray itemsArray;
                if (JsonArray::TryParse(content, itemsArray))
                {
                    items->clear();
                    for (uint32_t i = 0; i < itemsArray.Size(); ++i)
                    {
                        auto itemObj = itemsArray.GetAt(i).GetObject();

                        RSSItem item;
                        item.guid = itemObj.GetNamedString(L"guid", L"").c_str();
                        item.title = itemObj.GetNamedString(L"title", L"").c_str();
                        item.link = itemObj.GetNamedString(L"link", L"").c_str();
                        item.description = itemObj.GetNamedString(L"description", L"").c_str();
                        item.enclosureUrl = itemObj.GetNamedString(L"enclosureUrl", L"").c_str();
                        item.enclosureType = itemObj.GetNamedString(L"enclosureType", L"").c_str();
                        item.enclosureLength = static_cast<uint64_t>(itemObj.GetNamedNumber(L"enclosureLength", 0));

                        auto pubDateSeconds = static_cast<int64_t>(itemObj.GetNamedNumber(L"pubDate", 0));
                        item.pubDate = std::chrono::system_clock::time_point(std::chrono::seconds(pubDateSeconds));

                        item.category = itemObj.GetNamedString(L"category", L"").c_str();
                        item.isDownloaded = itemObj.GetNamedBoolean(L"isDownloaded", false);

                        items->push_back(item);
                    }
                }
            }
            catch (hresult_error const&)
            {
                // File read error, items remain empty
            }
        }
        catch (...) {}
    }

    std::wstring RSSManager::GenerateFeedId()
    {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;

        std::wstringstream ss;
        ss << std::hex << std::setfill(L'0') << std::setw(16) << dis(gen);
        return ss.str();
    }
}
