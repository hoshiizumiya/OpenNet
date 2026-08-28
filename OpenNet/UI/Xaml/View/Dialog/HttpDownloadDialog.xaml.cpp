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
		Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		m_saveDir = winrt::hstring{ ::OpenNet::Core::TorrentSettingsManager::Instance().Get().defaultSavePath };
		UpdateDiskSpace();
	}

	void HttpDownloadDialog::InitializeComponent()
	{
		HttpDownloadDialogT::InitializeComponent();
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	// ------------------------------------------------------------------
	//  Properties
	// ------------------------------------------------------------------

	winrt::hstring HttpDownloadDialog::Url() const
	{
		return m_url;
	}
	void HttpDownloadDialog::Url(hstring const& value)
	{
		auto text = std::wstring_view{ value };
		auto const first = text.find_first_not_of(L" \t\r\n");
		auto const last = text.find_last_not_of(L" \t\r\n");
		auto normalized = first == std::wstring_view::npos
			? hstring{}
		: hstring{ text.substr(first, last - first + 1) };
		if (!normalized.empty() && !ValidateUrl(normalized))
		{
			auto const candidate = std::wstring_view{ normalized };
			if (candidate.find(L' ') == std::wstring_view::npos
				&& candidate.find(L'.') != std::wstring_view::npos
				&& candidate.find(L"://") == std::wstring_view::npos)
			{
				normalized = L"https://" + normalized;
			}
		}
		if (!SetProperty(m_url, normalized, L"Url")) return;
		m_isUrlValid = ValidateUrl(m_url);
		RaisePropertyChanged(L"IsUrlValid");
		RaisePropertyChanged(L"CanFetchMetadata");
		m_isErrorOpen = !m_url.empty() && !m_isUrlValid;
		RaisePropertyChanged(L"IsErrorOpen");
	}
	winrt::hstring HttpDownloadDialog::SaveDirectory() const
	{
		return m_saveDir;
	}
	void HttpDownloadDialog::SaveDirectory(hstring const& value)
	{
		if (SetProperty(m_saveDir, value, L"SaveDirectory")) UpdateDiskSpace();
	}
	winrt::hstring HttpDownloadDialog::FileName() const
	{
		return m_fileName;
	}
	void HttpDownloadDialog::FileName(hstring const& value)
	{
		SetProperty(m_fileName, value, L"FileName");
	}
	bool HttpDownloadDialog::IsUrlValid() const
	{
		return m_isUrlValid;
	}
	double HttpDownloadDialog::ConnectionsPerServer() const
	{
		return static_cast<double>(m_connectionsPerServer);
	}
	void HttpDownloadDialog::ConnectionsPerServer(double const value)
	{
		SetProperty(m_connectionsPerServer, static_cast<int32_t>(std::clamp(value, 1.0, 16.0)), L"ConnectionsPerServer");
	}
	double HttpDownloadDialog::MaximumDownloadRate() const
	{
		return static_cast<double>(m_maximumDownloadRate);
	}
	void HttpDownloadDialog::MaximumDownloadRate(double const value)
	{
		SetProperty(m_maximumDownloadRate, static_cast<int64_t>((std::max)(0.0, value)), L"MaximumDownloadRate");
	}
	bool HttpDownloadDialog::StartPaused() const
	{
		return m_startPaused;
	}
	winrt::hstring HttpDownloadDialog::Referer() const
	{
		return m_referer;
	}
	void HttpDownloadDialog::Referer(hstring const& value)
	{
		SetProperty(m_referer, value, L"Referer");
	}
	winrt::hstring HttpDownloadDialog::UserAgent() const
	{
		return m_userAgent;
	}
	void HttpDownloadDialog::UserAgent(hstring const& value)
	{
		SetProperty(m_userAgent, value, L"UserAgent");
	}
	winrt::hstring HttpDownloadDialog::Cookie() const
	{
		return m_cookie;
	}
	void HttpDownloadDialog::Cookie(hstring const& value)
	{
		SetProperty(m_cookie, value, L"Cookie");
	}
	winrt::hstring HttpDownloadDialog::Headers() const
	{
		return m_headers;
	}
	void HttpDownloadDialog::Headers(hstring const& value)
	{
		SetProperty(m_headers, value, L"Headers");
	}
	winrt::hstring HttpDownloadDialog::Mirrors() const
	{
		return m_mirrors;
	}
	void HttpDownloadDialog::Mirrors(hstring const& value)
	{
		SetProperty(m_mirrors, value, L"Mirrors");
	}
	winrt::hstring HttpDownloadDialog::Checksum() const
	{
		return m_checksum;
	}
	void HttpDownloadDialog::Checksum(hstring const& value)
	{
		SetProperty(m_checksum, value, L"Checksum");
	}
	winrt::hstring HttpDownloadDialog::Description() const
	{
		return m_description;
	}
	void HttpDownloadDialog::Description(hstring const& value)
	{
		SetProperty(m_description, value, L"Description");
	}
	winrt::hstring HttpDownloadDialog::Username() const
	{
		return m_username;
	}
	void HttpDownloadDialog::Username(hstring const& value)
	{
		SetProperty(m_username, value, L"Username");
	}
	winrt::hstring HttpDownloadDialog::Password() const
	{
		return m_password;
	}
	void HttpDownloadDialog::Password(hstring const& value)
	{
		SetProperty(m_password, value, L"Password");
	}
	winrt::hstring HttpDownloadDialog::ClipboardPreviewText() const
	{
		return m_clipboardPreviewText;
	}
	Visibility HttpDownloadDialog::ClipboardPreviewVisibility() const
	{
		return m_clipboardPreviewVisibility;
	}
	winrt::hstring HttpDownloadDialog::FileSizeText() const
	{
		return m_fileSizeText;
	}
	winrt::hstring HttpDownloadDialog::DiskSpaceText() const
	{
		return m_diskSpaceText;
	}
	winrt::hstring HttpDownloadDialog::ResumeSupportText() const
	{
		return m_resumeSupportText;
	}
	bool HttpDownloadDialog::IsMetadataLoading() const
	{
		return m_isMetadataLoading;
	}
	bool HttpDownloadDialog::CanFetchMetadata() const
	{
		return !m_isMetadataLoading && m_isUrlValid;
	}
	winrt::hstring HttpDownloadDialog::ErrorTitle() const
	{
		return m_errorTitle;
	}
	winrt::hstring HttpDownloadDialog::ErrorMessage() const
	{
		return m_errorMessage;
	}
	bool HttpDownloadDialog::IsErrorOpen() const
	{
		return m_isErrorOpen;
	}

	// ------------------------------------------------------------------
	//  URL validation
	// ------------------------------------------------------------------

	bool HttpDownloadDialog::ValidateUrl(winrt::hstring const& url) const
	{
		if (url.empty()) return false;
		try
		{
			winrt::Windows::Foundation::Uri const uri{ url };
			auto scheme = std::wstring{ uri.SchemeName() };
			std::ranges::transform(scheme, scheme.begin(), ::towlower);
			return (scheme == L"http" || scheme == L"https" || scheme == L"ftp")
				&& !uri.Host().empty();
		}
		catch (...)
		{
			return false;
		}
	}

	// ------------------------------------------------------------------
	//  XAML event handlers
	// ------------------------------------------------------------------

	fire_and_forget HttpDownloadDialog::DialogRoot_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		(void)sender;
		(void)e;
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
				auto const text = co_await Core::Utils::Misc::getCurrentClipboardText();
				strong->SetProperty(strong->m_clipboardPreviewText, text, L"ClipboardPreviewText");
				strong->m_clipboardPreviewVisibility = text.empty() ? Visibility::Collapsed : Visibility::Visible;
				strong->RaisePropertyChanged(L"ClipboardPreviewVisibility");
			});

			auto clipboardContent = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();
			if (clipboardContent.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text()))
			{
				auto const text = co_await clipboardContent.GetTextAsync();
				SetProperty(m_clipboardPreviewText, text, L"ClipboardPreviewText");
				m_clipboardPreviewVisibility = text.empty() ? Visibility::Collapsed : Visibility::Visible;
				RaisePropertyChanged(L"ClipboardPreviewVisibility");
				if (ValidateUrl(text))
				{
					Url(text);
					co_await FetchMetadataAsync();
				}
			}
		}
		catch (...)
		{
			// Clipboard access may fail
		}
		if (m_isUrlValid && m_fileSizeText == L"Size: Unknown")
		{
			co_await FetchMetadataAsync();
		}
	}

	void HttpDownloadDialog::CaptureValues(bool const startPaused)
	{
		m_startPaused = startPaused;
	}

	void HttpDownloadDialog::UpdateDiskSpace()
	{
		if (m_saveDir.empty()) return;
		std::error_code error;
		auto const space = std::filesystem::space(std::filesystem::path{ m_saveDir.c_str() }, error);
		auto const text = error ? hstring{} : hstring{ std::format(L"Free: {:.1f} GiB", static_cast<double>(space.available) / 1073741824.0) };
		SetProperty(m_diskSpaceText, text, L"DiskSpaceText");
	}

	void HttpDownloadDialog::SetError(hstring const& title, hstring const& message)
	{
		SetProperty(m_errorTitle, title, L"ErrorTitle");
		SetProperty(m_errorMessage, message, L"ErrorMessage");
		if (!m_isErrorOpen)
		{
			m_isErrorOpen = true;
			RaisePropertyChanged(L"IsErrorOpen");
		}
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
		if (!ValidateUrl(m_url))
		{
			SetError(L"Invalid URL", L"Please enter a valid HTTP, HTTPS, or FTP URL.");
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
		options.MaximumDownloadRate = static_cast<std::uint64_t>(m_maximumDownloadRate) * 1024;
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
					self->SetError(L"Unable to add download", L"aria2 is unavailable or rejected the supplied options.");
				}
			}
		});
	}

	winrt::Windows::Foundation::IAsyncAction HttpDownloadDialog::FetchMetadataButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await FetchMetadataAsync();
	}

	winrt::Windows::Foundation::IAsyncAction HttpDownloadDialog::FetchMetadataAsync()
	{
		auto lifetime = get_strong();
		if (!ValidateUrl(m_url))
		{
			SetError(L"Invalid URL", L"Please enter a valid HTTP, HTTPS, or FTP URL.");
			co_return;
		}
		m_isMetadataLoading = true;
		RaisePropertyChanged(L"IsMetadataLoading");
		RaisePropertyChanged(L"CanFetchMetadata");
		try
		{
			winrt::Windows::Web::Http::HttpClient client;
			auto uri = winrt::Windows::Foundation::Uri{ m_url };
			if (uri.SchemeName() == L"ftp")
			{
				SetProperty(m_fileSizeText, hstring{ L"Size: Determined by aria2" }, L"FileSizeText");
				SetProperty(m_resumeSupportText, hstring{ L"Resume support: Determined by server" }, L"ResumeSupportText");
				if (m_fileName.empty())
				{
					auto const path = std::wstring_view{ uri.Path() };
					auto const slash = path.find_last_of(L'/');
					if (slash != std::wstring_view::npos && slash + 1 < path.size())
					{
						FileName(winrt::Windows::Foundation::Uri::UnescapeComponent(hstring{ path.substr(slash + 1) }));
					}
				}
				m_isMetadataLoading = false;
				RaisePropertyChanged(L"IsMetadataLoading");
				RaisePropertyChanged(L"CanFetchMetadata");
				co_return;
			}
			winrt::Windows::Web::Http::HttpRequestMessage request{ winrt::Windows::Web::Http::HttpMethod::Head(), uri };
			auto response = co_await client.SendRequestAsync(request, winrt::Windows::Web::Http::HttpCompletionOption::ResponseHeadersRead);
			if (!response.IsSuccessStatusCode() || !response.Content().Headers().ContentLength())
			{
				request = winrt::Windows::Web::Http::HttpRequestMessage{ winrt::Windows::Web::Http::HttpMethod::Get(), uri };
				request.Headers().TryAppendWithoutValidation(L"Range", L"bytes=0-0");
				response = co_await client.SendRequestAsync(request, winrt::Windows::Web::Http::HttpCompletionOption::ResponseHeadersRead);
			}
			if (!response.IsSuccessStatusCode()) throw winrt::hresult_error(E_FAIL, L"HTTP " + winrt::to_hstring(static_cast<int>(response.StatusCode())));

			std::uint64_t length{};
			if (auto contentLength = response.Content().Headers().ContentLength()) length = contentLength.Value();
			if (auto contentRange = response.Content().Headers().ContentRange())
			{
				if (auto rangeLength = contentRange.Length()) length = rangeLength.Value();
			}
			SetProperty(m_fileSizeText, length > 0 ? hstring{ std::format(L"Size: {:.2f} MiB", static_cast<double>(length) / 1048576.0) } : hstring{ L"Size: Unknown" }, L"FileSizeText");

			bool resumable = response.StatusCode() == winrt::Windows::Web::Http::HttpStatusCode::PartialContent;
			for (auto const& header : response.Headers())
			{
				if (header.Key() == L"Accept-Ranges" && std::wstring_view{ header.Value() }.find(L"bytes") != std::wstring_view::npos)
				{
					resumable = true;
					break;
				}
			}
			SetProperty(m_resumeSupportText, resumable ? hstring{ L"Resume support: Yes" } : hstring{ L"Resume support: No" }, L"ResumeSupportText");
			if (m_fileName.empty())
			{
				if (auto disposition = response.Content().Headers().ContentDisposition())
				{
					auto suggestedName = disposition.FileNameStar();
					if (suggestedName.empty()) suggestedName = disposition.FileName();
					if (!suggestedName.empty())
					{
						if (suggestedName.size() >= 2 && suggestedName.front() == L'\"' && suggestedName.back() == L'\"')
						{
							suggestedName = hstring{ std::wstring_view{ suggestedName }.substr(1, suggestedName.size() - 2) };
						}
						FileName(suggestedName);
					}
				}
				if (m_fileName.empty())
				{
					auto const path = response.RequestMessage().RequestUri().Path();
					auto const pathView = std::wstring_view{ path };
					auto const slash = pathView.find_last_of(L'/');
					if (slash != std::wstring_view::npos && slash + 1 < pathView.size())
					{
						FileName(winrt::Windows::Foundation::Uri::UnescapeComponent(hstring{ pathView.substr(slash + 1) }));
					}
				}
			}
			if (m_isErrorOpen)
			{
				m_isErrorOpen = false;
				RaisePropertyChanged(L"IsErrorOpen");
			}
		}
		catch (winrt::hresult_error const& error)
		{
			SetError(L"Unable to retrieve file information", error.message());
		}
		m_isMetadataLoading = false;
		RaisePropertyChanged(L"IsMetadataLoading");
		RaisePropertyChanged(L"CanFetchMetadata");
	}

	winrt::Windows::Foundation::IAsyncAction HttpDownloadDialog::PasteUrlButton_Click(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		try
		{
			auto clipboard = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();
			if (clipboard.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text()))
			{
				auto text = co_await clipboard.GetTextAsync();
				Url(text);
				co_await FetchMetadataAsync();
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
				SaveDirectory(folder.Path());
			}
		}
		catch (...)
		{
			// Picker may be cancelled
		}
	}
}
