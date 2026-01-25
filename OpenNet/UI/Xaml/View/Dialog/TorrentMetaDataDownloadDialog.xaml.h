#pragma once

#include "UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
    struct TorrentMetaDataDownloadDialog : TorrentMetaDataDownloadDialogT<TorrentMetaDataDownloadDialog>
    {
        TorrentMetaDataDownloadDialog();

        void OnPrimaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender,
            winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args);
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
    struct TorrentMetaDataDownloadDialog : TorrentMetaDataDownloadDialogT<TorrentMetaDataDownloadDialog, implementation::TorrentMetaDataDownloadDialog>
    {
    };
}
