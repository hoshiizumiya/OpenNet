#include "pch.h"
#include "TorrentMetaDataDownloadDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.g.cpp")
#include "UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Storage.Pickers.h>
#include "Core/P2PManager.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Storage::Pickers;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	TorrentMetaDataDownloadDialog::TorrentMetaDataDownloadDialog()
	{
		InitializeComponent();
	}

	void TorrentMetaDataDownloadDialog::OnPrimaryButtonClick(ContentDialog const& sender,
															 ContentDialogButtonClickEventArgs const& args)
	{
		(void)sender;
		(void)args;

		auto magnetBox = MagnetBox();

		// the torrent file need to save on it's temp folder firstly
		//auto savePathBox = SavePathBox();

		//if (!magnetBox || !savePathBox) return;

		//auto magnet = to_string(magnetBox.Text());
		//if (!magnet.empty())
		//{
		//	auto save = to_string(savePathBox.Text());
		//	if (save.empty()) save = ".";

		//	// Fire and forget the async operation
		//	[](std::string magnetUri, std::string savePath) -> fire_and_forget
		//		{
		//			co_await ::OpenNet::Core::P2PManager::Instance().AddMagnetAsync(magnetUri, savePath);
		//		}(magnet, save);
		//}
	}

}
