#include "pch.h"
#include "RSSPage.xaml.h"
#if __has_include("Pages/RSSPage.g.cpp")
#include "Pages/RSSPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::Pages::implementation
{
	RSSPage::RSSPage()
	{
		m_viewModel = winrt::make<ViewModels::implementation::RSSViewModel>();
		InitializeComponent();
	}

	void RSSPage::AddFeedButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		ShowAddFeedDialog();
	}

	void RSSPage::RefreshAllButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		m_viewModel.RefreshAllFeeds();
	}

	void RSSPage::RefreshFeedButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		if (auto feed = m_viewModel.SelectedFeed())
		{
			m_viewModel.RefreshFeed(feed.Id());
		}
	}

	void RSSPage::FeedSettingsButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		ShowFeedSettingsDialog();
	}

	void RSSPage::RemoveFeedButton_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		ConfirmRemoveFeed();
	}

	void RSSPage::DownloadItemButton_Click(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
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

	void RSSPage::ProcessAndShowTorrentMetadataWindow(hstring const& torrentLink)
	{
		if (torrentLink.empty())
		{
			return;
		}

		try
		{
			// Create a shared_ptr to keep the window alive during async operations
			auto checkWindow = winrt::make_self<winrt::OpenNet::UI::Xaml::View::Windows::implementation::TorrentCheckModalWindow>(torrentLink);
			checkWindow->Activate();
			// The window manages its own lifetime - it will close when user closes it or operations complete
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
				feed.Title(dialog.FeedTitle());
				feed.Url(dialog.FeedUrl());
				feed.SavePath(dialog.FeedSavePath());
				feed.UpdateIntervalMinutes(dialog.UpdateIntervalMinutes());
				feed.AutoDownload(dialog.AutoDownload());
				feed.FilterPattern(dialog.FilterPattern());

				m_viewModel.UpdateFeedSettings(feed);
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
			ContentDialog dialog;
			dialog.Title(box_value(L"Remove Feed"));
			dialog.Content(box_value(L"Are you sure you want to remove \"" + feed.Title() + L"\"?"));
			dialog.PrimaryButtonText(L"Remove");
			dialog.SecondaryButtonText(L"Cancel");
			dialog.DefaultButton(ContentDialogButton::Secondary);
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
}
