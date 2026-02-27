#include "pch.h"
#include "RSSManager.h"
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
        try
        {
            auto localFolder = ApplicationData::Current().LocalFolder();
            m_configPath = std::wstring(localFolder.Path().c_str()) + L"\\rss_subscriptions.json";
            co_await LoadSubscriptionsAsync();
        }
        catch (...)
        {
            // Handle initialization errors
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
            m_feeds[feed.id] = std::move(feed);
            SaveSubscriptions();
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
            SaveSubscriptions();
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
            SaveSubscriptions();
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

                            // Process new items
                            self->ProcessNewItems(existingFeed, parsedFeed->items);
                            existingFeed.lastUpdated = std::chrono::system_clock::now();
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
        // If feed has no items yet, add all items from parsed feed
        if (feed.items.empty())
        {
            feed.items = newItems;

            // Notify new item callback for each item
            for (const auto& item : newItems)
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                if (m_newItemCallback)
                {
                    m_newItemCallback(feed.id, item);
                }
            }
            
            // Save items to disk
            SaveFeedItems(feed.id, feed.items);
            return;
        }

        // Find items that don't exist in the current feed
        std::unordered_set<std::wstring> existingGuids;
        for (const auto& item : feed.items)
        {
            std::wstring itemId = item.guid.empty() ? item.link : item.guid;
            existingGuids.insert(itemId);
        }

        bool hasNewItems = false;
        for (const auto& newItem : newItems)
        {
            std::wstring itemId = newItem.guid.empty() ? newItem.link : newItem.guid;
            if (existingGuids.find(itemId) == existingGuids.end())
            {
                // This is a new item
                feed.items.push_back(newItem);
                hasNewItems = true;

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

        // Keep only the most recent 100 items per feed
        if (feed.items.size() > 100)
        {
            feed.items.erase(feed.items.begin(), feed.items.begin() + (feed.items.size() - 100));
        }
        
        // Save items to disk if any changes were made
        if (hasNewItems)
        {
            SaveFeedItems(feed.id, feed.items);
        }
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

    void RSSManager::SaveFeedItems(const std::wstring& feedId, const std::vector<RSSItem>& items)
    {
        // Fire and forget async save operation
        auto saveTask = [feedId = std::wstring(feedId), items = std::vector<RSSItem>(items)]() -> winrt::Windows::Foundation::IAsyncAction
        {
            try
            {
                auto folder = ApplicationData::Current().LocalFolder();
                
                // Create RSS folder if it doesn't exist
                winrt::Windows::Storage::StorageFolder rssFolder = nullptr;
                bool folderExists = false;
                
                try
                {
                    rssFolder = co_await folder.GetFolderAsync(L"rss_data");
                    folderExists = true;
                }
                catch (hresult_error const&)
                {
                    folderExists = false;
                }
                
                if (!folderExists)
                {
                    rssFolder = co_await folder.CreateFolderAsync(L"rss_data", CreationCollisionOption::OpenIfExists);
                }

                if (!rssFolder) co_return;

                // Build filename from feed ID
                std::wstring filename = L"rss_" + feedId + L".json";
                
                // Create JSON array of items
                JsonArray itemsArray;
                for (const auto& item : items)
                {
                    JsonObject itemObj;
                    itemObj.SetNamedValue(L"guid", JsonValue::CreateStringValue(item.guid));
                    itemObj.SetNamedValue(L"title", JsonValue::CreateStringValue(item.title));
                    itemObj.SetNamedValue(L"link", JsonValue::CreateStringValue(item.link));
                    itemObj.SetNamedValue(L"description", JsonValue::CreateStringValue(item.description));
                    itemObj.SetNamedValue(L"enclosureUrl", JsonValue::CreateStringValue(item.enclosureUrl));
                    itemObj.SetNamedValue(L"enclosureType", JsonValue::CreateStringValue(item.enclosureType));
                    itemObj.SetNamedValue(L"enclosureLength", JsonValue::CreateNumberValue(static_cast<double>(item.enclosureLength)));
                    itemObj.SetNamedValue(L"pubDate", JsonValue::CreateNumberValue(
                        static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                            item.pubDate.time_since_epoch()).count())));
                    itemObj.SetNamedValue(L"category", JsonValue::CreateStringValue(item.category));
                    itemObj.SetNamedValue(L"isDownloaded", JsonValue::CreateBooleanValue(item.isDownloaded));
                    itemsArray.Append(itemObj);
                }

                // Write to file
                auto file = co_await rssFolder.CreateFileAsync(filename, CreationCollisionOption::ReplaceExisting);
                co_await FileIO::WriteTextAsync(file, itemsArray.Stringify());
            }
            catch (...) {}
        }();
    }

    void RSSManager::SaveSubscriptions()
    {
        try
        {
            JsonArray jsonArray;
            
            for (const auto& [id, feed] : m_feeds)
            {
                JsonObject obj;
                obj.SetNamedValue(L"id", JsonValue::CreateStringValue(feed.id));
                obj.SetNamedValue(L"url", JsonValue::CreateStringValue(feed.url));
                obj.SetNamedValue(L"title", JsonValue::CreateStringValue(feed.title));
                obj.SetNamedValue(L"savePath", JsonValue::CreateStringValue(feed.savePath));
                obj.SetNamedValue(L"updateInterval", JsonValue::CreateNumberValue(static_cast<double>(feed.updateInterval.count())));
                obj.SetNamedValue(L"autoDownload", JsonValue::CreateBooleanValue(feed.autoDownload));
                obj.SetNamedValue(L"filterPattern", JsonValue::CreateStringValue(feed.filterPattern));
                obj.SetNamedValue(L"enabled", JsonValue::CreateBooleanValue(feed.enabled));
                jsonArray.Append(obj);
            }

            // Write to file asynchronously
            [](std::wstring path, hstring content) -> fire_and_forget
            {
                bool needsCreate = false;
                try
                {
                    auto file = co_await StorageFile::GetFileFromPathAsync(path);
                    co_await FileIO::WriteTextAsync(file, content);
                }
                catch (hresult_error const&)
                {
                    needsCreate = true;
                }

                if (needsCreate)
                {
                    try
                    {
                        auto folder = ApplicationData::Current().LocalFolder();
                        auto file = co_await folder.CreateFileAsync(L"rss_subscriptions.json", CreationCollisionOption::ReplaceExisting);
                        co_await FileIO::WriteTextAsync(file, content);
                    }
                    catch (...) {}
                }
            }(m_configPath, jsonArray.Stringify());
        }
        catch (...) {}
    }

    winrt::Windows::Foundation::IAsyncAction RSSManager::LoadSubscriptionsAsync()
    {
        try
        {
            auto folder = ApplicationData::Current().LocalFolder();

            // Use co_await for async operations
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

                    // Load persisted items for this feed asynchronously
                    std::vector<RSSItem> items;
                    co_await LoadFeedItemsAsync(feed.id, items);
                    feed.items = std::move(items);

                    m_feeds[feed.id] = std::move(feed);
                }
            }
        }
        catch (...) 
        {
            // Silently ignore errors during load
        }
    }

    winrt::Windows::Foundation::IAsyncAction RSSManager::LoadFeedItemsAsync(const std::wstring& feedId, std::vector<RSSItem> items)
    {
        try
        {
            auto folder = ApplicationData::Current().LocalFolder();
            
            // Use TryGetItemAsync to safely check if folder exists
            auto folderItem = co_await folder.TryGetItemAsync(L"rss_data");
            if (!folderItem)
            {
                // Folder doesn't exist yet, items remain empty
                co_return;
            }

            auto rssFolder = folderItem.try_as<StorageFolder>();
            if (!rssFolder)
            {
                co_return;
            }

            std::wstring filename = L"rss_" + feedId + L".json";
            
            try
            {
                auto fileItem = co_await rssFolder.TryGetItemAsync(filename);
                if (!fileItem)
                {
                    // File doesn't exist yet
                    co_return;
                }

                auto file = fileItem.try_as<StorageFile>();
                if (!file)
                {
                    co_return;
                }

                auto content = co_await FileIO::ReadTextAsync(file);

                JsonArray itemsArray;
                if (JsonArray::TryParse(content, itemsArray))
                {
                    items.clear();
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
                        
                        items.push_back(item);
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
