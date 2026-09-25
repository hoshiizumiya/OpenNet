#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/WebUISettingsPage.g.h"
#include "ViewModels/WebUISettingsViewModel.h"
#include <winrt/Microsoft.UI.Dispatching.h>

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	struct WebUISettingsPage : WebUISettingsPageT<WebUISettingsPage>
	{
		WebUISettingsPage();

		winrt::OpenNet::ViewModels::WebUISettingsViewModel ViewModel()
		{
			if (!m_viewModel)
				m_viewModel = winrt::OpenNet::ViewModels::WebUISettingsViewModel();
			return m_viewModel;
		}

		void OnGenerateApiKeyClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnCopyApiKeyClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnClearApiKeyClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnMaterialToggleLoaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnMaterialToggleChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::fire_and_forget OnOpenWebUIClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
	private:
		void LoadSettings();
		void ApplySettings();
		void ScheduleApply();
		void SyncMaterialToggles();
		void UpdateFollowLanguageUi();
		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_applyTimer{ nullptr };
		bool m_initializing{ true };
		bool m_followApplicationLanguage{ true };
		bool m_bypassLocalAuth{};
		bool m_csrfProtection{ true };
		bool m_hostValidation{ true };
		bool m_secureCookie{};
		void UpdateStatus(
			winrt::hstring const& message = {},
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity =
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
		winrt::hstring WebUIUrl();
		winrt::OpenNet::ViewModels::WebUISettingsViewModel m_viewModel{ nullptr };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct WebUISettingsPage : WebUISettingsPageT<WebUISettingsPage, implementation::WebUISettingsPage>
	{
	};
}
