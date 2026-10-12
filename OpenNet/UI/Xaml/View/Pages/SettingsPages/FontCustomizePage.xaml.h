#pragma once

#include "UI/Xaml/Control/Theme/Typography.h"
#include "UI/Xaml/View/Pages/SettingsPages/FontCustomizePage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    struct FontCustomizePage : FontCustomizePageT<FontCustomizePage>
    {
        FontCustomizePage();
        void Reset_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
    struct FontCustomizePage : FontCustomizePageT<FontCustomizePage, implementation::FontCustomizePage>
    {
    };
}
