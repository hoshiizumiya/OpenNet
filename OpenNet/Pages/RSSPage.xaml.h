#pragma once
#include "Pages/RSSPage.g.h"
#include "ViewModels/RSSViewModel.h"
#include "UI/Xaml/View/Dialog/AddRSSFeedDialog.xaml.h"
#include "UI/Xaml/View/Dialog/RSSFeedSettingsDialog.xaml.h"

namespace winrt::OpenNet::Pages::implementation
{
	struct RSSPage : RSSPageT<RSSPage>
	{
		RSSPage();

		OpenNet::ViewModels::RSSViewModel ViewModel() const { return m_viewModel; }

		// Event handlers
		void AddFeedButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void RefreshAllButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void RefreshFeedButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void FeedSettingsButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void RemoveFeedButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void DownloadItemButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);

	private:
		winrt::fire_and_forget ShowAddFeedDialog();
		winrt::fire_and_forget ShowFeedSettingsDialog();
		winrt::fire_and_forget ConfirmRemoveFeed();
		void ProcessAndShowTorrentMetadataWindow(hstring const& torrentLink);

		OpenNet::ViewModels::RSSViewModel m_viewModel{ nullptr };
	};
}

namespace winrt::OpenNet::Pages::factory_implementation
{
	struct RSSPage : RSSPageT<RSSPage, implementation::RSSPage> {};
}
