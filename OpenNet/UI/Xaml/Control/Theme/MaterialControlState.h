#pragma once

#include "UI/Xaml/Control/Theme/MaterialControlState.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
    struct MaterialControlState : MaterialControlStateT<MaterialControlState>
    {
        MaterialControlState() = default;

        static winrt::Microsoft::UI::Xaml::DependencyProperty StyleIndexProperty();
        std::int32_t StyleIndex();
        void StyleIndex(std::int32_t value);
    };
}

namespace winrt::OpenNet::UI::Xaml::Control::Theme::factory_implementation
{
    struct MaterialControlState : MaterialControlStateT<MaterialControlState, implementation::MaterialControlState>
    {
    };
}
