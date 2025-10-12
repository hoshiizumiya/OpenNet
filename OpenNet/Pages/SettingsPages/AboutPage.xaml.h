#pragma once
#include "Pages/SettingsPages/AboutPage.g.h"
#include "mvvm_framework/view_sync_data_context.h"
#include "ViewModels/MainViewModel.h"

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
    struct AboutPage : AboutPageT<AboutPage>
    {
        AboutPage();

    };
}

namespace winrt::OpenNet::Pages::SettingsPages::factory_implementation
{
    struct AboutPage : AboutPageT<AboutPage, implementation::AboutPage>
    {
    };
}
