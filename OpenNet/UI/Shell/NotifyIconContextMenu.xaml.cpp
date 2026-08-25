#include "XamlWorkaround.h"
#include "NotifyIconContextMenu.xaml.h"
#if __has_include("UI/Shell/NotifyIconContextMenu.g.cpp")
#include "UI/Shell/NotifyIconContextMenu.g.cpp"
#endif

#include "NotifyIconXamlHostWindow.xaml.h"
#include "MainWindow.xaml.h"
#include "UI/Xaml/View/Windows/InfoOverlayWindow.xaml.h"
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.xaml.h"

import OpenNet.App;
import OpenNet.Core.DownloadManager;
import OpenNet.Core.P2PManager;
import OpenNet.Core.TorrentSettings;
import OpenNet.Helpers.WindowHelper;
import winrt.WinUI3Package;
import winrtplus_coroutine;
import winrt.Windows.ApplicationModel;
import winrt.Windows.ApplicationModel.DataTransfer;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Windows.Foundation;
import winrt.Microsoft.Windows.Storage;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Shell::implementation
{
	NotifyIconContextMenu::~NotifyIconContextMenu()
	{
		try
		{
			m_clipboardRevoker.revoke();
		}
		catch (...)
		{
		}
		if (m_floatingWindow)
		{
			try
			{
				m_floatingWindow.Close();
			}
			catch (...)
			{
			}
			m_floatingWindow = nullptr;
		}
	}

	void NotifyIconContextMenu::InitializeComponent()
	{
		NotifyIconContextMenuT::InitializeComponent();
		LoadToggleSettings();
	}

	void NotifyIconContextMenu::ExitApplication()
	{
		winrt::OpenNet::implementation::App::RequestExit();
	}

	void NotifyIconContextMenu::HomeAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::ShowMainWindow();
	}

	void NotifyIconContextMenu::ExitAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		try
		{
			Hide();
		}
		catch (...)
		{
		}
		DispatcherQueue().TryEnqueue([]()
		{
			winrt::OpenNet::implementation::App::RequestExit();
		});
	}

	void NotifyIconContextMenu::LoadToggleSettings()
	{
		m_loadingToggleSettings = true;
		try
		{
			auto values = winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().LocalSettings().Values();
			auto read = [&values](wchar_t const* key, bool fallback)
			{
				return values.HasKey(key)
					? unbox_value_or<bool>(values.Lookup(key), fallback)
					: fallback;
			};
			SchedulerToggle().IsChecked(read(L"Tray.EnableScheduler", false));
			CaptureClipboardToggle().IsChecked(
				read(L"Tray.CaptureClipboardUrl", true));
			TrayBalloonToggle().IsChecked(read(L"Tray.BalloonEnabled", true));
			FloatingWindowToggle().IsChecked(
				read(L"Tray.FloatingWindowEnabled", false));
		}
		catch (...)
		{
		}
		m_loadingToggleSettings = false;

		auto& torrentSettingsManager = ::OpenNet::Core::TorrentSettingsManager::Instance();
		torrentSettingsManager.Load();
		auto const torrentSettings = torrentSettingsManager.Get();
		UpdateTransferLimitChecks(torrentSettings.downloadRateLimit, torrentSettings.uploadRateLimit);

		SetClipboardCaptureEnabled(CaptureClipboardToggle().IsChecked());
		if (FloatingWindowToggle().IsChecked())
		{
			winrt::Windows::Foundation::IInspectable empty{ nullptr };
			RoutedEventArgs args;
			FloatingWindowToggle_Click(empty, args);
		}
	}

	void NotifyIconContextMenu::SaveToggleSetting(wchar_t const* key, bool value)
	{
		if (m_loadingToggleSettings)
		{
			return;
		}
		try
		{
			winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().LocalSettings().Values().Insert(key, box_value(value));
		}
		catch (...)
		{
		}
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::RunBulkOperationAsync(hstring operation)
	{
		auto strong = get_strong();
		try
		{
			Hide();
		}
		catch (...)
		{
		}

		co_await winrt::resume_background();
		auto& manager = ::OpenNet::Core::P2PManager::Instance();

		if (operation == L"start-download"
			|| operation == L"start-upload"
			|| operation == L"resume-suspended")
		{
			co_await manager.EnsureTorrentCoreInitializedAsync();
		}

		auto tasks = manager.GetAllTasks();
		auto core = manager.TorrentCore();
		auto& http = ::OpenNet::Core::DownloadManager::Instance();

		if (operation == L"start-download")
		{
			if (core)
			{
				for (auto const& task : tasks)
				{
					if (task.status != 3 && task.status != 4)
					{
						core->ResumeTorrent(task.taskId);
					}
				}
			}
			http.ResumeAllHttp();
		}
		else if (operation == L"start-upload")
		{
			if (core)
			{
				for (auto const& task : tasks)
				{
					if (task.status == 3)
					{
						core->ResumeTorrent(task.taskId);
					}
				}
			}
		}
		else if (operation == L"stop")
		{
			m_lastSuspendedTorrentIds.clear();
			m_lastSuspendIncludedHttp = false;
			if (core)
			{
				for (auto const& task : tasks)
				{
					core->PauseTorrent(task.taskId);
				}
			}
			http.PauseAllHttp();
		}
		else if (operation == L"suspend")
		{
			m_lastSuspendedTorrentIds.clear();
			if (core)
			{
				for (auto const& task : tasks)
				{
					if (task.status == 1)
					{
						m_lastSuspendedTorrentIds.push_back(task.taskId);
						core->PauseTorrent(task.taskId);
					}
				}
			}
			m_lastSuspendIncludedHttp = http.IsAria2Available();
			if (m_lastSuspendIncludedHttp)
			{
				http.PauseAllHttp();
			}
		}
		else if (operation == L"resume-suspended")
		{
			if (core)
			{
				for (auto const& taskId : m_lastSuspendedTorrentIds)
				{
					core->ResumeTorrent(taskId);
				}
			}
			if (m_lastSuspendIncludedHttp)
			{
				http.ResumeAllHttp();
			}
			m_lastSuspendedTorrentIds.clear();
			m_lastSuspendIncludedHttp = false;
		}
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::OpenAddDialogAsync(hstring kind)
	{
		auto strong = get_strong();
		try
		{
			Hide();
		}
		catch (...)
		{
		}

		if (!winrt::OpenNet::implementation::App::CreateSetMainWindow())
		{
			co_return;
		}

		auto mainWindow = winrt::OpenNet::implementation::App::window.try_as<winrt::OpenNet::MainWindow>();
		if (!mainWindow)
		{
			co_return;
		}
		auto implementation = winrt::get_self<winrt::OpenNet::implementation::MainWindow>(mainWindow);
		co_await implementation->ShowAddTaskDialogAsync(kind);
	}

	void NotifyIconContextMenu::ApplyTransferLimit(bool download, int bytesPerSecond)
	{
		auto& settingsManager =	::OpenNet::Core::TorrentSettingsManager::Instance();
		settingsManager.Load();
		auto settings = settingsManager.Get();
		if (download)
		{
			settings.downloadRateLimit = bytesPerSecond;
		}
		else
		{
			settings.uploadRateLimit = bytesPerSecond;
		}
		settingsManager.Set(settings);

		if (auto core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
		{
			core->ReloadSettings();
		}
		UpdateTransferLimitChecks(settings.downloadRateLimit, settings.uploadRateLimit);
	}

	void NotifyIconContextMenu::UpdateTransferLimitChecks(int downloadLimit, int uploadLimit)
	{
		DownloadLimitUnlimited().IsChecked(downloadLimit == 0);
		DownloadLimit1MiB().IsChecked(downloadLimit == 1048576);
		DownloadLimit5MiB().IsChecked(downloadLimit == 5242880);
		DownloadLimit10MiB().IsChecked(downloadLimit == 10485760);
		DownloadLimit50MiB().IsChecked(downloadLimit == 52428800);

		UploadLimitUnlimited().IsChecked(uploadLimit == 0);
		UploadLimit256KiB().IsChecked(uploadLimit == 262144);
		UploadLimit1MiB().IsChecked(uploadLimit == 1048576);
		UploadLimit5MiB().IsChecked(uploadLimit == 5242880);
		UploadLimit10MiB().IsChecked(uploadLimit == 10485760);
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::StartDownloadingAll_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await RunBulkOperationAsync(L"start-download");
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::StartUploadingAll_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await RunBulkOperationAsync(L"start-upload");
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::StopAllTasks_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await RunBulkOperationAsync(L"stop");
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::SuspendAllActiveTasks_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await RunBulkOperationAsync(L"suspend");
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::ResumeLastSuspendedTasks_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await RunBulkOperationAsync(L"resume-suspended");
	}

	void NotifyIconContextMenu::TransferLimit_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto item = sender.try_as<FrameworkElement>();
		if (!item)
		{
			return;
		}
		auto tag = unbox_value_or<hstring>(item.Tag(), L"");
		std::wstring value(tag);
		auto separator = value.find(L':');
		if (separator == std::wstring::npos)
		{
			return;
		}
		try
		{
			const bool download = value.substr(0, separator) == L"download";
			const int limit = std::stoi(value.substr(separator + 1));
			ApplyTransferLimit(download, std::max(0, limit));
			Hide();
		}
		catch (...)
		{
		}
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::OpenTorrentFile_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await OpenAddDialogAsync(L"file");
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::AddTorrentFromUrl_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await OpenAddDialogAsync(L"url");
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::HttpDownload_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await OpenAddDialogAsync(L"http");
	}

	winrt::Windows::Foundation::IAsyncAction NotifyIconContextMenu::HttpBatchDownload_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		co_await OpenAddDialogAsync(L"http-batch");
	}

	void NotifyIconContextMenu::SchedulerToggle_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		SaveToggleSetting(L"Tray.EnableScheduler", SchedulerToggle().IsChecked());
	}

	void NotifyIconContextMenu::SetClipboardCaptureEnabled(bool enabled)
	{
		try
		{
			m_clipboardRevoker.revoke();
		}
		catch (...)
		{
		}
		if (!enabled)
		{
			return;
		}
		auto weak = get_weak();
		m_clipboardRevoker = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::ContentChanged(winrt::auto_revoke, [weak](auto const&, auto const&)
		{
			if (auto self = weak.get())
			{
				self->HandleClipboardChangedAsync();
			}
		});
	}

	winrt::fire_and_forget NotifyIconContextMenu::HandleClipboardChangedAsync()
	{
		auto strong = get_strong();
		auto dispatcher = DispatcherQueue();
		co_await winrtplus::resume_foreground(dispatcher);
		if (m_clipboardDialogOpen || winrt::OpenNet::implementation::App::s_isExiting.load())
		{
			co_return;
		}

		try
		{
			auto content = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();
			if (!content.Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text()))
			{
				co_return;
			}
			winrt::hstring text;

			bool retrying = false;
			std::chrono::steady_clock::time_point deadline{};

			while (true)
			{
				if (retrying)
				{
					if (std::chrono::steady_clock::now() >= deadline)
					{
						break;
					}

					co_await winrt::resume_after(200ms);

					if (std::chrono::steady_clock::now() >= deadline)
					{
						break;
					}
				}

				try
				{
					text = co_await content.GetTextAsync();
					break;
				}
				catch (...)
				{
					if (!retrying)
					{
						retrying = true;
						deadline = std::chrono::steady_clock::now() + 10s;
					}
				}
			}

			std::wstring normalized{ text.c_str() };
			auto const first = normalized.find_first_not_of(L" \t\r\n");
			if (first == std::wstring::npos)
			{
				co_return;
			}
			auto const last = normalized.find_last_not_of(L" \t\r\n");
			normalized = normalized.substr(first, last - first + 1);
			text = winrt::hstring{ normalized };
			std::wstring lower(text);
			std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
			const bool torrent = lower.starts_with(L"magnet:");
			const bool http = lower.starts_with(L"http://") || lower.starts_with(L"https://") || lower.starts_with(L"ftp://");
			if ((!torrent && !http) || text == m_lastCapturedClipboardUrl)
			{
				co_return;
			}

			m_lastCapturedClipboardUrl = text;
			m_clipboardDialogOpen = true;
			if (torrent)
			{
				try
				{
					Hide();
				}
				catch (...)
				{
				}
				if (winrt::OpenNet::implementation::App::CreateSetMainWindow())
				{
					auto checkWindow = winrt::make_self<winrt::OpenNet::UI::Xaml::View::Windows::implementation::TorrentCheckModalWindow>(text);
					::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(*checkWindow);
					checkWindow->Activate();
				}
			}
			else
			{
				co_await OpenAddDialogAsync(L"http");
			}
			m_clipboardDialogOpen = false;
		}
		catch (...)
		{
			m_clipboardDialogOpen = false;
		}
	}

	void NotifyIconContextMenu::CaptureClipboardToggle_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_loadingToggleSettings)
		{
			return;
		}
		const bool enabled = CaptureClipboardToggle().IsChecked();
		SaveToggleSetting(L"Tray.CaptureClipboardUrl", enabled);
		SetClipboardCaptureEnabled(enabled);
	}

	void NotifyIconContextMenu::TrayBalloonToggle_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		SaveToggleSetting(L"Tray.BalloonEnabled", TrayBalloonToggle().IsChecked());
	}

	void NotifyIconContextMenu::FloatingWindowToggle_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_loadingToggleSettings)
		{
			return;
		}

		const bool enabled = FloatingWindowToggle().IsChecked();
		SaveToggleSetting(L"Tray.FloatingWindowEnabled", enabled);
		if (enabled && !m_floatingWindow)
		{
			auto floating =	winrt::OpenNet::UI::Xaml::View::Windows::InfoOverlayWindow();
			m_floatingWindow = floating;
			auto weak = get_weak();
			floating.Closed([weak](auto const&, auto const&)
			{
				if (auto self = weak.get())
				{
					self->m_floatingWindow = nullptr;
					self->FloatingWindowToggle().IsChecked(false);
					self->SaveToggleSetting(L"Tray.FloatingWindowEnabled", false);
				}
			});
			::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(floating);
			floating.Activate();
		}
		else if (!enabled && m_floatingWindow)
		{
			auto floating = m_floatingWindow;
			m_floatingWindow = nullptr;
			try
			{
				floating.Close();
			}
			catch (...)
			{
			}
		}
	}

	void NotifyIconContextMenu::FloatingWindowSettings_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
	{
		Options_Click(sender, args);
	}

	void NotifyIconContextMenu::Options_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		try
		{
			Hide();
		}
		catch (...)
		{
		}
		if (!winrt::OpenNet::implementation::App::CreateSetMainWindow())
		{
			return;
		}
		if (auto mainWindow = winrt::OpenNet::implementation::App::window.try_as<winrt::OpenNet::MainWindow>())
		{
			mainWindow.Navigate(L"settings");
		}
	}
}
