#pragma once

#include "UI/Xaml/Media/Backdrop/InputActiveDesktopAcrylicBackdrop.g.h"

namespace winrt::OpenNet::UI::Xaml::Media::Backdrop::implementation
{
    struct InputActiveDesktopAcrylicBackdrop : InputActiveDesktopAcrylicBackdropT<InputActiveDesktopAcrylicBackdrop>
    {
        InputActiveDesktopAcrylicBackdrop();
    };
}

namespace winrt::OpenNet::UI::Xaml::Media::Backdrop::factory_implementation
{
    struct InputActiveDesktopAcrylicBackdrop : InputActiveDesktopAcrylicBackdropT<InputActiveDesktopAcrylicBackdrop, implementation::InputActiveDesktopAcrylicBackdrop>
    {
    };
}
