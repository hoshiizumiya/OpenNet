#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/MainSettingsPage.g.h"
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>

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

		// Breadcrumb handler
		void SettingsBar_ItemClicked(Microsoft::UI::Xaml::Controls::BreadcrumbBar const&, Microsoft::UI::Xaml::Controls::BreadcrumbBarItemClickedEventArgs const& args);

		// Navigation handler
		void SettingsNavView_SelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const& sender, Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);

		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> SettingsBarItems();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct MainSettingsPage : MainSettingsPageT<MainSettingsPage, implementation::MainSettingsPage>
	{
	};
}
