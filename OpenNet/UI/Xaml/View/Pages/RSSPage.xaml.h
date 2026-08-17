#pragma once
#include "UI/Xaml/View/Pages/RSSPage.g.h"
#include "ViewModels/RSSViewModel.h"
#include "UI/Xaml/View/Dialog/AddRSSFeedDialog.xaml.h"
#include "UI/Xaml/View/Dialog/RSSFeedSettingsDialog.xaml.h"
#include "UI/Xaml/View/Windows/RSSBrowserWindow.xaml.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct RSSPage : RSSPageT<RSSPage>
	{
		RSSPage();
		~RSSPage();

		OpenNet::ViewModels::RSSViewModel ViewModel() const { return m_viewModel; }

		// Event handlers
		void AddFeedButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void RefreshAllButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void RefreshFeedButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void FeedSettingsButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void RemoveFeedButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void DownloadItemButton_Click(::winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void AddSelectedItem_Click(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void ShowSelectedItemDetails_Click(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OpenSelectedItemInBrowser_Click(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void RSSItem_DoubleTapped(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const&);
		void RSSItem_RightTapped(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const&);
		void RSSItem_PointerEntered(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&);
		void RSSItem_PointerExited(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&);
		void RSSPreviewFlyout_Opened(::winrt::Windows::Foundation::IInspectable const&, ::winrt::Windows::Foundation::IInspectable const&);
		void RSSPreviewFlyout_Closed(::winrt::Windows::Foundation::IInspectable const&, ::winrt::Windows::Foundation::IInspectable const&);
		void RSSPreviewHost_PointerEntered(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&);
		void RSSPreviewHost_PointerExited(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&);
		void ItemDoubleClickActionBox_SelectionChanged(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
		void LinkOpenBehaviorBox_SelectionChanged(::winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
		void MaxItemsPerFeedBox_ValueChanged(Microsoft::UI::Xaml::Controls::NumberBox const& sender, Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);

	private:
		winrt::fire_and_forget ShowAddFeedDialog();
		winrt::fire_and_forget ShowFeedSettingsDialog();
		winrt::fire_and_forget ConfirmRemoveFeed();
		winrt::fire_and_forget ProcessAndShowTorrentMetadataWindow(hstring torrentLink);
		winrt::fire_and_forget OpenItemInBrowser(OpenNet::ViewModels::RSSItemViewModel item);
		winrt::fire_and_forget OpenItemExternally(OpenNet::ViewModels::RSSItemViewModel item);
		OpenNet::ViewModels::RSSItemViewModel ItemFromSender(::winrt::Windows::Foundation::IInspectable const& sender) const;

		OpenNet::ViewModels::RSSViewModel m_viewModel{ nullptr };
		Microsoft::UI::Xaml::Controls::Flyout m_previewFlyout{ nullptr };
		Microsoft::UI::Xaml::Controls::WebView2 m_previewWebView{ nullptr };
		Microsoft::UI::Xaml::Controls::Border m_previewHost{ nullptr };
		Microsoft::UI::Dispatching::DispatcherQueueTimer m_previewTimer{ nullptr };
		Microsoft::UI::Dispatching::DispatcherQueueTimer m_previewCloseTimer{ nullptr };
		Microsoft::UI::Xaml::FrameworkElement m_previewTarget{ nullptr };
		winrt::OpenNet::UI::Xaml::View::Windows::RSSBrowserWindow m_browserWindow{ nullptr };
		winrt::hstring m_previewUrl;
		bool m_previewPointerOverItem{};
		bool m_previewPointerOverFlyout{};
		bool m_previewOpen{};
		bool m_loadingSettings{ true };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct RSSPage : RSSPageT<RSSPage, implementation::RSSPage> {};
}
