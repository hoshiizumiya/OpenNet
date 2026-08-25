#include "XamlWorkaround.h"
#include "CreateTorrentDialog.xaml.h"
#include "UI/Xaml/View/Windows/CreateTorrentProgressWindow.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/CreateTorrentDialog.g.cpp")
#include "UI/Xaml/View/Dialog/CreateTorrentDialog.g.cpp"
#endif

import OpenNet.Core.Torrent.TorrentCreator;
import OpenNet.Core.Utils.Message;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.Windows.Storage.Pickers;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::Windows::Storage::Pickers;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	CreateTorrentDialog::CreateTorrentDialog()
	{
		this->Style(Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	void CreateTorrentDialog::CreateTorrentDialog_PrimaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args)
	{
		::OpenNet::Core::Torrent::TorrentCreationOptions options;
		options.sourcePath = std::filesystem::path{ CreateTorrentSourceTextBox().Text().c_str() };
		if (options.sourcePath.empty())
		{
			CreateTorrentStatusText().Text(ResourceGetString(L"CreateTorrentSelectSource"));
			args.Cancel(true);
			return;
		}
		switch (CreateTorrentFormatComboBox().SelectedIndex())
		{
			case 1: options.format = ::OpenNet::Core::Torrent::TorrentFormat::V1; break;
			case 2: options.format = ::OpenNet::Core::Torrent::TorrentFormat::V2; break;
			default: options.format = ::OpenNet::Core::Torrent::TorrentFormat::Hybrid; break;
		}
		auto const pieceSizeIndex = CreateTorrentPieceSizeComboBox().SelectedIndex();
		options.pieceSize = pieceSizeIndex > 0 ? 1024 << (pieceSizeIndex + 3) : 0;
		auto const privateChecked = CreateTorrentPrivateCheckBox().IsChecked();
		auto const ignoreDotFilesChecked = CreateTorrentIgnoreDotFilesCheckBox().IsChecked();
		auto const startSeedingChecked = CreateTorrentStartSeedingCheckBox().IsChecked();
		options.privateTorrent = privateChecked && privateChecked.Value();
		options.ignoreDotFiles = ignoreDotFilesChecked && ignoreDotFilesChecked.Value();
		options.comment = winrt::to_string(CreateTorrentCommentTextBox().Text());
		options.source = winrt::to_string(CreateTorrentSourceFieldTextBox().Text());
		auto parseLines = [](hstring const& text)
		{
			std::vector<std::string> values;
			std::wistringstream lines{ text.c_str() };
			for (std::wstring line; std::getline(lines, line);)
			{
				auto const first = line.find_first_not_of(L" \t\r");
				if (first == std::wstring::npos) continue;
				auto const last = line.find_last_not_of(L" \t\r");
				values.push_back(winrt::to_string(hstring{ line.substr(first, last - first + 1) }));
			}
			return values;
		};
		options.trackers = parseLines(CreateTorrentTrackersTextBox().Text());
		options.urlSeeds = parseLines(CreateTorrentUrlSeedsTextBox().Text());
		auto const startSeeding = startSeedingChecked && startSeedingChecked.Value();

		auto deferral = args.GetDeferral();
		args.Cancel(true);
		m_creationAction = PickTargetAndStartAsync(std::move(options), startSeeding, args, deferral);
	}

	winrt::Windows::Foundation::IAsyncAction CreateTorrentDialog::PickTargetAndStartAsync(::OpenNet::Core::Torrent::TorrentCreationOptions options, bool const startSeeding, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs args, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickDeferral deferral)
	{
		auto lifetime = get_strong();
		struct DeferralCompletion
		{
			winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickDeferral Deferral;
			~DeferralCompletion() noexcept
			{
				try
				{
					Deferral.Complete();
				}
				catch (...)
				{
				}
			}
		} completion{ deferral };

		FileSavePicker picker(XamlRoot().ContentIslandEnvironment().AppWindowId());
		picker.SuggestedStartLocation(PickerLocationId::Downloads);
		picker.SuggestedFileName(options.sourcePath.filename().wstring());
		picker.DefaultFileExtension(L".torrent");
		auto fileTypes = winrt::single_threaded_vector<hstring>();
		fileTypes.Append(L".torrent");
		picker.FileTypeChoices().Insert(L"BitTorrent file", fileTypes);
		auto file = co_await picker.PickSaveFileAsync();
		if (!file) co_return;

		auto const target = std::filesystem::path{ file.Path().c_str() };
		try
		{
			auto window = winrt::make_self<winrt::OpenNet::UI::Xaml::View::Windows::implementation::CreateTorrentProgressWindow>(std::move(options), target, startSeeding);
			::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(*window);
			window->Activate();
			args.Cancel(false);
		}
		catch (winrt::hresult_error const& error)
		{
			CreateTorrentStatusText().Text(ResourceGetString(L"CreateTorrentFailed") + L" " + error.message());
		}
		catch (std::exception const& error)
		{
			CreateTorrentStatusText().Text(ResourceGetString(L"CreateTorrentFailed") + L" " + to_hstring(error.what()));
		}
	}

	winrt::Windows::Foundation::IAsyncAction CreateTorrentDialog::CreateTorrentBrowseFile_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		FileOpenPicker picker(XamlRoot().ContentIslandEnvironment().AppWindowId());
		picker.ViewMode(PickerViewMode::List);
		picker.SuggestedStartLocation(PickerLocationId::ComputerFolder);
		picker.FileTypeFilter().Append(L"*");
		if (auto file = co_await picker.PickSingleFileAsync())
		{
			CreateTorrentSourceTextBox().Text(file.Path());
			CreateTorrentStatusText().Text(L"");
		}
	}

	winrt::Windows::Foundation::IAsyncAction CreateTorrentDialog::CreateTorrentBrowseFolder_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		FolderPicker picker(XamlRoot().ContentIslandEnvironment().AppWindowId());
		picker.SuggestedStartLocation(PickerLocationId::Downloads);
		if (auto folder = co_await picker.PickSingleFolderAsync())
		{
			CreateTorrentSourceTextBox().Text(folder.Path());
			CreateTorrentStatusText().Text(L"");
		}
	}
}
