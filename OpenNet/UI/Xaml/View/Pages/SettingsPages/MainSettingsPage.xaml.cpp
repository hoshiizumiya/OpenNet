#include "XamlWorkaround.h"
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
#include "UI/Xaml/View/Pages/SettingsPages/ClientFilterSettingsPage.xaml.h"
#include "UI/Xaml/View/Pages/SettingsPages/DownloadSettingsPage.xaml.h"
#include "UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.xaml.h"
#include "UI/Xaml/View/Pages/SettingsPages/WebUISettingsPage.xaml.h"
#include "SettingsPageTag.h"

import OpenNet.Core.Utils.Message;
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
		if (!SettingsFrame().SourcePageType().Name.empty()) SyncBreadcrumb();
		else SettingsNavView().SelectedItem(GeneralNavItem());
	}

	void MainSettingsPage::MainSettingsPage_PointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
	{
		auto props = e.GetCurrentPoint(nullptr).Properties();

		if (props.IsXButton1Pressed())
		{
			if (SettingsFrame().CanGoBack())
			{
				SettingsFrame().GoBack();
				e.Handled(true);
			}
		}
		else if (props.IsXButton2Pressed())
		{
			if (SettingsFrame().CanGoForward())
			{
				SettingsFrame().GoForward();
				e.Handled(true);
			}
		}
	}

	void MainSettingsPage::SettingsBar_ItemClicked(BreadcrumbBar const& /*sender*/, BreadcrumbBarItemClickedEventArgs const& args)
	{
		if (args.Index() >= static_cast<std::int32_t>(m_settingsBarItems.Size()) - 1) return;
		auto target = GeneralNavItem();
		if (args.Index() == 1)
		{
			auto const source = SettingsFrame().SourcePageType();
			auto const detail = source.Name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::ThemeSettingBackdropCustomizePage>().Name
				|| source.Name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::FontCustomizePage>().Name;
			target = detail ? AppearanceNavItem() :
			(m_settingsBarItems.Size() == 3 ? BitTorrentNavItem() : m_selectedItem);
		}
		else if (args.Index() == 2 && m_settingsBarItems.Size() == 3)
		{
			target = m_selectedItem;
		}
		if (!target) return;
		SettingsNavView().SelectedItem(target);
		auto const pageType = target.Tag().as<winrt::Windows::UI::Xaml::Interop::TypeName>();
		if (SettingsFrame().SourcePageType().Name != pageType.Name)
		{
			auto transition = SlideNavigationTransitionInfo{};
			transition.Effect(SlideNavigationTransitionEffect::FromLeft);
			SettingsFrame().Navigate(pageType, nullptr, transition);
		}
	}

	void MainSettingsPage::SettingsNavView_SelectionChanged(NavigationView const& /*sender*/, NavigationViewSelectionChangedEventArgs const& args)
	{
		if (m_syncingNavigation) return;
		if (auto selectedItem = args.SelectedItem().try_as<NavigationViewItem>())
		{
			m_selectedItem = selectedItem;
			auto const pageType = selectedItem.Tag().as<winrt::Windows::UI::Xaml::Interop::TypeName>();

			// Create slide transition
			// TODO: Imporve effect based on nav tag item index
			auto transitionInfo = SlideNavigationTransitionInfo{};
			transitionInfo.Effect(SlideNavigationTransitionEffect::FromBottom);

			if (SettingsFrame().SourcePageType().Name != pageType.Name)
			{
				SettingsFrame().Navigate(pageType, nullptr, transitionInfo);
			}
			else SyncBreadcrumb();
		}
	}

	void MainSettingsPage::SettingsFrame_Navigated(IInspectable const&, winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		SyncBreadcrumb();
	}

	void MainSettingsPage::SyncBreadcrumb()
	{
		auto const source = SettingsFrame().SourcePageType();
		if (source.Name.empty()) return;
		auto const backdrop = source.Name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::ThemeSettingBackdropCustomizePage>().Name;
		auto const font = source.Name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::FontCustomizePage>().Name;
		auto item = m_selectedItem;
		if (backdrop || font) item = AppearanceNavItem();
		else if (!item || !item.Tag() || item.Tag().as<winrt::Windows::UI::Xaml::Interop::TypeName>().Name != source.Name)
		{
			for (auto const& candidate : SearchableItems())
			{
				if (candidate.Tag().as<winrt::Windows::UI::Xaml::Interop::TypeName>().Name == source.Name)
				{
					item = candidate;
					break;
				}
			}
		}
		if (!item) return;
		m_selectedItem = item;
		m_syncingNavigation = true;
		SettingsNavView().SelectedItem(item);
		m_syncingNavigation = false;
		auto const root = ResourceGetString(L"SettingsBreadcrumbRoot");
		m_settingsBarItems.Clear();
		m_settingsBarItems.Append(root.empty() ? L"Settings" : root);
		if (item != GeneralNavItem())
		{
			if (item == NetworkNavItem() || item == TrackerNavItem() || item == IPFilterNavItem() || item == ClientFilterNavItem())
				m_settingsBarItems.Append(winrt::unbox_value_or<hstring>(BitTorrentNavItem().Content(), L"BitTorrent"));
			m_settingsBarItems.Append(winrt::unbox_value_or<hstring>(item.Content(), L""));
		}
		if (backdrop) m_settingsBarItems.Append(ResourceGetString(L"SoftBackgroundColors/Header"));
		else if (font) m_settingsBarItems.Append(ResourceGetString(L"ViewThemesSettingsPageFontSetting/Header"));
	}

	std::vector<NavigationViewItem> MainSettingsPage::SearchableItems()
	{
		return {
			GeneralNavItem(), BitTorrentNavItem(), NetworkNavItem(), TrackerNavItem(),
			IPFilterNavItem(), ClientFilterNavItem(), DownloadBehaviorNavItem(),
			WebUINavItem(), AppearanceNavItem(), AboutNavItem()
		};
	}

	NavigationViewItem MainSettingsPage::FindSearchResult(hstring const& text)
	{
		auto query = NormalizeSearchText(text);
		if (query.empty()) return nullptr;

		for (auto const& item : SearchableItems())
		{
			if (MatchesSearch(item, query))
				return item;
		}
		return nullptr;
	}

	void MainSettingsPage::SettingsSearchBox_TextChanged(
		AutoSuggestBox const& sender,
		AutoSuggestBoxTextChangedEventArgs const& args)
	{
		if (args.Reason() != AutoSuggestionBoxTextChangeReason::UserInput)
			return;

		auto suggestions = single_threaded_vector<IInspectable>();
		auto query = NormalizeSearchText(sender.Text());
		if (!query.empty())
		{
			for (auto const& item : SearchableItems())
			{
				auto const label = unbox_value_or<hstring>(item.Content(), L"");
				if (MatchesSearch(item, query))
					suggestions.Append(box_value(label));
			}
		}
		sender.ItemsSource(suggestions);
	}

	std::wstring MainSettingsPage::NormalizeSearchText(hstring const& value)
	{
		std::wstring normalized{ value.c_str() };
		std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::towlower);
		return normalized;
	}

	bool MainSettingsPage::MatchesSearch(NavigationViewItem const& item, std::wstring const& query)
	{
		auto const label = NormalizeSearchText(unbox_value_or<hstring>(item.Content(), L""));
		auto const pageType = item.Tag().as<winrt::Windows::UI::Xaml::Interop::TypeName>();
		auto const tags = NormalizeSearchText(TagsForPageType(pageType));
		auto const searchableText = label + L" " + tags;
		std::wistringstream tokens{ query };
		std::wstring token;
		bool matchedAnyToken = false;
		while (tokens >> token)
		{
			matchedAnyToken = true;
			if (searchableText.find(token) == std::wstring::npos)
			{
				return false;
			}
		}
		return matchedAnyToken;
	}

	hstring MainSettingsPage::TagsForPageType(winrt::Windows::UI::Xaml::Interop::TypeName const& pageType)
	{
		hstring result;
		for (auto const& registration : SettingsPageTag::Registry())
		{
			if (pageType.Name == registration.TypeName)
			{
				if (!result.empty()) result = result + L" ";
				result = result + ResourceGetString(registration.TagsResourceKey.data());
			}
		}
		return result;
	}

	void MainSettingsPage::SettingsSearchBox_QuerySubmitted(
		AutoSuggestBox const& sender,
		AutoSuggestBoxQuerySubmittedEventArgs const& args)
	{
		auto query = args.ChosenSuggestion()
			? unbox_value_or<hstring>(args.ChosenSuggestion(), sender.Text())
			: sender.Text();
		if (auto item = FindSearchResult(query))
		{
			SettingsNavView().SelectedItem(item);
			sender.Text(unbox_value_or<hstring>(item.Content(), query));
		}
	}

	winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> MainSettingsPage::SettingsBarItems()
	{
		return m_settingsBarItems;
	}

}
