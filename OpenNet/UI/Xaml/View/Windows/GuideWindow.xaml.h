#pragma once

#include "UI/Xaml/View/Windows/GuideWindow.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
    struct GuideWindow : GuideWindowT<GuideWindow>
    {
        GuideWindow();

    };
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
    struct GuideWindow : GuideWindowT<GuideWindow, implementation::GuideWindow>
    {
    };
}
