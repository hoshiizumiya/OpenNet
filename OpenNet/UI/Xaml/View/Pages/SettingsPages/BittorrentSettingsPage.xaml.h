#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.g.h"
#include "Core/TorrentSettings.h"
#include <winrt/Microsoft.UI.Xaml.h>

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    struct BittorrentSettingsPage : BittorrentSettingsPageT<BittorrentSettingsPage>
    {
        BittorrentSettingsPage();

        // Unified change handler – auto-saves on every control change
        void OnSettingChanged(winrt::Windows::Foundation::IInspectable const &sender,
                              winrt::Windows::Foundation::IInspectable const &args);

    private:
        winrt::fire_and_forget LoadSettings();
        void PopulateFromSettings(::OpenNet::Core::TorrentSettings const &s);
        ::OpenNet::Core::TorrentSettings CollectFromUI();
        void SaveAndApply();

        bool m_loading{false}; // suppress change events during initial load
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
    struct BittorrentSettingsPage : BittorrentSettingsPageT<BittorrentSettingsPage, implementation::BittorrentSettingsPage>
    {
    };
}
