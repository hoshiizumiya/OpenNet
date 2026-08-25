#pragma once

#include "UI/Xaml/View/Dialog/HttpDownloadDialog.g.h"

import OpenNet.ViewModels.ObservableMixin;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct HttpDownloadDialog : HttpDownloadDialogT<HttpDownloadDialog>, ::OpenNet::ViewModels::ObservableMixin<HttpDownloadDialog>
	{
		using ::OpenNet::ViewModels::ObservableMixin<HttpDownloadDialog>::SetProperty;
		HttpDownloadDialog();
		void InitializeComponent();

		// IDL-exposed read-only properties
		winrt::hstring Url() const;
		void Url(winrt::hstring const& value);
		winrt::hstring SaveDirectory() const;
		void SaveDirectory(winrt::hstring const& value);
		winrt::hstring FileName() const;
		void FileName(winrt::hstring const& value);
		bool IsUrlValid() const;
		double ConnectionsPerServer() const;
		void ConnectionsPerServer(double value);
		double MaximumDownloadRate() const;
		void MaximumDownloadRate(double value);
		bool StartPaused() const;
		winrt::hstring Referer() const;
		void Referer(winrt::hstring const& value);
		winrt::hstring UserAgent() const;
		void UserAgent(winrt::hstring const& value);
		winrt::hstring Cookie() const;
		void Cookie(winrt::hstring const& value);
		winrt::hstring Headers() const;
		void Headers(winrt::hstring const& value);
		winrt::hstring Mirrors() const;
		void Mirrors(winrt::hstring const& value);
		winrt::hstring Checksum() const;
		void Checksum(winrt::hstring const& value);
		winrt::hstring Description() const;
		void Description(winrt::hstring const& value);
		winrt::hstring Username() const;
		void Username(winrt::hstring const& value);
		winrt::hstring Password() const;
		void Password(winrt::hstring const& value);
		winrt::hstring ClipboardPreviewText() const;
		winrt::Microsoft::UI::Xaml::Visibility ClipboardPreviewVisibility() const;
		winrt::hstring FileSizeText() const;
		winrt::hstring DiskSpaceText() const;
		winrt::hstring ResumeSupportText() const;
		bool IsMetadataLoading() const;
		bool CanFetchMetadata() const;
		winrt::hstring ErrorTitle() const;
		winrt::hstring ErrorMessage() const;
		bool IsErrorOpen() const;

		// XAML event handlers
		fire_and_forget DialogRoot_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void OnPrimaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args);
		void OnSecondaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction FetchMetadataButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction PasteUrlButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction BrowseDirButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
	private:
		bool ValidateUrl(winrt::hstring const& url) const;
		void CaptureValues(bool startPaused);
		void UpdateDiskSpace();
		winrt::Windows::Foundation::IAsyncAction FetchMetadataAsync();
		void SetError(winrt::hstring const& title, winrt::hstring const& message);
		winrt::fire_and_forget StartDownloadAsync(bool startPaused);
		static void AppendLines(std::vector<std::string>& destination, winrt::hstring const& text);

		winrt::hstring m_url;
		winrt::hstring m_saveDir;
		winrt::hstring m_fileName;
		bool m_isUrlValid{ false };
		int32_t m_connectionsPerServer{ 8 };
		int64_t m_maximumDownloadRate{};
		bool m_startPaused{};
		winrt::hstring m_referer;
		winrt::hstring m_userAgent;
		winrt::hstring m_cookie;
		winrt::hstring m_headers;
		winrt::hstring m_mirrors;
		winrt::hstring m_checksum;
		winrt::hstring m_description;
		winrt::hstring m_username;
		winrt::hstring m_password;
		winrt::hstring m_clipboardPreviewText;
		winrt::Microsoft::UI::Xaml::Visibility m_clipboardPreviewVisibility{ winrt::Microsoft::UI::Xaml::Visibility::Collapsed };
		winrt::hstring m_fileSizeText{ L"Size: Unknown" };
		winrt::hstring m_diskSpaceText;
		winrt::hstring m_resumeSupportText{ L"Resume support: Unknown" };
		bool m_isMetadataLoading{};
		winrt::hstring m_errorTitle{ L"Invalid URL" };
		winrt::hstring m_errorMessage{ L"Please enter a valid HTTP, HTTPS, or FTP URL." };
		bool m_isErrorOpen{};
		winrt::Windows::ApplicationModel::DataTransfer::Clipboard::ContentChanged_revoker m_event_revoker;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct HttpDownloadDialog : HttpDownloadDialogT<HttpDownloadDialog, implementation::HttpDownloadDialog>
	{
	};
}
