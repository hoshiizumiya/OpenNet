#include "XamlWorkaround.h"
#include "CreateTorrentDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/CreateTorrentDialog.g.cpp")
#include "UI/Xaml/View/Dialog/CreateTorrentDialog.g.cpp"
#endif

import OpenNet.Core.Torrent.TorrentCreator;
import OpenNet.Core.Utils.Message;
import OpenNet.Helpers.ThemeHelper;
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
		auto deferral = args.GetDeferral();
		args.Cancel(true);
		[getStrong = get_strong(), sender, args, deferral]() -> winrt::Windows::Foundation::IAsyncAction
		{
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

			auto const sourceText = getStrong->CreateTorrentSourceTextBox().Text();
			if (sourceText.empty())
			{
				getStrong->CreateTorrentStatusText().Text(ResourceGetString(L"CreateTorrentSelectSource"));
				co_return;
			}

			FileSavePicker picker(getStrong->XamlRoot().ContentIslandEnvironment().AppWindowId());
			picker.SuggestedStartLocation(PickerLocationId::Downloads);
			picker.SuggestedFileName(std::filesystem::path{ sourceText.c_str() }.filename().wstring());
			picker.DefaultFileExtension(L".torrent");
			auto fileTypes = winrt::single_threaded_vector<hstring>();
			fileTypes.Append(L".torrent");
			picker.FileTypeChoices().Insert(L"BitTorrent file", fileTypes);
			auto file = co_await picker.PickSaveFileAsync();
			if (!file)
			{
				co_return;
			}

			::OpenNet::Core::Torrent::TorrentCreationOptions options;
			options.sourcePath = std::filesystem::path{ sourceText.c_str() };
			switch (getStrong->CreateTorrentFormatComboBox().SelectedIndex())
			{
				case 1:
					options.format = ::OpenNet::Core::Torrent::TorrentFormat::V1;
					break;
				case 2:
					options.format = ::OpenNet::Core::Torrent::TorrentFormat::V2;
					break;
				default:
					options.format = ::OpenNet::Core::Torrent::TorrentFormat::Hybrid;
					break;
			}
			auto const privateChecked = getStrong->CreateTorrentPrivateCheckBox().IsChecked();
			auto const ignoreDotFilesChecked = getStrong->CreateTorrentIgnoreDotFilesCheckBox().IsChecked();
			options.privateTorrent = privateChecked && privateChecked.Value();
			options.ignoreDotFiles = ignoreDotFilesChecked && ignoreDotFilesChecked.Value();
			options.comment = winrt::to_string(getStrong->CreateTorrentCommentTextBox().Text());
			std::wistringstream trackerLines
			{
				getStrong->CreateTorrentTrackersTextBox().Text().c_str()
			};
			for (std::wstring line; std::getline(trackerLines, line);)
			{
				auto const first = line.find_first_not_of(L" \t\r");
				if (first == std::wstring::npos) continue;
				auto const last = line.find_last_not_of(L" \t\r");
				options.trackers.push_back(winrt::to_string(
					winrt::hstring{ line.substr(first, last - first + 1) }));
			}

			sender.IsPrimaryButtonEnabled(false);
			getStrong->CreateTorrentProgressRing().IsActive(true);
			getStrong->CreateTorrentProgressRing().Visibility(Visibility::Visible);
			getStrong->CreateTorrentStatusText().Text(ResourceGetString(L"CreateTorrentCreating"));
			winrt::apartment_context uiContext;
			auto const target = std::filesystem::path{ file.Path().c_str() };
			winrt::hstring failureMessage;
			try
			{
				co_await winrt::resume_background();
				auto creation = ::OpenNet::Core::Torrent::TorrentCreator::Create(options);
				::OpenNet::Core::Torrent::TorrentCreator::WriteFile(target, creation);
			}
			catch (std::exception const& exception)
			{
				failureMessage = winrt::to_hstring(exception.what());
			}
			co_await uiContext;
			sender.IsPrimaryButtonEnabled(true);
			getStrong->CreateTorrentProgressRing().IsActive(false);
			getStrong->CreateTorrentProgressRing().Visibility(Visibility::Collapsed);
			if (failureMessage.empty())
			{
				getStrong->CreateTorrentStatusText().Text(ResourceGetString(L"CreateTorrentComplete"));
				args.Cancel(false);
			}
			else
			{
				getStrong->CreateTorrentStatusText().Text(ResourceGetString(L"CreateTorrentFailed") + L" " + failureMessage);
			}
		}();
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
