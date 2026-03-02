#include "pch.h"
#include "RSSViewModel.h"
#include "ViewModels/RSSItemViewModel.g.cpp"
#include "ViewModels/RSSFeedViewModel.g.cpp"
#include "ViewModels/RSSViewModel.g.cpp"
#include "Core/RSS/RSSManager.h"
#include "Core/RSS/RSSParser.h"
#include <winrt/Windows.Storage.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace winrt::OpenNet::ViewModels::implementation
{
    using namespace ::OpenNet::Core::RSS;

    // Helper to format file size
    static hstring FormatFileSize(uint64_t bytes)
    {
        const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
        int unitIndex = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024 && unitIndex < 4)
        {
            size /= 1024;
            unitIndex++;
        }

        std::wstringstream ss;
        ss << std::fixed << std::setprecision(1) << size << L" " << units[unitIndex];
        return hstring(ss.str());
    }

    // Helper to format date
    static hstring FormatDate(std::chrono::system_clock::time_point tp)
    {
        auto time_t_val = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_val;
        localtime_s(&tm_val, &time_t_val);

        std::wstringstream ss;
        ss << std::put_time(&tm_val, L"%Y-%m-%d %H:%M");
        return hstring(ss.str());
    }

    // RSSItemViewModel implementation
    RSSItemViewModel::RSSItemViewModel(const RSSItem& item, const std::wstring& feedId)
        : m_title(item.title)
        , m_description(item.description)
        , m_link(item.link)
        , m_torrentLink(RSSParser::ExtractTorrentLink(item))
        , m_pubDate(FormatDate(item.pubDate))
        , m_category(item.category)
        , m_fileSize(FormatFileSize(item.enclosureLength))
        , m_isDownloaded(item.isDownloaded)
        , m_feedId(feedId)
        , m_itemGuid(item.guid)
    {
    }

    winrt::event_token RSSItemViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void RSSItemViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void RSSItemViewModel::RaisePropertyChanged(hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
    }

    // RSSFeedViewModel implementation
    RSSFeedViewModel::RSSFeedViewModel(const RSSFeed& feed)
        : m_id(feed.id)
        , m_title(feed.title)
        , m_url(feed.url)
        , m_description(feed.description)
        , m_savePath(feed.savePath)
        , m_updateIntervalMinutes(static_cast<int32_t>(feed.updateInterval.count()))
        , m_autoDownload(feed.autoDownload)
        , m_filterPattern(feed.filterPattern)
        , m_enabled(feed.enabled)
        , m_lastUpdated(FormatDate(feed.lastUpdated))
    {
        for (const auto& item : feed.items)
        {
            m_items.Append(winrt::make<RSSItemViewModel>(item, feed.id));
        }
    }

    void RSSFeedViewModel::Title(hstring const& value)
    {
        if (m_title != value)
        {
            m_title = value;
            RaisePropertyChanged(L"Title");
        }
    }

    void RSSFeedViewModel::Url(hstring const& value)
    {
        if (m_url != value)
        {
            m_url = value;
            RaisePropertyChanged(L"Url");
        }
    }

    void RSSFeedViewModel::SavePath(hstring const& value)
    {
        if (m_savePath != value)
        {
            m_savePath = value;
            RaisePropertyChanged(L"SavePath");
        }
    }

    void RSSFeedViewModel::UpdateIntervalMinutes(int32_t value)
    {
        if (m_updateIntervalMinutes != value)
        {
            m_updateIntervalMinutes = value;
            RaisePropertyChanged(L"UpdateIntervalMinutes");
        }
    }

    void RSSFeedViewModel::AutoDownload(bool value)
    {
        if (m_autoDownload != value)
        {
            m_autoDownload = value;
            RaisePropertyChanged(L"AutoDownload");
        }
    }

    void RSSFeedViewModel::FilterPattern(hstring const& value)
    {
        if (m_filterPattern != value)
        {
            m_filterPattern = value;
            RaisePropertyChanged(L"FilterPattern");
        }
    }

    void RSSFeedViewModel::Enabled(bool value)
    {
        if (m_enabled != value)
        {
            m_enabled = value;
            RaisePropertyChanged(L"Enabled");
        }
    }

    int32_t RSSFeedViewModel::ItemCount() const
    {
        return static_cast<int32_t>(m_items.Size());
    }

    void RSSFeedViewModel::UpdateFromFeed(const RSSFeed& feed)
    {
        m_title = hstring(feed.title);
        m_description = hstring(feed.description);
        m_lastUpdated = FormatDate(feed.lastUpdated);

        m_items.Clear();
        for (const auto& item : feed.items)
        {
            m_items.Append(winrt::make<RSSItemViewModel>(item, feed.id));
        }

        RaisePropertyChanged(L"Title");
        RaisePropertyChanged(L"Description");
        RaisePropertyChanged(L"LastUpdated");
        RaisePropertyChanged(L"ItemCount");
    }

    winrt::event_token RSSFeedViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void RSSFeedViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void RSSFeedViewModel::RaisePropertyChanged(hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
    }

    // RSSViewModel implementation
    RSSViewModel::RSSViewModel()
    {
        m_dispatcherQueue = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        auto& manager = RSSManager::Instance();

        // Set up callbacks before initialization
        manager.SetFeedUpdatedCallback([this](const std::wstring& feedId) {
            OnFeedUpdated(feedId);
        });

        manager.SetNewItemCallback([this](const std::wstring& feedId, const RSSItem& item) {
            OnNewItem(feedId, item);
        });

        manager.SetErrorCallback([this](const std::wstring& feedId, const std::wstring& error) {
            OnError(feedId, error);
        });

        // Initialize asynchronously using fire_and_forget pattern
        InitializeManagerAsync();
    }

    winrt::fire_and_forget RSSViewModel::InitializeManagerAsync()
    {
        auto weak_this = get_weak();
        auto& manager = RSSManager::Instance();

        // Ensure initialization is complete (idempotent if already done by App)
        co_await manager.InitializeAsync();

        // Resume on UI thread to update UI
        if (auto strong_this = weak_this.get())
        {
            if (m_dispatcherQueue)
            {
                m_dispatcherQueue.TryEnqueue([weak_this]() {
                    if (auto strong = weak_this.get())
                    {
                        strong->LoadFeeds();
                        // Start is idempotent - safe to call even if already started by App
                        RSSManager::Instance().Start();
                    }
                });
            }
        }
    }

    void RSSViewModel::SelectedFeed(OpenNet::ViewModels::RSSFeedViewModel const& value)
    {
        if (m_selectedFeed != value)
        {
            m_selectedFeed = value;
            RaisePropertyChanged(L"SelectedFeed");
            RaisePropertyChanged(L"HasSelectedFeed");
        }
    }

    void RSSViewModel::SelectedItem(OpenNet::ViewModels::RSSItemViewModel const& value)
    {
        if (m_selectedItem != value)
        {
            m_selectedItem = value;
            RaisePropertyChanged(L"SelectedItem");
        }
    }

    void RSSViewModel::AddFeed(hstring const& url, hstring const& name, hstring const& savePath)
    {
        SetIsLoading(true);
        SetStatusMessage(L"Adding feed...");

        RSSSubscription sub;
        sub.url = std::wstring(url.c_str());
        sub.name = std::wstring(name.c_str());

        // Use Downloads folder as default if no save path specified
        if (savePath.empty())
        {
            try
            {
                auto userFolder = winrt::Windows::Storage::UserDataPaths::GetDefault();
                sub.savePath = std::wstring(userFolder.Downloads().c_str());
            }
            catch (...)
            {
                sub.savePath = L"";
            }
        }
        else
        {
            sub.savePath = std::wstring(savePath.c_str());
        }

        sub.updateInterval = std::chrono::minutes(30);
        sub.autoDownload = false;
        sub.enabled = true;

        if (RSSManager::Instance().AddSubscription(sub))
        {
            // Add placeholder feed to the list immediately
            // It will be updated when OnFeedUpdated callback fires
            RSSFeed placeholderFeed;
            placeholderFeed.id = sub.id.empty() ? L"" : sub.id;  // ID is generated by AddSubscription
            placeholderFeed.url = sub.url;
            placeholderFeed.title = sub.name.empty() ? L"Loading..." : sub.name;
            placeholderFeed.savePath = sub.savePath;
            placeholderFeed.updateInterval = sub.updateInterval;
            placeholderFeed.autoDownload = sub.autoDownload;
            placeholderFeed.enabled = sub.enabled;

            // Reload to get the actual feed with its generated ID
            LoadFeeds();
            SetStatusMessage(L"Fetching feed content...");
        }
        else
        {
            SetStatusMessage(L"Failed to add feed");
            SetIsLoading(false);
        }
    }

    void RSSViewModel::RemoveFeed(hstring const& feedId)
    {
        if (RSSManager::Instance().RemoveSubscription(std::wstring(feedId.c_str())))
        {
            // Find and remove from our list
            for (uint32_t i = 0; i < m_feeds.Size(); ++i)
            {
                if (m_feeds.GetAt(i).Id() == feedId)
                {
                    m_feeds.RemoveAt(i);
                    break;
                }
            }
            SetStatusMessage(L"Feed removed");
        }
    }

    void RSSViewModel::RefreshFeed(hstring const& feedId)
    {
        SetIsLoading(true);
        SetStatusMessage(L"Refreshing feed...");
        RSSManager::Instance().RefreshFeed(std::wstring(feedId.c_str()));
    }

    void RSSViewModel::RefreshAllFeeds()
    {
        SetIsLoading(true);
        SetStatusMessage(L"Refreshing all feeds...");
        RSSManager::Instance().RefreshAllFeeds();
    }

    void RSSViewModel::DownloadItem(OpenNet::ViewModels::RSSItemViewModel const& item)
    {
        if (!item) return;

        RSSItem coreItem;
        coreItem.guid = std::wstring(item.ItemGuid().c_str());
        coreItem.link = std::wstring(item.Link().c_str());
        coreItem.title = std::wstring(item.Title().c_str());

        RSSManager::Instance().DownloadItem(
            std::wstring(item.FeedId().c_str()),
            coreItem
        );

        SetStatusMessage(L"Download started: " + item.Title());
    }

    void RSSViewModel::UpdateFeedSettings(OpenNet::ViewModels::RSSFeedViewModel const& feed)
    {
        if (!feed) return;

        RSSSubscription sub;
        sub.id = std::wstring(feed.Id().c_str());
        sub.url = std::wstring(feed.Url().c_str());
        sub.name = std::wstring(feed.Title().c_str());
        sub.savePath = std::wstring(feed.SavePath().c_str());
        sub.updateInterval = std::chrono::minutes(feed.UpdateIntervalMinutes());
        sub.autoDownload = feed.AutoDownload();
        sub.filterPattern = std::wstring(feed.FilterPattern().c_str());
        sub.enabled = feed.Enabled();

        if (RSSManager::Instance().UpdateSubscription(sub))
        {
            SetStatusMessage(L"Feed settings updated");
        }
    }

    winrt::event_token RSSViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void RSSViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void RSSViewModel::RaisePropertyChanged(hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
    }

    void RSSViewModel::LoadFeeds()
    {
        m_feeds.Clear();
        auto feeds = RSSManager::Instance().GetAllFeeds();
        for (const auto& feed : feeds)
        {
            m_feeds.Append(winrt::make<RSSFeedViewModel>(feed));
        }
    }

    void RSSViewModel::OnFeedUpdated(const std::wstring& feedId)
    {
        if (!m_dispatcherQueue) return;

        m_dispatcherQueue.TryEnqueue([this, feedId]() {
            auto feedOpt = RSSManager::Instance().GetFeed(feedId);
            if (!feedOpt) return;

            // Find and update the feed in our list
            bool found = false;
            for (uint32_t i = 0; i < m_feeds.Size(); ++i)
            {
                auto feed = m_feeds.GetAt(i);
                if (std::wstring(feed.Id().c_str()) == feedId)
                {
                    auto impl = winrt::get_self<RSSFeedViewModel>(feed);
                    impl->UpdateFromFeed(*feedOpt);
                    found = true;
                    break;
                }
            }

            // If feed wasn't in our list yet, add it (race condition protection)
            if (!found)
            {
                m_feeds.Append(winrt::make<RSSFeedViewModel>(*feedOpt));
            }

            SetIsLoading(false);
            SetStatusMessage(L"Feed updated");
        });
    }

    void RSSViewModel::OnNewItem(const std::wstring& feedId, const RSSItem& item)
    {
        if (!m_dispatcherQueue) return;

        m_dispatcherQueue.TryEnqueue([this, feedId, item]() {
            SetStatusMessage(L"New item: " + hstring(item.title));
        });
    }

    void RSSViewModel::OnError(const std::wstring& feedId, const std::wstring& error)
    {
        if (!m_dispatcherQueue) return;

        m_dispatcherQueue.TryEnqueue([this, error]() {
            SetIsLoading(false);
            SetStatusMessage(L"Error: " + hstring(error));
        });
    }

    void RSSViewModel::SetStatusMessage(hstring const& message)
    {
        m_statusMessage = message;
        RaisePropertyChanged(L"StatusMessage");
    }

    void RSSViewModel::SetIsLoading(bool value)
    {
        if (m_isLoading != value)
        {
            m_isLoading = value;
            RaisePropertyChanged(L"IsLoading");
        }
    }
}
