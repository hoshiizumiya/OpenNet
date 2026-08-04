#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.h"
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
		void ApplyBackgroundToSecondaryWindowsSwitch_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		winrt::Windows::Foundation::IAsyncAction SetImageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void ClearImageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void ImageModeComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void ImageStretchComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void ImageOpacitySlider_ValueChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);
		void BackgroundRotationMinutesBox_ValueChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction SetVideoButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void ClearVideoButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void VideoModeComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void VideoMutedSwitch_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void VideoLoopingSwitch_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void VideoStretchComboBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
		void VideoOpacitySlider_ValueChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);
		void NavigateToThemeSettingBackdropCustomizePageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void NavigateToThemeSettingFontSettingPageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

	private:
		void LoadBackdropSettings();
		void ApplyBackdropFromSelection();
		void UpdateBackdropCardState();
		void UpdateMediaCardState();
		void PersistMediaOptions();
		void ApplyImageBackgroundFromSettings();
		// XAML property assignment can raise SelectionChanged/ValueChanged before
		// every named element has been connected. Keep handlers disabled until the
		// first Loaded pass has populated all settings controls.
		bool m_isInitializing{ true };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct ThemesSettingsPage : ThemesSettingsPageT<ThemesSettingsPage, implementation::ThemesSettingsPage>
	{
	};
}
