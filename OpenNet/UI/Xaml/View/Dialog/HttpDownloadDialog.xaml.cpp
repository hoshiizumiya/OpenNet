#include "pch.h"
#include "HttpDownloadDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/HttpDownloadDialog.g.cpp")
#include "UI/Xaml/View/Dialog/HttpDownloadDialog.g.cpp"
#endif

#include "Helpers/ThemeHelper.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>

#include <algorithm>
#include <string>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
    HttpDownloadDialog::HttpDownloadDialog()
    {
        RequestedTheme(::OpenNet::Helpers::ThemeHelper::ThemeHelper::RootTheme());
    }

    // ------------------------------------------------------------------
    //  Properties
    // ------------------------------------------------------------------

    winrt::hstring HttpDownloadDialog::Url() const { return m_url; }
    winrt::hstring HttpDownloadDialog::SaveDirectory() const { return m_saveDir; }
    winrt::hstring HttpDownloadDialog::FileName() const { return m_fileName; }
    bool HttpDownloadDialog::IsUrlValid() const { return m_isUrlValid; }

    // ------------------------------------------------------------------
    //  URL validation
    // ------------------------------------------------------------------

    bool HttpDownloadDialog::ValidateUrl(winrt::hstring const &url) const
    {
        if (url.empty())
            return false;

        std::wstring lower{url};
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

        return lower.starts_with(L"http://") ||
               lower.starts_with(L"https://") ||
               lower.starts_with(L"ftp://");
    }

    // ------------------------------------------------------------------
    //  XAML event handlers
    // ------------------------------------------------------------------

    void HttpDownloadDialog::UrlBox_TextChanged(
        winrt::Windows::Foundation::IInspectable const & /*sender*/,
        winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const & /*args*/)
    {
        auto text = UrlBox().Text();
        m_isUrlValid = ValidateUrl(text);
        UrlErrorInfoBar().IsOpen(!m_isUrlValid && !text.empty());
    }

    void HttpDownloadDialog::OnPrimaryButtonClick(
        ContentDialog const & /*sender*/,
        ContentDialogButtonClickEventArgs const &args)
    {
        auto url = UrlBox().Text();
        if (!ValidateUrl(url))
        {
            UrlErrorInfoBar().IsOpen(true);
            args.Cancel(true);
            return;
        }

        m_url = url;
        m_fileName = FileNameBox().Text();
        // m_saveDir is already set by BrowseDirButton_Click
    }

    winrt::Windows::Foundation::IAsyncAction HttpDownloadDialog::PasteUrlButton_Click(
        winrt::Windows::Foundation::IInspectable const & /*sender*/,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const & /*e*/)
    {
        try
        {
            auto clipboard = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();
            if (clipboard.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text()))
            {
                auto text = co_await clipboard.GetTextAsync();
                UrlBox().Text(text);
            }
        }
        catch (...)
        {
            // Clipboard access may fail
        }
    }

    winrt::Windows::Foundation::IAsyncAction HttpDownloadDialog::BrowseDirButton_Click(
        winrt::Windows::Foundation::IInspectable const & /*sender*/,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const & /*e*/)
    {
        try
        {
            auto picker = winrt::Microsoft::Windows::Storage::Pickers::FolderPicker(
                XamlRoot().ContentIslandEnvironment().AppWindowId());
            picker.ViewMode(winrt::Microsoft::Windows::Storage::Pickers::PickerViewMode::List);
            picker.SuggestedStartLocation(
                winrt::Microsoft::Windows::Storage::Pickers::PickerLocationId::Downloads);

            auto folder = co_await picker.PickSingleFolderAsync();
            if (folder)
            {
                m_saveDir = folder.Path();
                SaveDirBox().Text(m_saveDir);
            }
        }
        catch (...)
        {
            // Picker may be cancelled
        }
    }
}
