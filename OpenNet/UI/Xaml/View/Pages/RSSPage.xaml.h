#pragma once
#include "UI/Xaml/View/Pages/RSSPage.g.h"
#include "ViewModels/RSSViewModel.h"
#include "UI/Xaml/View/Dialog/AddRSSFeedDialog.xaml.h"
#include "UI/Xaml/View/Dialog/RSSFeedSettingsDialog.xaml.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct RSSPage : RSSPageT<RSSPage>
	{
		RSSPage();

		OpenNet::ViewModels::RSSViewModel ViewModel() const { return m_viewModel; }

		// Event handlers
		void AddFeedButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void RefreshAllButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void RefreshFeedButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void FeedSettingsButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void RemoveFeedButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void DownloadItemButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void MaxItemsPerFeedBox_ValueChanged(Microsoft::UI::Xaml::Controls::NumberBox const& sender, Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);

	private:
		winrt::fire_and_forget ShowAddFeedDialog();
		winrt::fire_and_forget ShowFeedSettingsDialog();
		winrt::fire_and_forget ConfirmRemoveFeed();
		void ProcessAndShowTorrentMetadataWindow(hstring const& torrentLink);

		OpenNet::ViewModels::RSSViewModel m_viewModel{ nullptr };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct RSSPage : RSSPageT<RSSPage, implementation::RSSPage> {};
}
