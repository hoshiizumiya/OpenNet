#include "pch.h"
#include "TorrentMetaDataDownloadDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.g.cpp")
#include "UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>  
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include "Core/P2PManager.h"
#include "Helpers/ThemeHelper.h"
#include "Core/Utils/Misc.h"
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
using namespace winrt::Windows::Storage::Pickers;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	TorrentMetaDataDownloadDialog::TorrentMetaDataDownloadDialog()
	{
		InitializeComponent();
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());

		CloseButtonText(ResourceLoader().GetString(L"Cancel"));
		PrimaryButtonText(ResourceLoader().GetString(L"OK"));
	}

	void TorrentMetaDataDownloadDialog::OnPrimaryButtonClick(ContentDialog const& /*sender*/, ContentDialogButtonClickEventArgs const& args)
	{
		auto magnetBox = MagnetBox();

		// auto magnet = to_string(magnetBox.Text());
		const auto text = magnetBox.Text();
		if (!Utils::Misc::isTorrentLink(text))
		{
			// Prevent dialog from closing by marking link invalid which is bound to InfoBar
			IsLinkValid(true);
			// Cancel default close
			args.Cancel(true);
			return;
		}
		else
		{
			IsLinkValid(false);
			// proceed to add magnet
			// Convert to std::string (UTF-8)
			std::string magnetUri = winrt::to_string(text);
			auto TorrentCheckModalWindow = winrt::make<winrt::OpenNet::UI::Xaml::View::Windows::implementation::TorrentCheckModalWindow>(text);
		}
	}

	winrt::Windows::Foundation::IAsyncAction TorrentMetaDataDownloadDialog::PasteTextFromTheClipboard_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*args*/)
	{
		auto clipboard = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();
		if (clipboard.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text()))
		{
			auto text = co_await clipboard.GetTextAsync();
			if (Utils::Misc::isTorrentLink(text))
			{
				MagnetBox().Text(text);
				IsLinkValid(false);
			}
			else
			{
				MagnetBox().Text(L"-Incorrect link format! Please check your clipboard first (Win + V)-");
				IsLinkValid(true);
			}
		}
	}


	bool TorrentMetaDataDownloadDialog::IsLinkValid()
	{
		return m_isLinkValid;
	}

	void TorrentMetaDataDownloadDialog::IsLinkValid(bool value)
	{
		// Use ObservableMixin helper to set and notify
		this->SetProperty(m_isLinkValid, value, L"IsLinkValid");
	}
}
