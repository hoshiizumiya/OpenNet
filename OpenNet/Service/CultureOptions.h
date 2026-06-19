#pragma once

#include "Service\CultureOptions.g.h"

namespace winrt::OpenNet::Service::implementation
{
    struct CultureOptions : CultureOptionsT<CultureOptions>
    {
        CultureOptions() = default;

		hstring CultureInfo();
		void CultureInfo(hstring const& value);
    };
}

namespace winrt::OpenNet::Service::factory_implementation
{
    struct CultureOptions : CultureOptionsT<CultureOptions, implementation::CultureOptions>
    {
    };
}
