#include "XamlWorkaround.h"
#include "HttpDownloadDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/HttpDownloadDialog.g.cpp")
#include "UI/Xaml/View/Dialog/HttpDownloadDialog.g.cpp"
#endif

import Core.Utils.Misc;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import OpenNet.Core.DownloadManager;
import OpenNet.Core.Aria2.Aria2Models;
import OpenNet.Core.TorrentSettings;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Content;
import winrt.Windows.ApplicationModel.DataTransfer;
import winrt.Windows.Web.Http;
import winrt.Microsoft.Windows.Storage.Pickers;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	HttpDownloadDialog::HttpDownloadDialog()
	{
		this->Style(Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	// ------------------------------------------------------------------
	//  Properties
	// ------------------------------------------------------------------

	winrt::hstring HttpDownloadDialog::Url() const
	{
		return m_url;
	}
	winrt::hstring HttpDownloadDialog::SaveDirectory() const
	{
		return m_saveDir;
	}
	winrt::hstring HttpDownloadDialog::FileName() const
	{
		return m_fileName;
	}
	bool HttpDownloadDialog::IsUrlValid() const
	{
		return m_isUrlValid;
	}
	int32_t HttpDownloadDialog::ConnectionsPerServer() const
	{
		return m_connectionsPerServer;
	}
	int64_t HttpDownloadDialog::MaximumDownloadRate() const
	{
		return m_maximumDownloadRate;
	}
	bool HttpDownloadDialog::StartPaused() const
	{
		return m_startPaused;
	}
	winrt::hstring HttpDownloadDialog::Referer() const
	{
		return m_referer;
	}
	winrt::hstring HttpDownloadDialog::UserAgent() const
	{
		return m_userAgent;
	}
	winrt::hstring HttpDownloadDialog::Cookie() const
	{
		return m_cookie;
	}
	winrt::hstring HttpDownloadDialog::Headers() const
	{
		return m_headers;
	}
	winrt::hstring HttpDownloadDialog::Mirrors() const
	{
		return m_mirrors;
	}
	winrt::hstring HttpDownloadDialog::Checksum() const
	{
		return m_checksum;
	}
	winrt::hstring HttpDownloadDialog::Description() const
	{
		return m_description;
	}
	winrt::hstring HttpDownloadDialog::Username() const
	{
		return m_username;
	}
	winrt::hstring HttpDownloadDialog::Password() const
	{
		return m_password;
	}

	// ------------------------------------------------------------------
	//  URL validation
	// ------------------------------------------------------------------

	bool HttpDownloadDialog::ValidateUrl(winrt::hstring const& url) const
	{
		if (url.empty())
			return false;

		std::wstring lower{ url };
		std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

		return lower.starts_with(L"http://") ||
			lower.starts_with(L"https://") ||
			lower.starts_with(L"ftp://");
	}

	// ------------------------------------------------------------------
	//  XAML event handlers
	// ------------------------------------------------------------------

	void HttpDownloadDialog::UrlBox_TextChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& /*args*/)
	{
		auto text = UrlBox().Text();
		m_isUrlValid = ValidateUrl(text);
		UrlErrorInfoBar().IsOpen(!m_isUrlValid && !text.empty());
	}

	fire_and_forget HttpDownloadDialog::DialogRoot_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		if (m_saveDir.empty())
		{
			m_saveDir = winrt::hstring{ ::OpenNet::Core::TorrentSettingsManager::Instance().Get().defaultSavePath };
			SaveDirBox().Text(m_saveDir);
			UpdateDiskSpace();
		}
		// preview clipboard content whether it's a valid URL
		try
		{
			auto weak = this->get_weak();
			m_event_revoker = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::ContentChanged(winrt::auto_revoke, [weak](auto&&, auto&&) -> winrt::fire_and_forget
			{
				auto self = weak.get();
				auto strong = self ? self->get_strong() : nullptr;
				if (strong == nullptr)
				{
					co_return;
				}
				strong->ClipboardPreview().Text(co_await Core::Utils::Misc::getCurrentClipboardText());
				strong->ClipboardPreview().Visibility(Visibility::Visible);
			});

			auto clipboardContent = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();
			if (clipboardContent.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text()))
			{
				auto text = co_await clipboardContent.GetTextAsync();
				ClipboardPreview().Text(text);
				ClipboardPreview().Visibility(Visibility::Visible);
				if (ValidateUrl(text))
				{
					UrlBox().Text(text);
				}
			}
		}
		catch (...)
		{
			// Clipboard access may fail
		}
	}

	void HttpDownloadDialog::CaptureValues(bool const startPaused)
	{
		m_url = UrlBox().Text();
		m_saveDir = SaveDirBox().Text();
		m_fileName = FileNameBox().Text();
		m_connectionsPerServer = static_cast<int32_t>(std::clamp(ConnectionsNumberBox().Value(), 1.0, 16.0));
		m_maximumDownloadRate = static_cast<int64_t>((std::max)(0.0, MaximumRateNumberBox().Value()) * 1024.0);
		m_startPaused = startPaused;
		m_referer = RefererBox().Text();
		m_userAgent = UserAgentBox().Text();
		m_cookie = CookieBox().Text();
		m_headers = HeadersBox().Text();
		m_mirrors = MirrorsBox().Text();
		m_checksum = ChecksumBox().Text();
		m_description = DescriptionBox().Text();
		m_username = UsernameBox().Text();
		m_password = PasswordBox().Password();
	}

	void HttpDownloadDialog::UpdateDiskSpace()
	{
		if (SaveDirBox().Text().empty()) return;
		std::error_code error;
		auto const space = std::filesystem::space(std::filesystem::path{ SaveDirBox().Text().c_str() }, error);
		if (!error) DiskSpaceText().Text(std::format(L"Free: {:.1f} GiB", static_cast<double>(space.available) / 1073741824.0));
	}

	void HttpDownloadDialog::OnPrimaryButtonClick(ContentDialog const&, ContentDialogButtonClickEventArgs const& args)
	{
		args.Cancel(true);
		StartDownloadAsync(false);
	}

	void HttpDownloadDialog::OnSecondaryButtonClick(ContentDialog const&, ContentDialogButtonClickEventArgs const& args)
	{
		args.Cancel(true);
		StartDownloadAsync(true);
	}

	void HttpDownloadDialog::AppendLines(std::vector<std::string>& destination, winrt::hstring const& text)
	{
		std::wistringstream lines{ text.c_str() };
		for (std::wstring line; std::getline(lines, line);)
		{
			auto const first = line.find_first_not_of(L" \t\r");
			if (first == std::wstring::npos) continue;
			auto const last = line.find_last_not_of(L" \t\r");
			destination.push_back(winrt::to_string(winrt::hstring{ line.substr(first, last - first + 1) }));
		}
	}

	winrt::fire_and_forget HttpDownloadDialog::StartDownloadAsync(bool const startPaused)
	{
		auto lifetime = get_strong();
		if (!ValidateUrl(UrlBox().Text()))
		{
			UrlErrorInfoBar().IsOpen(true);
			co_return;
		}
		IsPrimaryButtonEnabled(false);
		IsSecondaryButtonEnabled(false);
		CaptureValues(startPaused);
		::OpenNet::Core::Aria2::HttpDownloadOptions options;
		options.Uris.push_back(winrt::to_string(m_url));
		AppendLines(options.Uris, m_mirrors);
		AppendLines(options.Headers, m_headers);
		options.Dir = winrt::to_string(m_saveDir);
		options.OutFileName = winrt::to_string(m_fileName);
		options.ConnectionsPerServer = static_cast<std::uint32_t>(m_connectionsPerServer);
		options.MaximumDownloadRate = static_cast<std::uint64_t>(m_maximumDownloadRate);
		options.StartPaused = m_startPaused;
		options.Referer = winrt::to_string(m_referer);
		options.UserAgent = winrt::to_string(m_userAgent);
		options.Cookie = winrt::to_string(m_cookie);
		options.Username = winrt::to_string(m_username);
		options.Password = winrt::to_string(m_password);
		options.Description = winrt::to_string(m_description);
		auto checksum = winrt::to_string(m_checksum);
		std::erase_if(checksum, [](unsigned char value)
		{
			return std::isspace(value);
		});
		if (!checksum.empty() && !checksum.contains('='))
		{
			if (checksum.size() == 32) checksum = "md5=" + checksum;
			else if (checksum.size() == 40) checksum = "sha-1=" + checksum;
			else if (checksum.size() == 64) checksum = "sha-256=" + checksum;
			else if (checksum.size() == 128) checksum = "sha-512=" + checksum;
		}
		options.Checksum = std::move(checksum);
		auto dispatcher = DispatcherQueue();
		auto weak = get_weak();
		co_await winrt::resume_background();
		auto gid = ::OpenNet::Core::DownloadManager::Instance().AddHttpDownload(options);
		dispatcher.TryEnqueue([weak, gid = std::move(gid)]()
		{
			if (auto self = weak.get())
			{
				if (!gid.empty()) self->Hide();
				else
				{
					self->IsPrimaryButtonEnabled(true);
					self->IsSecondaryButtonEnabled(true);
					self->UrlErrorInfoBar().Title(L"Unable to add download");
					self->UrlErrorInfoBar().Message(L"aria2 is unavailable or rejected the supplied options.");
					self->UrlErrorInfoBar().IsOpen(true);
				}
			}
		});
	}

	winrt::Windows::Foundation::IAsyncAction HttpDownloadDialog::FetchMetadataButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		if (!ValidateUrl(UrlBox().Text()))
		{
			UrlErrorInfoBar().IsOpen(true);
			co_return;
		}
		FetchMetadataButton().IsEnabled(false);
		try
		{
			winrt::Windows::Web::Http::HttpClient client;
			winrt::Windows::Web::Http::HttpRequestMessage request{ winrt::Windows::Web::Http::HttpMethod::Head(), winrt::Windows::Foundation::Uri{ UrlBox().Text() } };
			auto response = co_await client.SendRequestAsync(request);
			if (!response.IsSuccessStatusCode()) throw winrt::hresult_error(E_FAIL, L"HTTP " + winrt::to_hstring(static_cast<int>(response.StatusCode())));
			if (auto length = response.Content().Headers().ContentLength())
				FileSizeText().Text(std::format(L"Size: {:.2f} MiB", static_cast<double>(length.Value()) / 1048576.0));
			if (FileNameBox().Text().empty())
			{
				auto const path = response.RequestMessage().RequestUri().Path();
				std::wstring_view const pathView{ path };
				auto const slash = pathView.find_last_of(L'/');
				if (slash != std::wstring_view::npos && slash + 1 < pathView.size()) FileNameBox().Text(winrt::hstring{ pathView.substr(slash + 1) });
			}
			UrlErrorInfoBar().IsOpen(false);
		}
		catch (winrt::hresult_error const& error)
		{
			UrlErrorInfoBar().Title(L"Unable to retrieve file information");
			UrlErrorInfoBar().Message(error.message());
			UrlErrorInfoBar().IsOpen(true);
		}
		FetchMetadataButton().IsEnabled(true);
	}

	winrt::Windows::Foundation::IAsyncAction HttpDownloadDialog::PasteUrlButton_Click(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
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

	winrt::Windows::Foundation::IAsyncAction HttpDownloadDialog::BrowseDirButton_Click(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		try
		{
			auto picker = winrt::Microsoft::Windows::Storage::Pickers::FolderPicker(XamlRoot().ContentIslandEnvironment().AppWindowId());
			picker.ViewMode(winrt::Microsoft::Windows::Storage::Pickers::PickerViewMode::List);
			picker.SuggestedStartLocation(winrt::Microsoft::Windows::Storage::Pickers::PickerLocationId::Downloads);

			auto folder = co_await picker.PickSingleFolderAsync();
			if (folder)
			{
				m_saveDir = folder.Path();
				SaveDirBox().Text(m_saveDir);
				UpdateDiskSpace();
			}
		}
		catch (...)
		{
			// Picker may be cancelled
		}
	}
}
