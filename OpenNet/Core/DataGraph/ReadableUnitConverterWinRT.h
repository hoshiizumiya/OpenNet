#pragma once

#include "Core/DataGraph/ReadableUnitConverterWinRT.g.h"

namespace winrt::OpenNet::Core::DataGraph::implementation
{
    struct ReadableUnitConverterWinRT : ReadableUnitConverterWinRTT<ReadableUnitConverterWinRT>
    {
        static winrt::hstring ConvertSize(uint64_t size);
        static winrt::hstring ConvertSpeed(double speed);
		static winrt::hstring ConvertPercent(double percentOfOne);
        static winrt::hstring ConvertDateTime(winrt::Windows::Foundation::DateTime dateTime);
    };
}

namespace winrt::OpenNet::Core::DataGraph::factory_implementation
{
    struct ReadableUnitConverterWinRT : ReadableUnitConverterWinRTT<ReadableUnitConverterWinRT, implementation::ReadableUnitConverterWinRT>
    {
    };
}
