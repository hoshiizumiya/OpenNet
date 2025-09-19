#pragma once

#include "Pages/FilesPage.g.h"

namespace winrt::OpenNet::Pages::implementation
{
    struct FilesPage : FilesPageT<FilesPage>
    {
        FilesPage();
    };
}

namespace winrt::OpenNet::Pages::factory_implementation
{
    struct FilesPage : FilesPageT<FilesPage, implementation::FilesPage>
    {
    };
}
