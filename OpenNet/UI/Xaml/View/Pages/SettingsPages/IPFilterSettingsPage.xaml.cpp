#include "pch.h"
#include "IPFilterSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.g.cpp"
#endif

#include "Core/IPFilter/IPFilterManager.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>
#include <wil/cppwinrt_helpers.h>

#include <fstream>
#include <sstream>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Windows::Storage::Pickers;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    IPFilterSettingsPage::IPFilterSettingsPage()
    {
        InitializeComponent();

        Loaded([this](IInspectable const&, RoutedEventArgs const&)
        {
            LoadState();
        });
    }

    winrt::fire_and_forget IPFilterSettingsPage::LoadState()
    {
        auto strong = get_strong();
        auto dispatcher = DispatcherQueue();

        co_await winrt::resume_background();

        auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
        mgr.Initialize();
        bool enabled = mgr.IsEnabled();
        int count = mgr.GetRuleCount();

        co_await wil::resume_foreground(dispatcher);

        m_loading = true;
        EnableFilterToggle().IsOn(enabled);
        RuleCountText().Text(winrt::to_hstring(count));
        m_loading = false;
    }

    void IPFilterSettingsPage::RefreshRuleCount()
    {
        auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
        int count = mgr.GetRuleCount();
        RuleCountText().Text(winrt::to_hstring(count));
    }

    void IPFilterSettingsPage::ShowStatus(winrt::hstring const& message, InfoBarSeverity severity)
    {
        StatusInfoBar().Message(message);
        StatusInfoBar().Severity(severity);
        StatusInfoBar().IsOpen(true);
    }

    void IPFilterSettingsPage::OnEnableToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loading) return;

        auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
        mgr.SetEnabled(EnableFilterToggle().IsOn());
        mgr.ApplyToSession();
    }

    void IPFilterSettingsPage::OnAddRuleClick(IInspectable const&, RoutedEventArgs const&)
    {
        auto text = winrt::to_string(ManualIpTextBox().Text());
        if (text.empty()) return;

        auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
        int added = mgr.ImportFromText(text);

        if (added > 0)
        {
            mgr.ApplyToSession();
            ManualIpTextBox().Text(L"");
            RefreshRuleCount();
            ShowStatus(winrt::to_hstring(std::to_string(added) + " rule(s) added"),
                       InfoBarSeverity::Success);
        }
        else
        {
            ShowStatus(L"No valid IP addresses found in input",
                       InfoBarSeverity::Warning);
        }
    }

    winrt::fire_and_forget IPFilterSettingsPage::OnImportClick(IInspectable const&, RoutedEventArgs const&)
    {
        auto strong = get_strong();

        // Create file picker using the WinUI 3 AppWindow-based API
        auto picker = FileOpenPicker(this->XamlRoot().ContentIslandEnvironment().AppWindowId());
        picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
        picker.FileTypeFilter().Append(L".txt");
        picker.FileTypeFilter().Append(L".dat");
        picker.FileTypeFilter().Append(L".p2p");
        picker.FileTypeFilter().Append(L"*");

        auto file = co_await picker.PickSingleFileAsync();
        if (!file) co_return;

        auto dispatcher = DispatcherQueue();

        // Read file on background thread
        co_await winrt::resume_background();

        std::string content;
        try
        {
            auto path = winrt::to_string(file.Path());
            std::ifstream ifs(path, std::ios::binary);
            if (ifs)
            {
                std::ostringstream oss;
                oss << ifs.rdbuf();
                content = oss.str();
            }
        }
        catch (...) { /* fallthrough */ }

        int added = 0;
        if (!content.empty())
        {
            auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
            added = mgr.ImportFromText(content);
            if (added > 0)
                mgr.ApplyToSession();
        }

        co_await wil::resume_foreground(dispatcher);

        RefreshRuleCount();
        if (added > 0)
        {
            ShowStatus(winrt::to_hstring(std::to_string(added) + " rule(s) imported from file"),
                       InfoBarSeverity::Success);
        }
        else
        {
            ShowStatus(L"No valid IP addresses found in file",
                       InfoBarSeverity::Warning);
        }
    }

    winrt::fire_and_forget IPFilterSettingsPage::OnClearAllClick(IInspectable const&, RoutedEventArgs const&)
    {
        auto strong = get_strong();

        // Show confirmation dialog
        ContentDialog dialog;
        dialog.XamlRoot(this->XamlRoot());
        dialog.Title(box_value(L"Clear All Rules"));
        dialog.Content(box_value(L"Are you sure you want to remove all IP filter rules? This action cannot be undone."));
        dialog.PrimaryButtonText(L"Clear All");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Close);

        auto result = co_await dialog.ShowAsync();
        if (result != ContentDialogResult::Primary)
            co_return;

        auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
        mgr.ClearAllRules();
        mgr.ApplyToSession();

        RefreshRuleCount();
        ShowStatus(L"All rules cleared", InfoBarSeverity::Informational);
    }
}
