#pragma once

#include "Pages/SettingsPages/MainSettingsPage.g.h"

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
	struct MainSettingsPage : MainSettingsPageT<MainSettingsPage>
	{
	private:
		static MainSettingsPage* s_current;

	public:
		MainSettingsPage();
		static MainSettingsPage* Current();

		// Breadcrumb handler referenced in cpp
		void SettingsBar_ItemClicked(Microsoft::UI::Xaml::Controls::BreadcrumbBar const&, Microsoft::UI::Xaml::Controls::BreadcrumbBarItemClickedEventArgs const& args);

		// Expose a helper so other pages can update the SettingsBar without accessing implementation internals
		// void UpdateSettingsBarItems(winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> const& items);
		void UpdateSettingsBarItems(winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::Pages::SettingsPages::Folder> const& items);
	};
}

namespace winrt::OpenNet::Pages::SettingsPages::factory_implementation
{
	struct MainSettingsPage : MainSettingsPageT<MainSettingsPage, implementation::MainSettingsPage>
	{
	};
}
