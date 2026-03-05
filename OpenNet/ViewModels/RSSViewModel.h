#pragma once
#include "ViewModels/RSSItemViewModel.g.h"
#include "ViewModels/RSSFeedViewModel.g.h"
#include "ViewModels/RSSViewModel.g.h"
#include "Core/RSS/RSSTypes.h"
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::OpenNet::ViewModels::implementation
{
    struct RSSItemViewModel : RSSItemViewModelT<RSSItemViewModel>
    {
        RSSItemViewModel() = default;
        RSSItemViewModel(const ::OpenNet::Core::RSS::RSSItem& item, const std::wstring& feedId);

        hstring Title() const { return m_title; }
        hstring Description() const { return m_description; }
        hstring Link() const { return m_link; }
        hstring TorrentLink() const { return m_torrentLink; }
        hstring PubDate() const { return m_pubDate; }
        hstring Category() const { return m_category; }
        hstring FileSize() const { return m_fileSize; }
        bool IsDownloaded() const { return m_isDownloaded; }
        hstring FeedId() const { return m_feedId; }
        hstring ItemGuid() const { return m_itemGuid; }

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        void RaisePropertyChanged(hstring const& propertyName);

        hstring m_title;
        hstring m_description;
        hstring m_link;
        hstring m_torrentLink;
        hstring m_pubDate;
        hstring m_category;
        hstring m_fileSize;
        bool m_isDownloaded{ false };
        hstring m_feedId;
        hstring m_itemGuid;

        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };

    struct RSSFeedViewModel : RSSFeedViewModelT<RSSFeedViewModel>
    {
        RSSFeedViewModel() = default;
        RSSFeedViewModel(const ::OpenNet::Core::RSS::RSSFeed& feed);

        hstring Id() const { return m_id; }
        hstring Title() const { return m_title; }
        void Title(hstring const& value);
        hstring Url() const { return m_url; }
        void Url(hstring const& value);
        hstring Description() const { return m_description; }
        hstring SavePath() const { return m_savePath; }
        void SavePath(hstring const& value);
        int32_t UpdateIntervalMinutes() const { return m_updateIntervalMinutes; }
        void UpdateIntervalMinutes(int32_t value);
        bool AutoDownload() const { return m_autoDownload; }
        void AutoDownload(bool value);
        hstring FilterPattern() const { return m_filterPattern; }
        void FilterPattern(hstring const& value);
        bool Enabled() const { return m_enabled; }
        void Enabled(bool value);
        hstring LastUpdated() const { return m_lastUpdated; }
        int32_t ItemCount() const;
        Windows::Foundation::Collections::IObservableVector<OpenNet::ViewModels::RSSItemViewModel> Items() const { return m_items; }

        void UpdateFromFeed(const ::OpenNet::Core::RSS::RSSFeed& feed);

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        void RaisePropertyChanged(hstring const& propertyName);

        hstring m_id;
        hstring m_title;
        hstring m_url;
        hstring m_description;
        hstring m_savePath;
        int32_t m_updateIntervalMinutes{ 30 };
        bool m_autoDownload{ false };
        hstring m_filterPattern;
        bool m_enabled{ true };
        hstring m_lastUpdated;
        Windows::Foundation::Collections::IObservableVector<OpenNet::ViewModels::RSSItemViewModel> m_items{
            winrt::single_threaded_observable_vector<OpenNet::ViewModels::RSSItemViewModel>()
        };

        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };

    struct RSSViewModel : RSSViewModelT<RSSViewModel>
    {
        RSSViewModel();
        ~RSSViewModel();

        Windows::Foundation::Collections::IObservableVector<OpenNet::ViewModels::RSSFeedViewModel> Feeds() const { return m_feeds; }
        OpenNet::ViewModels::RSSFeedViewModel SelectedFeed() const { return m_selectedFeed; }
        void SelectedFeed(OpenNet::ViewModels::RSSFeedViewModel const& value);
        OpenNet::ViewModels::RSSItemViewModel SelectedItem() const { return m_selectedItem; }
        void SelectedItem(OpenNet::ViewModels::RSSItemViewModel const& value);
        bool HasSelectedFeed() const { return m_selectedFeed != nullptr; }
        bool IsLoading() const { return m_isLoading; }
        hstring StatusMessage() const { return m_statusMessage; }

        void AddFeed(hstring const& url, hstring const& name, hstring const& savePath);
        void RemoveFeed(hstring const& feedId);
        void RefreshFeed(hstring const& feedId);
        void RefreshAllFeeds();
        void DownloadItem(OpenNet::ViewModels::RSSItemViewModel const& item);
        void UpdateFeedSettings(OpenNet::ViewModels::RSSFeedViewModel const& feed);
        void SetStatusMessage(hstring const& message);

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        void RaisePropertyChanged(hstring const& propertyName);
        void LoadFeeds();
        winrt::fire_and_forget InitializeManagerAsync();
        void OnFeedUpdated(const std::wstring& feedId);
        void OnNewItem(const std::wstring& feedId, const ::OpenNet::Core::RSS::RSSItem& item);
        void OnError(const std::wstring& feedId, const std::wstring& error);
        void SetIsLoading(bool value);

        Windows::Foundation::Collections::IObservableVector<OpenNet::ViewModels::RSSFeedViewModel> m_feeds{
            winrt::single_threaded_observable_vector<OpenNet::ViewModels::RSSFeedViewModel>()
        };
        OpenNet::ViewModels::RSSFeedViewModel m_selectedFeed{ nullptr };
        OpenNet::ViewModels::RSSItemViewModel m_selectedItem{ nullptr };
        bool m_isLoading{ false };
        hstring m_statusMessage;

        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct RSSItemViewModel : RSSItemViewModelT<RSSItemViewModel, implementation::RSSItemViewModel> {};
    struct RSSFeedViewModel : RSSFeedViewModelT<RSSFeedViewModel, implementation::RSSFeedViewModel> {};
    struct RSSViewModel : RSSViewModelT<RSSViewModel, implementation::RSSViewModel> {};
}
