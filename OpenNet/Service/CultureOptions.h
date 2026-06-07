#pragma once

#include "Service\CultureOptions.g.h"

namespace winrt::OpenNet::Service::implementation
{
    struct CultureOptions : CultureOptionsT<CultureOptions>
    {
        CultureOptions() = default;

    };
}

namespace winrt::OpenNet::Service::factory_implementation
{
    struct CultureOptions : CultureOptionsT<CultureOptions, implementation::CultureOptions>
    {
    };
}
