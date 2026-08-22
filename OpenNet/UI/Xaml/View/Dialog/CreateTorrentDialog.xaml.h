#pragma once

#include "UI/Xaml/View/Dialog/CreateTorrentDialog.g.h"

import OpenNet.Core.Torrent.TorrentCreator;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct CreateTorrentDialog : CreateTorrentDialogT<CreateTorrentDialog>
	{
		CreateTorrentDialog();

		void CreateTorrentDialog_PrimaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction CreateTorrentBrowseFile_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction CreateTorrentBrowseFolder_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

	private:
		winrt::Windows::Foundation::IAsyncAction PickTargetAndStartAsync(::OpenNet::Core::Torrent::TorrentCreationOptions options, bool startSeeding, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs args, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickDeferral deferral);

		winrt::Windows::Foundation::IAsyncAction m_creationAction{ nullptr };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct CreateTorrentDialog : CreateTorrentDialogT<CreateTorrentDialog, implementation::CreateTorrentDialog>
	{
	};
}
