#pragma once

#include "UI/Xaml/Behavior/InvokeCommandOnLoadedBehavior.g.h"

namespace winrt::OpenNet::UI::Xaml::Behavior::implementation
{
    struct InvokeCommandOnLoadedBehavior : InvokeCommandOnLoadedBehaviorT<InvokeCommandOnLoadedBehavior>
    {
        InvokeCommandOnLoadedBehavior() = default;


    };
}

namespace winrt::OpenNet::UI::Xaml::Behavior::factory_implementation
{
    struct InvokeCommandOnLoadedBehavior : InvokeCommandOnLoadedBehaviorT<InvokeCommandOnLoadedBehavior, implementation::InvokeCommandOnLoadedBehavior>
    {
    };
}
