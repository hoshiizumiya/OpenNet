#pragma once

#include "UI/Shell/NotifyIconContextMenu.g.h"

import std;
import winrt.WinUI3Package;
import winrt.OpenNet.UI.Xaml.View.Windows;
import winrt.Windows.ApplicationModel.DataTransfer;
import winrt.Microsoft.UI.Xaml.Controls;
namespace winrt::OpenNet::UI::Shell::implementation
{
	struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu>
	{
	private:
		winrt::OpenNet::UI::Xaml::View::Windows::InfoOverlayWindow m_floatingWindow{ nullptr };
		winrt::Windows::ApplicationModel::DataTransfer::Clipboard::ContentChanged_revoker m_clipboardRevoker;
		std::vector<std::string> m_lastSuspendedTorrentIds;
		bool m_lastSuspendIncludedHttp{ false };
		bool m_loadingToggleSettings{ false };
		bool m_clipboardDialogOpen{ false };
		winrt::hstring m_lastCapturedClipboardUrl;

		void LoadToggleSettings();
		void SaveToggleSetting(wchar_t const* key, bool value);
		void SetClipboardCaptureEnabled(bool enabled);
		winrt::fire_and_forget HandleClipboardChangedAsync();
		winrt::Windows::Foundation::IAsyncAction RunBulkOperationAsync(winrt::hstring operation);
		winrt::Windows::Foundation::IAsyncAction OpenAddDialogAsync(winrt::hstring kind);
		void ApplyTransferLimit(bool download, int bytesPerSecond);
		void UpdateTransferLimitChecks(int downloadLimit, int uploadLimit);
	public:
		NotifyIconContextMenu() = default;
		~NotifyIconContextMenu();
		void InitializeComponent();

		// Tray icon control
		void ExitApplication();
		void HomeAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void ExitAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction StartDownloadingAll_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction StartUploadingAll_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction StopAllTasks_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction SuspendAllActiveTasks_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction ResumeLastSuspendedTasks_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void TransferLimit_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction OpenTorrentFile_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction AddTorrentFromUrl_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction HttpDownload_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncAction HttpBatchDownload_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void SchedulerToggle_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void CaptureClipboardToggle_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void TrayBalloonToggle_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void FloatingWindowToggle_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void FloatingWindowSettings_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void Options_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
	};
}

namespace winrt::OpenNet::UI::Shell::factory_implementation
{
	struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu, implementation::NotifyIconContextMenu>
	{
	};
}
