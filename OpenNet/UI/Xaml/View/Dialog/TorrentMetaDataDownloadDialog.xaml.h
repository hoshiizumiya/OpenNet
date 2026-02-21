#pragma once

#include "UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.g.h"
#include "ViewModels/ObservableMixin.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct TorrentMetaDataDownloadDialog : TorrentMetaDataDownloadDialogT<TorrentMetaDataDownloadDialog>, ::OpenNet::ViewModels::ObservableMixin<TorrentMetaDataDownloadDialog>
	{
		TorrentMetaDataDownloadDialog();

		void OnPrimaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender,
								  winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction PasteTextFromTheClipboard_Click(IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		// Property used by x:Bind in XAML (InfoBar.IsOpen)
		bool IsLinkValid();
		void IsLinkValid(bool value);

		// Get the validated magnet link entered by user
		winrt::hstring GetMagnetLink() const;

		// INotifyPropertyChanged is provided by mvvm::WrapNotifyPropertyChanged mixin
	private:
		bool m_isLinkValid{ false };
		winrt::hstring m_validatedMagnetLink{};
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct TorrentMetaDataDownloadDialog : TorrentMetaDataDownloadDialogT<TorrentMetaDataDownloadDialog, implementation::TorrentMetaDataDownloadDialog>
	{};
}
