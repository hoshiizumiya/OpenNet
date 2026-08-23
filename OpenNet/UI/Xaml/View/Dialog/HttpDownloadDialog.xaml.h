#pragma once

#include "UI/Xaml/View/Dialog/HttpDownloadDialog.g.h"

import OpenNet.ViewModels.ObservableMixin;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct HttpDownloadDialog : HttpDownloadDialogT<HttpDownloadDialog>, ::OpenNet::ViewModels::ObservableMixin<HttpDownloadDialog>
	{
		HttpDownloadDialog();

		// IDL-exposed read-only properties
		winrt::hstring Url() const;
		winrt::hstring SaveDirectory() const;
		winrt::hstring FileName() const;
		bool IsUrlValid() const;
		int32_t ConnectionsPerServer() const;
		int64_t MaximumDownloadRate() const;
		bool StartPaused() const;
		winrt::hstring Referer() const;
		winrt::hstring UserAgent() const;
		winrt::hstring Cookie() const;
		winrt::hstring Headers() const;
		winrt::hstring Mirrors() const;
		winrt::hstring Checksum() const;
		winrt::hstring Description() const;
		winrt::hstring Username() const;
		winrt::hstring Password() const;

		// XAML event handlers
		fire_and_forget DialogRoot_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void OnPrimaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args);
		void OnSecondaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender, winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction FetchMetadataButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction PasteUrlButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::Windows::Foundation::IAsyncAction BrowseDirButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void UrlBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);

	private:
		bool ValidateUrl(winrt::hstring const& url) const;
		void CaptureValues(bool startPaused);
		void UpdateDiskSpace();
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
		winrt::Windows::ApplicationModel::DataTransfer::Clipboard::ContentChanged_revoker m_event_revoker;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct HttpDownloadDialog : HttpDownloadDialogT<HttpDownloadDialog, implementation::HttpDownloadDialog>
	{
	};
}
