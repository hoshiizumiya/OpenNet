#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/DownloadSettingsPage.g.h"
#include "Core/TorrentSettings.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    struct DownloadSettingsPage : DownloadSettingsPageT<DownloadSettingsPage>
    {
        DownloadSettingsPage();

        // Unified change handler
        void OnSettingChanged(winrt::Windows::Foundation::IInspectable const &sender,
                              winrt::Windows::Foundation::IInspectable const &args);

        // Folder browse handlers
        winrt::fire_and_forget BrowseSavePathButton_Click(
            winrt::Windows::Foundation::IInspectable const &sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const &args);
        winrt::fire_and_forget BrowseMovePathButton_Click(
            winrt::Windows::Foundation::IInspectable const &sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const &args);

    private:
        void LoadSettings();
        void PopulateFromSettings(::OpenNet::Core::TorrentSettings const &s);
        void SaveSettings();
        winrt::Windows::Foundation::IAsyncAction PickFolder(winrt::Microsoft::UI::Xaml::Controls::TextBox target);

        bool m_loading{false};
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
    struct DownloadSettingsPage : DownloadSettingsPageT<DownloadSettingsPage, implementation::DownloadSettingsPage>
    {
    };
}
