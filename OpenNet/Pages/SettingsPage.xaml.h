#pragma once

#include "Pages/SettingsPage.g.h"

namespace winrt::OpenNet::Pages::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage();
    };
}

namespace winrt::OpenNet::Pages::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}
