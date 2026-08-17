#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/MainSettingsPage.g.h"

import winrt.Microsoft.UI.Xaml.Media.Animation;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	struct MainSettingsPage : MainSettingsPageT<MainSettingsPage>
	{
	private:
		static MainSettingsPage* s_current;

		// Navigate to page by tag
		void NavigateByTag(winrt::hstring const& tag,
						   winrt::Microsoft::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo const& transitionInfo);

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_settingsBarItems = winrt::single_threaded_observable_vector(std::vector<winrt::hstring>{L"Settings"});
	public:
		MainSettingsPage();
		~MainSettingsPage();
		static MainSettingsPage* Current();

		void MainSettingsPage_Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void MainSettingsPage_PointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
		// Breadcrumb handler
		void SettingsBar_ItemClicked(winrt::Microsoft::UI::Xaml::Controls::BreadcrumbBar const&, winrt::Microsoft::UI::Xaml::Controls::BreadcrumbBarItemClickedEventArgs const& args);

		// Navigation handler
		void SettingsNavView_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender, winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);
		void SettingsSearchBox_TextChanged(
			winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& sender,
			winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& args);
		void SettingsSearchBox_QuerySubmitted(
			winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& sender,
			winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs const& args);

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> SettingsBarItems();

	private:
		std::vector<winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem>
			SearchableItems();
		winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem
			FindSearchResult(winrt::hstring const& text);
		static std::wstring NormalizeSearchText(winrt::hstring const& value);
		static bool MatchesSearch(
			winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem const& item,
			std::wstring const& query);
		static winrt::hstring TagsForRoute(winrt::hstring const& route);
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct MainSettingsPage : MainSettingsPageT<MainSettingsPage, implementation::MainSettingsPage>
	{
	};
}
