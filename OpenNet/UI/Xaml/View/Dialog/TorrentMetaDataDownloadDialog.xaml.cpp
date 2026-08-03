// warning C4189
#include "XamlWorkaround.h"
import winrt.XamlToolkit.Labs.WinUI;
#include "TorrentMetaDataDownloadDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.g.cpp")
#include "UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.g.cpp"
#endif

#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.xaml.h"
#include "UI/Xaml/View/Pages/TasksPage.xaml.h"

import Core.Utils.Misc;
import OpenNet.Core.P2PManager;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.ViewModels.ObservableMixin;
import winrt.Windows.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.Windows.ApplicationModel.Resources;
import winrt.Windows.Storage.Pickers;
import winrt.Windows.ApplicationModel.DataTransfer;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
using namespace winrt::Windows::Storage::Pickers;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	TorrentMetaDataDownloadDialog::TorrentMetaDataDownloadDialog()
	{
		this->Style(Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());

		CloseButtonText(ResourceLoader().GetString(L"Cancel"));
		PrimaryButtonText(ResourceLoader().GetString(L"OK"));

		// Keep the tray "Capture Clipboard URL" flow frictionless: when the
		// add-from-URL dialog opens, prefill a valid torrent link if one is
		// currently on the clipboard.
		auto weak = get_weak();
		Loaded([weak](auto const&, auto const&) -> winrt::fire_and_forget
		{
			auto self = weak.get();
			auto strong = self ? self->get_strong() : nullptr;
			if (!strong)
			{
				co_return;
			}
			try
			{
				auto clipboard =
					winrt::Windows::ApplicationModel::DataTransfer::Clipboard::
						GetContent();
				if (clipboard.Contains(
					winrt::Windows::ApplicationModel::DataTransfer::
						StandardDataFormats::Text()))
				{
					auto text = co_await clipboard.GetTextAsync();
					if (Core::Utils::Misc::isTorrentLink(text))
					{
						strong->MagnetBox().Text(text);
						strong->IsLinkValid(false);
					}
				}
			}
			catch (...)
			{
			}
		});
	}

	void TorrentMetaDataDownloadDialog::OnPrimaryButtonClick(ContentDialog const& /*sender*/, ContentDialogButtonClickEventArgs const& args)
	{
		//HWND hwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(winrt::OpenNet::implementation::App::window);
		//BOOL enabled = IsWindowEnabled(hwnd);
		auto magnetBox = MagnetBox();

		// auto magnet = to_string(magnetBox.Text());
		const auto text = magnetBox.Text();
		if (!Core::Utils::Misc::isTorrentLink(text))
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
			m_validatedMagnetLink = text;
		}
	}

	winrt::Windows::Foundation::IAsyncAction TorrentMetaDataDownloadDialog::PasteTextFromTheClipboard_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*args*/)
	{
		auto clipboard = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();
		if (clipboard.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text()))
		{
			auto text = co_await clipboard.GetTextAsync();
			if (Core::Utils::Misc::isTorrentLink(text))
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

	winrt::hstring TorrentMetaDataDownloadDialog::GetMagnetLink() const
	{
		return m_validatedMagnetLink;
	}
}
