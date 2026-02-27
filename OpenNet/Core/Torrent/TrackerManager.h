#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>

namespace OpenNet::Core::Torrent
{
    /// <summary>
    /// Tracker information
    /// </summary>
    struct TrackerInfo
    {
        std::wstring id;           // Unique identifier
        std::wstring name;         // Display name
        std::wstring url;          // Tracker URL
        std::wstring category;     // Category (public, private, etc.)
        bool enabled{ true };      // Is enabled
        long long addedTime{ 0 };  // Timestamp when added
    };

    /// <summary>
    /// Manages torrent trackers and tracker subscriptions
    /// </summary>
    class TrackerManager
    {
    public:
        static TrackerManager& Instance();

        TrackerManager(const TrackerManager&) = delete;
        TrackerManager& operator=(const TrackerManager&) = delete;

        /// <summary>
        /// Initialize tracker manager and load saved trackers
        /// </summary>
        winrt::Windows::Foundation::IAsyncAction InitializeAsync();

        /// <summary>
        /// Add a custom tracker
        /// </summary>
        bool AddTracker(const TrackerInfo& tracker);

        /// <summary>
        /// Remove a tracker
        /// </summary>
        bool RemoveTracker(const std::wstring& trackerId);

        /// <summary>
        /// Update a tracker
        /// </summary>
        bool UpdateTracker(const TrackerInfo& tracker);

        /// <summary>
        /// Get all trackers
        /// </summary>
        std::vector<TrackerInfo> GetAllTrackers() const;

        /// <summary>
        /// Get enabled trackers
        /// </summary>
        std::vector<TrackerInfo> GetEnabledTrackers() const;

        /// <summary>
        /// Subscribe to tracker list (from .txt file URL)
        /// </summary>
        winrt::Windows::Foundation::IAsyncAction SubscribeToTrackerListAsync(
            const std::wstring& subscriptionUrl,
            const std::wstring& subscriptionName);

        /// <summary>
        /// Get all subscriptions
        /// </summary>
        std::vector<std::pair<std::wstring, std::wstring>> GetSubscriptions() const;

        /// <summary>
        /// Remove a subscription
        /// </summary>
        bool RemoveSubscription(const std::wstring& subscriptionId);

    private:
        TrackerManager();
        ~TrackerManager();

        void SaveTrackers();
        winrt::Windows::Foundation::IAsyncAction LoadTrackersAsync();

        mutable std::mutex m_mutex;
        std::vector<TrackerInfo> m_trackers;
        std::vector<std::pair<std::wstring, std::wstring>> m_subscriptions;  // id, url pairs
        std::wstring m_configPath;
    };
}
