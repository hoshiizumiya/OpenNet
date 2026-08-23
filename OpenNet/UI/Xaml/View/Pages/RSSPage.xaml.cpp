#include "XamlWorkaround.h"
#include "RSSPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/RSSPage.g.cpp")
#include "UI/Xaml/View/Pages/RSSPage.g.cpp"
#endif

#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.xaml.h"

import OpenNet.Helpers.WindowHelper;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.RSS.RSSLinkResolver;
import OpenNet.Core.Utils.Message;
import winrt.OpenNet.UI.Xaml.View.Dialog;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Controls.Primitives;
import winrt.Microsoft.UI.Xaml.Input;
import winrt.Windows.System;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	RSSPage::RSSPage()
	{
		m_viewModel = winrt::make<ViewModels::implementation::RSSViewModel>();

		// Defer UI initialization to Loaded event per C++/WinRT guidelines
		Loaded([this](IInspectable const& sender, RoutedEventArgs const& e)
		{
			try
			{
				auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
				int maxItems = static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "max_items_per_feed", 100));
				MaxItemsPerFeedBox().Value(static_cast<double>(maxItems));
				m_previousSelectedIndex = static_cast<int32_t>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "item_double_click_action", 0));
				m_enableWebViewPreview = db.GetBool(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "enable_webview_preview", false).value();
				LinkOpenBehaviorBox().SelectedIndex(static_cast<int32_t>(
					db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_RSS,
							  "link_open_behavior", 0)));
				m_loadingSettings = false;
			}
			catch (...)
			{
			}
		});
		Unloaded([weak = get_weak()](auto const&, auto const&)
		{
		});
	}

	void RSSPage::RSSPreviewFlyout_Opened(IInspectable const&, IInspectable const&)
	{
		m_previewOpen = true;
	}

	void RSSPage::RSSPreviewFlyout_Closed(IInspectable const&, IInspectable const&)
	{
		m_previewOpen = false;
		m_previewPointerOverFlyout = false;
		if (!m_previewPointerOverItem) m_previewTarget = nullptr;
	}

	void RSSPage::AddFeedButton_Click(::winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		ShowAddFeedDialog();
	}

	void RSSPage::RefreshAllButton_Click(::winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		m_viewModel.RefreshAllFeeds();
	}

	void RSSPage::RefreshFeedButton_Click(::winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		if (auto feed = m_viewModel.SelectedFeed())
		{
			m_viewModel.RefreshFeed(feed.Id());
		}
	}

	void RSSPage::FeedSettingsButton_Click(::winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		ShowFeedSettingsDialog();
	}

	void RSSPage::RemoveFeedButton_Click(::winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		ConfirmRemoveFeed();
	}

	void RSSPage::DownloadItemButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
	{
		if (auto button = sender.try_as<Button>())
		{
			if (auto item = button.Tag().try_as<ViewModels::RSSItemViewModel>())
			{
				// Get the torrent link
				hstring torrentLink = item.TorrentLink();
				if (torrentLink.empty())
				{
					// Fallback to regular link if no torrent link
					torrentLink = item.Link();
				}

				if (!torrentLink.empty())
				{
					ProcessAndShowTorrentMetadataWindow(torrentLink);
					m_viewModel.SetStatusMessage(L"Opening download: " + item.Title());
				}
				else
				{
					m_viewModel.SetStatusMessage(L"No download link available for: " + item.Title());
				}
			}
		}
	}

	winrt::fire_and_forget RSSPage::ProcessAndShowTorrentMetadataWindow(hstring torrentLink)
	{
		auto lifetime = get_strong();
		if (torrentLink.empty())
		{
			co_return;
		}

		try
		{
			auto source = co_await ::OpenNet::Core::RSS::RSSLinkResolver::ResolveTorrentSourceAsync(torrentLink);
			// Create a shared_ptr to keep the window alive during async operations
			auto checkWindow = winrt::make_self<winrt::OpenNet::UI::Xaml::View::Windows::implementation::TorrentCheckModalWindow>(source);
			::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(*checkWindow);
			checkWindow->Activate();
			// The window manages its own lifetime - it will close when user closes it or operations complete
		}
		catch (winrt::hresult_error const& ex)
		{
			m_viewModel.SetStatusMessage(L"Unable to open torrent: " + ex.message());
		}
		catch (const std::exception& ex)
		{
			// Log error if needed
			OutputDebugStringW(L"Error creating torrent check window: ");
			OutputDebugStringW(winrt::to_hstring(ex.what()).c_str());
			OutputDebugStringW(L"\n");
		}
		catch (...)
		{
			OutputDebugStringW(L"Unknown error creating torrent check window\n");
		}
	}

	OpenNet::ViewModels::RSSItemViewModel RSSPage::ItemFromSender(IInspectable const& sender) const
	{
		if (auto element = sender.try_as<FrameworkElement>())
		{
			return element.Tag().try_as<ViewModels::RSSItemViewModel>();
		}
		return nullptr;
	}

	void RSSPage::RSSItem_RightTapped(IInspectable const& sender, Input::RightTappedRoutedEventArgs const&)
	{
		if (auto item = ItemFromSender(sender))
		{
			ItemsListView().SelectedItem(item);
		}
	}

	void RSSPage::RSSItem_DoubleTapped(IInspectable const& sender, Input::DoubleTappedRoutedEventArgs const& args)
	{
		if (auto item = ItemFromSender(sender))
		{
			ItemsListView().SelectedItem(item);
			if (m_previousSelectedIndex == 0)
			{
				ProcessAndShowTorrentMetadataWindow(item.TorrentLink().empty() ? item.Link() : item.TorrentLink());
			}
			else
			{
				OpenItemInBrowser(item);
			}
			args.Handled(true);
		}
	}

	void RSSPage::RSSItem_PointerEntered(IInspectable const& sender, Input::PointerRoutedEventArgs const&)
	{
		if (m_enableWebViewPreview == false)
			return; // Only show preview if double-click action is "Open in browser"
		auto item = ItemFromSender(sender);
		auto target = sender.try_as<FrameworkElement>();
		if (!item || !target || item.Link().empty()) return;

		m_previewTarget = target;
		if (m_previewUrl != item.Link())
		{
			m_previewUrl = item.Link();
			RSSPreviewWebView().Source(winrt::Windows::Foundation::Uri(m_previewUrl));
			RSSPreviewFlyout().ShowAt(m_previewTarget);
		}
	}


	void RSSPage::AddSelectedItem_Click(IInspectable const&, RoutedEventArgs const&)
	{
		if (auto item = m_viewModel.SelectedItem())
		{
			ProcessAndShowTorrentMetadataWindow(item.TorrentLink().empty() ? item.Link() : item.TorrentLink());
		}
	}

	void RSSPage::ShowSelectedItemDetails_Click(IInspectable const&, RoutedEventArgs const&)
	{
		OpenItemInBrowser(m_viewModel.SelectedItem());
	}

	void RSSPage::OpenSelectedItemInBrowser_Click(IInspectable const&, RoutedEventArgs const&)
	{
		OpenItemExternally(m_viewModel.SelectedItem());
	}

	winrt::fire_and_forget RSSPage::OpenItemInBrowser(ViewModels::RSSItemViewModel item)
	{
		auto lifetime = get_strong();
		if (!item || item.Link().empty()) co_return;
		try
		{
			if (LinkOpenBehaviorBox().SelectedIndex() == 1)
			{
				co_await winrt::Windows::System::Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri(item.Link()));
				co_return;
			}

			if (!m_browserWindow)
			{
				m_browserWindow = winrt::make<winrt::OpenNet::UI::Xaml::View::Windows::implementation::RSSBrowserWindow>();
				::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(m_browserWindow.Window());
				m_browserWindow.Closed([weak = get_weak()](auto const&, auto const&)
				{
					if (auto self = weak.get()) self->m_browserWindow = nullptr;
				});
			}
			// Activate first so WebView2 can bind its controller to a real HWND.
			m_browserWindow.Activate();
			m_browserWindow.AddNewTab(item.Link(), item.Title());
		}
		catch (...)
		{
			m_viewModel.SetStatusMessage(L"Unable to open the article link.");
		}
	}

	winrt::fire_and_forget RSSPage::OpenItemExternally(ViewModels::RSSItemViewModel item)
	{
		auto lifetime = get_strong();
		if (!item || item.Link().empty()) co_return;
		try
		{
			co_await winrt::Windows::System::Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri(item.Link()));
		}
		catch (...)
		{
			m_viewModel.SetStatusMessage(L"Unable to open the article link.");
		}
	}

	void RSSPage::LinkOpenBehaviorBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_loadingSettings) return;
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(
			::OpenNet::Core::AppSettingsDatabase::CAT_RSS,
			"link_open_behavior", LinkOpenBehaviorBox().SelectedIndex());
	}

	winrt::fire_and_forget RSSPage::ShowAddFeedDialog()
	{
		try
		{
			auto dialog = winrt::make<winrt::OpenNet::UI::Xaml::View::Dialog::implementation::AddRSSFeedDialog>();
			dialog.XamlRoot(this->XamlRoot());
			auto result = co_await dialog.ShowAsync();

			if (result == ContentDialogResult::Primary)
			{
				auto url = dialog.FeedUrl();
				if (!url.empty())
				{
					m_viewModel.AddFeed(
						url,
						dialog.FeedName(),
						dialog.FeedSavePath()
					);
				}
			}
		}
		catch (winrt::hresult_error const& ex)
		{
			OutputDebugStringW((L"ShowAddFeedDialog error: " + std::wstring(ex.message().c_str()) + L"\n").c_str());
		}
	}

	winrt::fire_and_forget RSSPage::ShowFeedSettingsDialog()
	{
		auto feed = m_viewModel.SelectedFeed();
		if (!feed) co_return;

		if (!this->XamlRoot())
		{
			co_return;
		}

		try
		{
			auto dialog = winrt::make<winrt::OpenNet::UI::Xaml::View::Dialog::implementation::RSSFeedSettingsDialog>();
			dialog.SetFeed(feed);
			dialog.XamlRoot(this->XamlRoot());
			auto result = co_await dialog.ShowAsync();

			if (result == ContentDialogResult::Primary)
			{
				auto const previousTitle = feed.Title();
				auto const previousUrl = feed.Url();
				auto const previousSavePath = feed.SavePath();
				auto const previousInterval = feed.UpdateIntervalMinutes();
				auto const previousEnabled = feed.Enabled();
				auto const previousAutoDownload = feed.AutoDownload();
				auto const previousFilter = feed.FilterPattern();

				feed.Title(dialog.FeedTitle());
				feed.Url(dialog.FeedUrl());
				feed.SavePath(dialog.FeedSavePath());
				feed.UpdateIntervalMinutes(dialog.UpdateIntervalMinutes());
				feed.Enabled(dialog.Enabled());
				feed.AutoDownload(dialog.AutoDownload());
				feed.FilterPattern(dialog.FilterPattern());

				if (!m_viewModel.UpdateFeedSettings(feed))
				{
					feed.Title(previousTitle);
					feed.Url(previousUrl);
					feed.SavePath(previousSavePath);
					feed.UpdateIntervalMinutes(previousInterval);
					feed.Enabled(previousEnabled);
					feed.AutoDownload(previousAutoDownload);
					feed.FilterPattern(previousFilter);
					co_return;
				}

				m_previousSelectedIndex = dialog.ItemDoubleClickAction();
				m_enableWebViewPreview = dialog.EnableWebViewPreview();
				if (!m_enableWebViewPreview && m_previewOpen) RSSPreviewFlyout().Hide();
				auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
				database.SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "item_double_click_action", m_previousSelectedIndex);
				database.SetBool(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "enable_webview_preview", m_enableWebViewPreview);
			}
		}
		catch (winrt::hresult_error const& ex)
		{
			OutputDebugStringW((L"ShowFeedSettingsDialog error: " + std::wstring(ex.message().c_str()) + L"\n").c_str());
		}
	}

	winrt::fire_and_forget RSSPage::ConfirmRemoveFeed()
	{
		auto feed = m_viewModel.SelectedFeed();
		if (!feed) co_return;

		// Ensure XamlRoot is available
		if (!this->XamlRoot())
		{
			co_return;
		}

		try
		{
			winrt::OpenNet::UI::Xaml::View::Dialog::ConfirmationDialog dialog;
			dialog.Configure(ResourceGetString(L"ViewRSSPageRemoveFeedTitle"), ResourceGetString(L"ViewRSSPageRemoveFeedConfirmationPrefix") + feed.Title() + ResourceGetString(L"ViewRSSPageRemoveFeedConfirmationSuffix"), {}, ResourceGetString(L"CommonRemove"), ResourceGetString(L"CommonCancel"), true, false, {});
			dialog.XamlRoot(this->XamlRoot());

			auto result = co_await dialog.ShowAsync();

			if (result == ContentDialogResult::Primary)
			{
				m_viewModel.RemoveFeed(feed.Id());
			}
		}
		catch (winrt::hresult_error const& ex)
		{
			OutputDebugStringW((L"ConfirmRemoveFeed error: " + std::wstring(ex.message().c_str()) + L"\n").c_str());
		}
	}

	void RSSPage::MaxItemsPerFeedBox_ValueChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const& /*sender*/,
												  winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args)
	{
		auto newVal = args.NewValue();
		// NaN means the user cleared the box
		if (std::isnan(newVal)) return;

		int value = static_cast<int>(newVal);
		if (value < 10) value = 10;

		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		db.SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "max_items_per_feed", value);
	}
}
