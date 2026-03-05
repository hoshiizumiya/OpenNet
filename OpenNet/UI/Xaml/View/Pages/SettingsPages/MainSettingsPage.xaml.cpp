#include "pch.h"
#include "MainSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/MainSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/MainSettingsPage.g.cpp"
#endif

#include "Folder.h"
#include "SettingsPage.xaml.h"
#include "AboutPage.xaml.h"
#include "ThemesSettingsPage.xaml.h"
#include "UI/Xaml/View/Pages/NetworkSettingsPage.xaml.h"
#include "UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.xaml.h"
#include "UI/Xaml/View/Pages/SettingsPages/DownloadSettingsPage.xaml.h"
#include "UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	MainSettingsPage* MainSettingsPage::s_current{ nullptr };

	MainSettingsPage::MainSettingsPage()
	{
		InitializeComponent();
		s_current = this;

		// Defer UI element initialization to Loaded event per C++/WinRT guidelines
		Loaded([this](IInspectable const& sender, RoutedEventArgs const& e)
			{
				try
				{
					DataContext() = *this;
					SettingsNavView().SelectedItem(GeneralNavItem());

					// Populate MainSettingsPageBar with a single Folder item
					auto items = single_threaded_observable_vector<IInspectable>();
					auto folder = winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::Folder();
					folder.Name(L"Settings");
					items.Append(folder);
					MainSettingsPageBar().ItemsSource(items);
				}
				catch (...) {}
			});
	}

	MainSettingsPage::~MainSettingsPage()
	{
		if (s_current == this)
		{
			s_current = nullptr;
		}
	}

	MainSettingsPage* MainSettingsPage::Current()
	{
		return s_current;
	}

	void MainSettingsPage::UpdateSettingsBarItems(winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::Folder> const& items)
	{
		MainSettingsPageBar().ItemsSource(items);
	}

	void MainSettingsPage::SettingsBar_ItemClicked(BreadcrumbBar const& /*sender*/, BreadcrumbBarItemClickedEventArgs const& args)
	{
		// Trim items after clicked index
		auto itemsObj = MainSettingsPageBar().ItemsSource();
		auto vec = itemsObj.try_as<IObservableVector<IInspectable>>();
		if (!vec)
			return;

		int32_t count = static_cast<int32_t>(vec.Size());
		for (int32_t i = count - 1; i >= args.Index() + 1; --i)
		{
			vec.RemoveAtEnd();
		}

		// Navigate back to appropriate page based on breadcrumb depth
		if (args.Index() == 0)
		{
			// Navigate back to General Settings (root)
			auto transitionInfo = SlideNavigationTransitionInfo{};
			transitionInfo.Effect(SlideNavigationTransitionEffect::FromLeft);
			SettingsNavView().SelectedItem(GeneralNavItem());
		}
	}

	void MainSettingsPage::SettingsNavView_SelectionChanged(NavigationView const& /*sender*/, NavigationViewSelectionChangedEventArgs const& args)
	{
		if (auto selectedItem = args.SelectedItem().try_as<NavigationViewItem>())
		{
			auto tag = unbox_value_or<hstring>(selectedItem.Tag(), L"");

			// Create slide transition
			// TODO: Imporve effect based on nav tag item index
			auto transitionInfo = SlideNavigationTransitionInfo{};
			transitionInfo.Effect(SlideNavigationTransitionEffect::FromBottom);

			// Update breadcrumb
			auto itemsObj = MainSettingsPageBar().ItemsSource();
			if (auto breadcrumbItems = itemsObj.try_as<IObservableVector<IInspectable>>())
			{
				// Keep only root "Settings" item
				while (breadcrumbItems.Size() > 1)
				{
					breadcrumbItems.RemoveAtEnd();
				}

				// Add new category if not general
				if (tag != L"general")
				{
					auto categoryFolder = winrt::make<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation::Folder>();
					categoryFolder.Name(unbox_value_or<hstring>(selectedItem.Content(), L""));
					breadcrumbItems.Append(categoryFolder);
				}
			}

			// Navigate based on tag
			NavigateByTag(tag, transitionInfo);
		}
	}

	void MainSettingsPage::NavigateByTag(hstring const& tag, SlideNavigationTransitionInfo const& transitionInfo)
	{
		if (tag == L"general")
		{
			SettingsFrame().Navigate(
				xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::SettingsPage>(),
				nullptr,
				transitionInfo);
		}
		else if (tag == L"about")
		{
			SettingsFrame().Navigate(
				xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::AboutPage>(),
				nullptr,
				transitionInfo);
		}
		else if (tag == L"network" || tag == L"tracker")
		{
			SettingsFrame().Navigate(
				xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NetworkSettingsPage>(),
				nullptr,
				transitionInfo);
		}
		else if (tag == L"bittorrent" || tag == L"advanced")
		{
			SettingsFrame().Navigate(
				xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::BittorrentSettingsPage>(),
				nullptr,
				transitionInfo);
		}
		else if (tag == L"ipfilter")
		{
			SettingsFrame().Navigate(
				xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::IPFilterSettingsPage>(),
				nullptr,
				transitionInfo);
		}
		else if (tag == L"appearance")
		{
			SettingsFrame().Navigate(
				xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::ThemesSettingsPage>(),
				nullptr,
				transitionInfo);
		}
		else if (tag == L"downloadflash")
		{
			SettingsFrame().Navigate(
				xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::DownloadSettingsPage>(),
				nullptr,
				transitionInfo);
		}
		else
		{
			// Default: navigate to general settings as placeholder
			SettingsFrame().Navigate(
				xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::SettingsPage>(),
				nullptr,
				transitionInfo);
		}
	}
}
