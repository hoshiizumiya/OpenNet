#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/WebUISettingsPage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	struct WebUISettingsPage : WebUISettingsPageT<WebUISettingsPage>
	{
		WebUISettingsPage();

		void OnSaveClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnGenerateApiKeyClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnCopyApiKeyClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnClearApiKeyClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnFollowApplicationLanguageToggled(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::fire_and_forget OnOpenWebUIClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		void LoadSettings();
		void UpdateStatus(
			winrt::hstring const& message = {},
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity =
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
		winrt::hstring WebUIUrl();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct WebUISettingsPage : WebUISettingsPageT<WebUISettingsPage, implementation::WebUISettingsPage>
	{
	};
}
