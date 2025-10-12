#pragma once
#include "pch.h"
#include "Pages/SettingsPages/Folder.g.h"

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
    struct Folder : FolderT<Folder>
    {
        Folder();

        winrt::hstring Name();
        void Name(winrt::hstring const& value);

    private:
        winrt::hstring m_name;
    };
}

namespace winrt::OpenNet::Pages::SettingsPages::factory_implementation
{
    struct Folder : FolderT<Folder, implementation::Folder>
    {
    };
}
