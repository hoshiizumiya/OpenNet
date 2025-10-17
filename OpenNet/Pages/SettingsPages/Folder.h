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

        winrt::event_token PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

    private:
        winrt::hstring m_name;
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::OpenNet::Pages::SettingsPages::factory_implementation
{
    struct Folder : FolderT<Folder, implementation::Folder>
    {
    };
}
