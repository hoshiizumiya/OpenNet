#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/FontCustomizePage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    struct FontCustomizePage : FontCustomizePageT<FontCustomizePage>
    {
        FontCustomizePage();
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
    struct FontCustomizePage : FontCustomizePageT<FontCustomizePage, implementation::FontCustomizePage>
    {
    };
}
