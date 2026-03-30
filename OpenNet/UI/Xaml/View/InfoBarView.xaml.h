#pragma once

#include "UI/Xaml/View/InfoBarView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
    struct InfoBarView : InfoBarViewT<InfoBarView>
    {
        InfoBarView()=default;

    };
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
    struct InfoBarView : InfoBarViewT<InfoBarView, implementation::InfoBarView>
    {
    };
}
