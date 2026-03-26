#include "pch.h"
#include "MainSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/MainSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/MainSettingsPage.g.cpp"
#endif

#include "SettingsPage.xaml.h"
#include "AboutPage.xaml.h"
#include "ThemesSettingsPage.xaml.h"
#include "ThemeSettingBackdropCustomizePage.xaml.h"
#include "FontCustomizePage.xaml.h"
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
		s_current = this;
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

	void MainSettingsPage::MainSettingsPage_Loaded(IInspectable const&, RoutedEventArgs const&)
	{
		SettingsNavView().SelectedItem(GeneralNavItem());
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
			SettingsNavView().SelectedItem(GeneralNavItem());
		}
		else if (args.Index() == 1)
		{
			SettingsNavView().SelectedItem(AppearanceNavItem());
		}
		else if (args.Index() == 2 && m_settingsBarItems.Size() > 2)
		{
			auto const pageTitle = m_settingsBarItems.GetAt(2);
			auto transitionInfo = SlideNavigationTransitionInfo{};
			transitionInfo.Effect(SlideNavigationTransitionEffect::FromLeft);
			SettingsNavView().SelectedItem(AppearanceNavItem());

			if (pageTitle == L"Colors Style")
			{
				SettingsFrame().Navigate(
					xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::ThemeSettingBackdropCustomizePage>(),
					nullptr,
					transitionInfo);
				return;
			}

			if (pageTitle == L"Font Setting")
			{
				SettingsFrame().Navigate(
					xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::FontCustomizePage>(),
					nullptr,
					transitionInfo);
			}
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

			// Keep only root "Settings" item
			while (m_settingsBarItems.Size() > 1)
			{
				m_settingsBarItems.RemoveAtEnd();
			}

			// Add new category if not general
			if (tag != L"general")
			{
				m_settingsBarItems.Append(winrt::unbox_value_or(selectedItem.Content(), L""));
			}

			// Navigate based on tag
			NavigateByTag(tag, transitionInfo);
		}
	}

	winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> MainSettingsPage::SettingsBarItems()
	{
		return m_settingsBarItems;
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
		else if (tag == L"download")
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
