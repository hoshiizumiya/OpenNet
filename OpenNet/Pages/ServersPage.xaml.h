#pragma once

#include "Pages/ServersPage.g.h"

namespace winrt::OpenNet::Pages::implementation
{
    struct ServersPage : ServersPageT<ServersPage>
    {
        ServersPage();
    };
}

namespace winrt::OpenNet::Pages::factory_implementation
{
    struct ServersPage : ServersPageT<ServersPage, implementation::ServersPage>
    {
    };
}
