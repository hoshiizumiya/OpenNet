#pragma once

#include "UI/Xaml/View/Windows/InfoOverlayWindow.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
    struct InfoOverlayWindow : InfoOverlayWindowT<InfoOverlayWindow>
    {
        InfoOverlayWindow();

    };
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
    struct InfoOverlayWindow : InfoOverlayWindowT<InfoOverlayWindow, implementation::InfoOverlayWindow>
    {
    };
}
