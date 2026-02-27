#pragma once

#include "Pages/SettingsPages/MainSettingsPage.g.h"
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
	struct MainSettingsPage : MainSettingsPageT<MainSettingsPage>
	{
	private:
		static MainSettingsPage* s_current;

		// Navigate to page by tag
		void NavigateByTag(winrt::hstring const& tag, 
			winrt::Microsoft::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo const& transitionInfo);

	public:
		MainSettingsPage();
		static MainSettingsPage* Current();

		// Breadcrumb handler
		void SettingsBar_ItemClicked(Microsoft::UI::Xaml::Controls::BreadcrumbBar const&, Microsoft::UI::Xaml::Controls::BreadcrumbBarItemClickedEventArgs const& args);

		// Navigation handler
		void SettingsNavView_SelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const& sender, Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);

		// Helper to update breadcrumb items
		void UpdateSettingsBarItems(winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::Pages::SettingsPages::Folder> const& items);
	};
}

namespace winrt::OpenNet::Pages::SettingsPages::factory_implementation
{
	struct MainSettingsPage : MainSettingsPageT<MainSettingsPage, implementation::MainSettingsPage>
	{
	};
}
