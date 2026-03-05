#pragma once

#include "UI/Xaml/View/Pages/FilesPage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct FilesPage : FilesPageT<FilesPage>
    {
        FilesPage();
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct FilesPage : FilesPageT<FilesPage, implementation::FilesPage>
    {
    };
}
