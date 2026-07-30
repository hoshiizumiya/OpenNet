#pragma once

import winrt.OpenNet.UI.Xaml.Control;
#include "UI/Xaml/View/Pages/SettingsPages/SettingsPage.g.h"
#include "Service/Update/UpdateService.h"

import winrt.Microsoft.UI.Dispatching;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	struct SettingsPage : SettingsPageT<SettingsPage>
	{
	public:
		SettingsPage();
		~SettingsPage();
		static SettingsPage* Current();

		// Event handlers referenced from XAML
		void AppUpdateCheckButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void UpdateStatusControl_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void AutoCheckUpdateCheckbox_Checked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void AutoCheckUpdateCheckbox_Unchecked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		void SoftLanguageCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void SoftBackgroundCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void StartPageCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void themeMode_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void GuiRefreshIntervalBox_ValueChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction SetDesktopBackground();

		// void AnnotatedScrollBarPage_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		// void AnnotatedScrollBarPage_Unloaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		// void AnnotatedScrollBar_DetailLabelRequested(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::AnnotatedScrollBarDetailLabelRequestedEventArgs const& e);

		winrt::Windows::Foundation::IAsyncAction OnSettingsPageLoadedAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		// Click handler for the InfoBar action button to restart
		void RestartToApplyLanguage_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		::winrt::Windows::Foundation::IAsyncAction CheckForUpdatesAsync(bool showSuccess);
		::winrt::Windows::Foundation::IAsyncAction LaunchAvailableUpdateAsync();

	private:
		//static winrt::weak_ref<SettingsPage> s_current;
		static SettingsPage* s_current;

		winrt::Windows::Foundation::IAsyncAction m_loadAction;
		winrt::Windows::Foundation::IAsyncAction m_updateCheckAction;
		winrt::Windows::Foundation::IAsyncAction m_updateLaunchAction;
		winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr }; // 在构造里捕获
		::OpenNet::Service::Update::UpdateService m_updateService;
		std::shared_ptr<::OpenNet::Service::Update::CheckUpdateResult> m_updateResult;

		// Track the language override at page load, and any pending selection
		winrt::hstring m_initialLanguageOverride{};
		winrt::hstring m_pendingLanguageOverride{};
		bool m_hasPendingLangChange{ false };
		bool m_isStartPageLoading{ false };
		bool m_isRefreshIntervalLoading{ false };
		bool m_isInitializingUpdateSettings{ false };
		bool m_isCheckingForUpdate{ false };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
	{
	};
}
