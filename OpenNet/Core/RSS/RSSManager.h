#pragma once
#include "RSSTypes.h"
#include "RSSParser.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <unordered_map>

namespace OpenNet::Core::RSS
{
    /// <summary>
    /// Manages RSS feed subscriptions, updates, and auto-download
    /// </summary>
    class RSSManager
    {
    public:
        using FeedUpdatedCallback = std::function<void(const std::wstring& feedId)>;
        using NewItemCallback = std::function<void(const std::wstring& feedId, const RSSItem& item)>;
        using ErrorCallback = std::function<void(const std::wstring& feedId, const std::wstring& error)>;

        static RSSManager& Instance();

        RSSManager(const RSSManager&) = delete;
        RSSManager& operator=(const RSSManager&) = delete;

        /// <summary>
        /// Initialize the RSS manager and load saved subscriptions
        /// </summary>
        winrt::Windows::Foundation::IAsyncAction InitializeAsync();

        /// <summary>
        /// Start background update thread
        /// </summary>
        void Start();

        /// <summary>
        /// Stop background update thread
        /// </summary>
        void Stop();

        /// <summary>
        /// Add a new RSS feed subscription
        /// </summary>
        bool AddSubscription(const RSSSubscription& subscription);

        /// <summary>
        /// Remove a subscription by ID
        /// </summary>
        bool RemoveSubscription(const std::wstring& feedId);

        /// <summary>
        /// Update a subscription's settings
        /// </summary>
        bool UpdateSubscription(const RSSSubscription& subscription);

        /// <summary>
        /// Get all subscriptions
        /// </summary>
        std::vector<RSSFeed> GetAllFeeds() const;

        /// <summary>
        /// Get a specific feed by ID
        /// </summary>
        std::optional<RSSFeed> GetFeed(const std::wstring& feedId) const;

        /// <summary>
        /// Force refresh a specific feed
        /// </summary>
        void RefreshFeed(const std::wstring& feedId);

        /// <summary>
        /// Force refresh all feeds
        /// </summary>
        void RefreshAllFeeds();

        /// <summary>
        /// Mark an item as downloaded
        /// </summary>
        void MarkItemAsDownloaded(const std::wstring& feedId, const std::wstring& itemGuid);

        /// <summary>
        /// Download a specific item
        /// </summary>
        void DownloadItem(const std::wstring& feedId, const RSSItem& item);

        // Callbacks
        void SetFeedUpdatedCallback(FeedUpdatedCallback callback);
        void SetNewItemCallback(NewItemCallback callback);
        void SetErrorCallback(ErrorCallback callback);

    private:
        RSSManager();
        ~RSSManager();

        void UpdateLoop();
        void FetchFeed(const std::wstring& feedId);
        winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> FetchFeedContentAsync(const std::wstring& url);
        void ProcessNewItems(RSSFeed& feed, const std::vector<RSSItem>& newItems);

        // Legacy JSON persistence (kept for one-time migration)
        void SaveSubscriptions();
        void SaveFeedItems(const std::wstring& feedId, const std::vector<RSSItem>& items);
        winrt::Windows::Foundation::IAsyncAction LoadSubscriptionsFromLegacyJsonAsync();
        winrt::Windows::Foundation::IAsyncAction LoadFeedItemsFromLegacyJsonAsync(
            const std::wstring& feedId, std::shared_ptr<std::vector<RSSItem>> items);

        std::wstring GenerateFeedId();

        std::unordered_map<std::wstring, RSSFeed> m_feeds;
        mutable std::mutex m_feedsMutex;
        mutable std::mutex m_callbackMutex;
        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_initialized{ false };
        std::thread m_updateThread;
        std::condition_variable m_stopCv;
        std::mutex m_stopMutex;
        std::wstring m_configPath;

        FeedUpdatedCallback m_feedUpdatedCallback;
        NewItemCallback m_newItemCallback;
        ErrorCallback m_errorCallback;
    };
}
