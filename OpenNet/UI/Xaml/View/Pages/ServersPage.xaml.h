#pragma once

#include "UI/Xaml/View/Pages/ServersPage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct ServersPage : ServersPageT<ServersPage>
    {
        ServersPage();
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct ServersPage : ServersPageT<ServersPage, implementation::ServersPage>
    {
    };
}
