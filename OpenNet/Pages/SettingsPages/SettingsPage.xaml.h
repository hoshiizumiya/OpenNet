#pragma once

#include "Pages/SettingsPages/SettingsPage.g.h"

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
	struct SettingsPage : SettingsPageT<SettingsPage>
	{
	public:
		SettingsPage();
		static SettingsPage* Current();

		// Event handlers referenced from XAML
		void Aboutp_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void SPSettings_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		void SoftLanguageCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void SoftBackgroundCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void StartPageCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void SetDesktopBackground();

		void AnnotatedScrollBarPage_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void AnnotatedScrollBarPage_Unloaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void AnnotatedScrollBar_DetailLabelRequested(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::AnnotatedScrollBarDetailLabelRequestedEventArgs const& e);

		// Click handler for the InfoBar action button to restart
		void RestartToApplyLanguage_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);


	private:
		//static winrt::weak_ref<SettingsPage> s_current;
		static SettingsPage* s_current;


		// Track the language override at page load, and any pending selection
		winrt::hstring m_initialLanguageOverride{};
		winrt::hstring m_pendingLanguageOverride{};
		bool m_hasPendingLangChange{ false };
	};
}

namespace winrt::OpenNet::Pages::SettingsPages::factory_implementation
{
	struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
	{
	};
}
