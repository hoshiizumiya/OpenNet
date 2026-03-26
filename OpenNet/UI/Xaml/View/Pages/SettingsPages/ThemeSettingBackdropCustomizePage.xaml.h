#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/ThemeSettingBackdropCustomizePage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    struct ThemeSettingBackdropCustomizePage : ThemeSettingBackdropCustomizePageT<ThemeSettingBackdropCustomizePage>
    {
        ThemeSettingBackdropCustomizePage();

        void Page_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void BackdropValueChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void BackdropValueChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::ColorChangedEventArgs const& e);
        void BackdropValueChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);

    private:
        void LoadFromSettings();
        void SaveToSettings();
        void SyncFromCurrentBackdrop();
        void ApplyToCurrentBackdrop();

        bool m_isUpdatingUI{ false };
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
    struct ThemeSettingBackdropCustomizePage : ThemeSettingBackdropCustomizePageT<ThemeSettingBackdropCustomizePage, implementation::ThemeSettingBackdropCustomizePage>
    {
    };
}
