#pragma once

#include "UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct TorrentCheckGeneralPage : TorrentCheckGeneralPageT<TorrentCheckGeneralPage>
    {
        TorrentCheckGeneralPage();
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct TorrentCheckGeneralPage : TorrentCheckGeneralPageT<TorrentCheckGeneralPage, implementation::TorrentCheckGeneralPage>
    {
    };
}
