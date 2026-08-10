#include "XamlWorkaround.h"
#include "RSSViewModel.h"
#include "ViewModels/RSSItemViewModel.g.cpp"
#include "ViewModels/RSSFeedViewModel.g.cpp"
#include "ViewModels/RSSViewModel.g.cpp"

import Core.Utils.Misc;
import OpenNet.Core.RSS.RSSManager;
import OpenNet.Core.RSS.RSSParser;
import OpenNet.Core.Utils.Message;
import winrt.Windows.Storage;

using namespace ::OpenNet::Core::RSS;

namespace winrt::OpenNet::ViewModels::implementation
{
	// Helper to format date
	static winrt::hstring FormatDate(std::chrono::system_clock::time_point tp)
	{
		auto zt = std::chrono::zoned_time{
			std::chrono::current_zone(),
			tp
		};

		return hstring(
			std::format(L"{:%Y-%m-%d %H:%M}", zt)
		);
	}
	// RSSItemViewModel implementation
	RSSItemViewModel::RSSItemViewModel(const RSSItem& item, const std::wstring& feedId)
		: m_title(item.title)
		, m_description(item.description)
		, m_link(item.link)
		, m_torrentLink(RSSParser::ExtractTorrentLink(item))
		, m_pubDate(FormatDate(item.pubDate))
		, m_category(item.category)
		// RSS enclosure length is optional and describes the .torrent enclosure,
		// not the payload declared by the torrent. Do not present a missing value
		// as a zero-byte download.
		, m_fileSize(item.enclosureLength == 0
					 ? winrt::hstring{ L"—" }
					 : winrt::hstring{ ::Core::Utils::Misc::friendlyUnitCompact(item.enclosureLength) })
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

		// Set up callbacks with weak_ref so background RSS fetches won't
		// crash when the ViewModel (and its DispatcherQueue) is already gone.
		manager.SetFeedUpdatedCallback([weak = get_weak()](const std::wstring& feedId)
		{
			if (auto self = weak.get())
				self->OnFeedUpdated(feedId);
		});

		manager.SetNewItemCallback([weak = get_weak()](const std::wstring& feedId, const RSSItem& item)
		{
			if (auto self = weak.get())
				self->OnNewItem(feedId, item);
		});

		manager.SetErrorCallback([weak = get_weak()](const std::wstring& feedId, const std::wstring& error)
		{
			if (auto self = weak.get())
				self->OnError(feedId, error);
		});

		// Initialize asynchronously using fire_and_forget pattern
		InitializeManagerAsync();
	}

	RSSViewModel::~RSSViewModel()
	{
		// Clear callbacks so background RSS threads won't call into destroyed UI.
		auto& manager = RSSManager::Instance();
		manager.SetFeedUpdatedCallback(nullptr);
		manager.SetNewItemCallback(nullptr);
		manager.SetErrorCallback(nullptr);
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
				m_dispatcherQueue.TryEnqueue([weak_this]()
				{
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
		SetStatusMessage(ResourceGetString(L"ViewRSSViewModelAddingFeed"));

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
			SetStatusMessage(ResourceGetString(L"ViewRSSViewModelFetchingFeed"));
		}
		else
		{
			SetStatusMessage(ResourceGetString(L"ViewRSSViewModelFailedToAddFeed"));
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
			SetStatusMessage(ResourceGetString(L"ViewRSSViewModelFeedRemoved"));
		}
	}

	void RSSViewModel::RefreshFeed(hstring const& feedId)
	{
		SetIsLoading(true);
		SetStatusMessage(ResourceGetString(L"ViewRSSViewModelRefreshingFeed"));
		RSSManager::Instance().RefreshFeed(std::wstring(feedId.c_str()));
	}

	void RSSViewModel::RefreshAllFeeds()
	{
		SetIsLoading(true);
		SetStatusMessage(ResourceGetString(L"ViewRSSViewModelRefreshingAllFeeds"));
		RSSManager::Instance().RefreshAllFeeds();
	}

	void RSSViewModel::DownloadItem(OpenNet::ViewModels::RSSItemViewModel const& item)
	{
		if (!item) return;

		RSSItem coreItem;
		coreItem.guid = std::wstring(item.ItemGuid().c_str());
		coreItem.link = std::wstring(item.Link().c_str());
		coreItem.enclosureUrl = std::wstring(item.TorrentLink().c_str());
		coreItem.title = std::wstring(item.Title().c_str());

		RSSManager::Instance().DownloadItem(
			std::wstring(item.FeedId().c_str()),
			coreItem
		);

		SetStatusMessage(ResourceGetString(L"ViewRSSViewModelDownloadStarted") + L" " + item.Title());
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
			SetStatusMessage(ResourceGetString(L"ViewRSSViewModelFeedSettingsUpdated"));
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

		m_dispatcherQueue.TryEnqueue([this, feedId]()
		{
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
			SetStatusMessage(ResourceGetString(L"ViewRSSViewModelFeedUpdated"));
		});
	}

	void RSSViewModel::OnNewItem(const std::wstring& feedId, const RSSItem& item)
	{
		if (!m_dispatcherQueue) return;

		m_dispatcherQueue.TryEnqueue([this, feedId, item]()
		{
			SetStatusMessage(ResourceGetString(L"ViewRSSViewModelNewItem") + L" " + hstring(item.title));
		});
	}

	void RSSViewModel::OnError(const std::wstring& /*feedId*/, const std::wstring& error)
	{
		if (!m_dispatcherQueue) return;

		m_dispatcherQueue.TryEnqueue([this, error]()
		{
			SetIsLoading(false);
			SetStatusMessage(ResourceGetString(L"ViewRSSViewModelError") + L" " + hstring(error));
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
