#pragma once

#include "UI/Xaml/View/Dialog/HttpDownloadDialog.g.h"
#include "ViewModels/ObservableMixin.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
    struct HttpDownloadDialog : HttpDownloadDialogT<HttpDownloadDialog>,
                                ::OpenNet::ViewModels::ObservableMixin<HttpDownloadDialog>
    {
        HttpDownloadDialog();

        // IDL-exposed read-only properties
        winrt::hstring Url() const;
        winrt::hstring SaveDirectory() const;
        winrt::hstring FileName() const;
        bool IsUrlValid() const;

        // XAML event handlers
        void OnPrimaryButtonClick(
            winrt::Microsoft::UI::Xaml::Controls::ContentDialog const &sender,
            winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const &args);
        winrt::Windows::Foundation::IAsyncAction PasteUrlButton_Click(
            winrt::Windows::Foundation::IInspectable const &sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        winrt::Windows::Foundation::IAsyncAction BrowseDirButton_Click(
            winrt::Windows::Foundation::IInspectable const &sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void UrlBox_TextChanged(
            winrt::Windows::Foundation::IInspectable const &sender,
            winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const &args);

    private:
        bool ValidateUrl(winrt::hstring const &url) const;

        winrt::hstring m_url;
        winrt::hstring m_saveDir;
        winrt::hstring m_fileName;
        bool m_isUrlValid{false};
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
    struct HttpDownloadDialog : HttpDownloadDialogT<HttpDownloadDialog, implementation::HttpDownloadDialog>
    {
    };
}
