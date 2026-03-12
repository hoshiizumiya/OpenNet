#include "pch.h"
#include "DownloadSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/DownloadSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/DownloadSettingsPage.g.cpp"
#endif

#include "Core/TorrentSettings.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Windows::Storage::Pickers;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    DownloadSettingsPage::DownloadSettingsPage()
    {
        InitializeComponent();

        Loaded([this](IInspectable const &, RoutedEventArgs const &)
               { LoadSettings(); });

        // Save settings when page unloads so TextBox LostFocus changes aren't lost
        Unloaded([this](IInspectable const &, RoutedEventArgs const &)
        {
            if (!m_loading)
            {
                SaveSettings();
            }
        });
    }

    void DownloadSettingsPage::LoadSettings()
    {
        m_loading = true;
        auto &mgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
        mgr.Load();
        PopulateFromSettings(mgr.Get());
        m_loading = false;
    }

    void DownloadSettingsPage::PopulateFromSettings(::OpenNet::Core::TorrentSettings const &s)
    {
        DefaultSavePathTextBox().Text(s.defaultSavePath);
        PreallocateStorageToggle().IsOn(s.preallocateStorage);
        AutoStartDownloadsToggle().IsOn(s.autoStartDownloads);
        MoveCompletedToggle().IsOn(s.moveCompletedEnabled);
        MoveCompletedPathTextBox().Text(s.moveCompletedPath);
    }

    void DownloadSettingsPage::OnSettingChanged(IInspectable const &, IInspectable const &)
    {
        if (m_loading)
            return;
        SaveSettings();
    }

    void DownloadSettingsPage::SaveSettings()
    {
        auto &mgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
        auto s = mgr.Get();

        s.defaultSavePath = DefaultSavePathTextBox().Text();
        s.preallocateStorage = PreallocateStorageToggle().IsOn();
        s.autoStartDownloads = AutoStartDownloadsToggle().IsOn();
        s.moveCompletedEnabled = MoveCompletedToggle().IsOn();
        s.moveCompletedPath = MoveCompletedPathTextBox().Text();
        mgr.Set(s);
    }

    winrt::fire_and_forget DownloadSettingsPage::BrowseSavePathButton_Click(IInspectable const &, RoutedEventArgs const &)
    {
        co_await PickFolder(DefaultSavePathTextBox());
    }

    winrt::fire_and_forget DownloadSettingsPage::BrowseMovePathButton_Click(IInspectable const &, RoutedEventArgs const &)
    {
        co_await PickFolder(MoveCompletedPathTextBox());
    }

    IAsyncAction DownloadSettingsPage::PickFolder(TextBox target)
    {
        auto folderPicker = FolderPicker(XamlRoot().ContentIslandEnvironment().AppWindowId());
        folderPicker.SuggestedStartLocation(PickerLocationId::Downloads);

        auto folder = co_await folderPicker.PickSingleFolderAsync();
        if (folder)
        {
            target.Text(folder.Path());
            SaveSettings();
        }
    }
}
