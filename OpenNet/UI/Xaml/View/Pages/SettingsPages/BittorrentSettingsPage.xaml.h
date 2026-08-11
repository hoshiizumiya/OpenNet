#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.g.h"

import OpenNet.Core.TorrentSettings;
import winrt.Microsoft.UI.Xaml;

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

        // XAML controls can raise change events while InitializeComponent is still
        // connecting later named elements. Start suppressed and only enable saving
        // after the initial settings snapshot has populated every control.
        bool m_loading{true};
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
    struct BittorrentSettingsPage : BittorrentSettingsPageT<BittorrentSettingsPage, implementation::BittorrentSettingsPage>
    {
    };
}
