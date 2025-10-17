#pragma once

#include "Pages/SettingsPages/ThemesSettingsPage.g.h"
#include "mvvm_framework/view_sync_data_context.h"
#include "ViewModels/MainViewModel.h"

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
    struct ThemesSettingsPage : ThemesSettingsPageT<ThemesSettingsPage>
    {
        ThemesSettingsPage();

    };
}

namespace winrt::OpenNet::Pages::SettingsPages::factory_implementation
{
    struct ThemesSettingsPage : ThemesSettingsPageT<ThemesSettingsPage, implementation::ThemesSettingsPage>
    {
    };
}
