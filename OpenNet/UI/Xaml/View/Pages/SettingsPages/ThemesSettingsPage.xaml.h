#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.h"
#include "mvvm_framework/view_sync_data_context.h"
#include "ViewModels/MainViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	struct ThemesSettingsPage : ThemesSettingsPageT<ThemesSettingsPage>
	{
		ThemesSettingsPage();

		void BackgroundComboBox_SelectionChanged(IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void MicaTypeComboBox_SelectionChanged(IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void AcrylicTypeComboBox_SelectionChanged(IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void BackdropFallbackSwitch_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		winrt::Windows::Foundation::IAsyncAction SetImageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void ClearImageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void ImageStretchComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void ImageOpacitySlider_ValueChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);
		void NavigateToThemeSettingBackdropCustomizePageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void NavigateToThemeSettingFontSettingPageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

	private:
		void LoadBackdropSettings();
		void ApplyBackdropFromSelection();
		void UpdateBackdropCardState();
		void SyncSelectionWithCurrentBackdrop();
		void ApplyImageBackgroundFromSettings();
		bool m_isInitializing{ false };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct ThemesSettingsPage : ThemesSettingsPageT<ThemesSettingsPage, implementation::ThemesSettingsPage>
	{
	};
}
