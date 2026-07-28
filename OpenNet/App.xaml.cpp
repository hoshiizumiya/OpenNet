module;
#include "Core/WebUI/WebUIHost.h"
#include "XamlWorkaround.h"
#include "MainWindow.xaml.h"
#include "UI/Shell/NotifyIconXamlHostWindow.xaml.h"
#include "UI/Xaml/View/Dialog/CloseToTrayDialog.h"
#include "UI/Xaml/View/Windows/DevWindow.xaml.h"
#include "UI/Xaml/View/Windows/GuideWindow.xaml.h"
#include "UI/Xaml/View/InfoBarView.xaml.h"
#include "UI/Xaml/Control/Effect/TextMorphEffect.h"

module OpenNet.App;

import OpenNet.Core.ExceptionService.ExceptionHandling;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.DownloadManager;
import OpenNet.Core.GeoIP.GeoIPManager;
import OpenNet.Core.P2PManager;
import OpenNet.Core.RSS.RSSManager;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import winrt.Windows.ApplicationModel.Activation;
import winrt.Microsoft.Windows.Storage;
import winrt.Microsoft.UI.Xaml.Controls;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Windowing;
using namespace winrt::Microsoft::Windows::AppLifecycle;

namespace winrt::OpenNet::implementation
{
	/// <summary>
	/// Initializes the singleton application object.  This is the first line of authored code
	/// executed, and as such is the logical equivalent of main() or WinMain().
	/// </summary>
	App::App()
	{
		// Initialize theme system early
		::OpenNet::Helpers::ThemeHelper::Initialize();

		// Xaml objects should not call InitializeComponent during construction.
		// See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent

		// Register unified exception handler
		UnhandledException(::OpenNet::Core::ExceptionService::ExceptionHandling::OnAppUnhandledException);
	}

	/// <summary>
	/// Invoked when the application is launched.
	/// </summary>
	/// <param name="e">Details about the launch request and process.</param>
	void App::OnLaunched([[maybe_unused]] Microsoft::UI::Xaml::LaunchActivatedEventArgs const& e)
	{
		// Create main window
		window = make<MainWindow>();
		::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(window);
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);

		// Apply saved theme to the window
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);

		// Register window closing event - close strategy (hide to tray / ask / exit)
		window.AppWindow().Closing([](auto const&, winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
		{
			// If we are in an intentional exit, allow the window to close
			if (App::s_isExiting.load())
				return;

			// If no tray icon was created, allow direct close
			if (!App::trayIcon)
				return;

			// Prevent re-entrance: if already showing close dialog, just keep it cancelled
			if (App::s_isHandlingClose)
			{
				args.Cancel(true);
				return;
			}

			// Check LocalSettings for a saved preference
			try
			{
				auto values = winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().LocalSettings().Values();
				if (values.HasKey(L"Hide2TrayWhenCloseAsked"))
				{
					bool asked = unbox_value<bool>(values.Lookup(L"Hide2TrayWhenCloseAsked"));
					if (asked)
					{
						bool hide = false;
						if (values.HasKey(L"Hide2TrayWhenClose"))
							hide = unbox_value<bool>(values.Lookup(L"Hide2TrayWhenClose"));

						if (hide)
						{
							// Synchronous: just hide and cancel
							args.Cancel(true);
							HideToTray();
							return;
						}
						else
						{
							// User chose to exit — cancel close and go async
							args.Cancel(true);
							ReallyClose();
							return;
						}
					}
				}
			}
			catch (...)
			{
			}

			// First time: need to show dialog — cancel close and go async
			args.Cancel(true);
			App::s_isHandlingClose = true;
			HandleCloseStrategyAsync();
		});


		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		if (!database.GetBool("webui_host", "initialized").value_or(false))
		{
			guideWindow = winrt::make<
				winrt::OpenNet::UI::Xaml::View::Windows::implementation::GuideWindow>();
			::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(
				guideWindow);
			guideWindow.Activate();
			return;
		}

		StartMainExperience();
	}

	void App::Exit()
	{
		RequestExit();
	}

	void App::RequestExit()
	{
		ReallyClose();
	}

	void App::CompleteFirstRun()
	{
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		database.SetBool("webui_host", "initialized", true);
		StartMainExperience();
		guideWindow = nullptr;
	}

	void App::StartMainExperience()
	{
		if (s_mainExperienceStarted.exchange(true))
		{
			CreateSetMainWindow();
			return;
		}

		if (!trayIcon)
		{
			trayIcon = OpenNet::UI::Shell::NotifyIconXamlHostWindow();
			trayIcon.Show();
		}

		window.Activate();
		::OpenNet::Core::GeoIPManager::Instance().Initialize();
		InitializeTorrentCoreAsync();
		InitializeRSSManagerAsync();
		InitializeWebUIAsync();

#if _DEBUG
		auto devWindow = winrt::make<
			winrt::OpenNet::UI::Xaml::View::Windows::implementation::DevWindow>();
		devWindow.Activate();
#endif

		HandleActivation(AppInstance::GetCurrent().GetActivatedEventArgs());
	}

	bool App::CreateSetMainWindow()
	{
		if (s_isExiting.load())
		{
			return false;
		}

		// 检查窗口是否存在
		if (!window)
		{
			// 如果窗口不存在，创建新窗口
			// 这种情况发生在重新激活时窗口已关闭的情况
			window = make<MainWindow>();
			::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(window);
			::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);

			// Apply theme to new window
			::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);
		}

		// 获取窗口句柄
		HWND hwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(window);
		if (!hwnd)
		{
			return false;
		}

		// 如果窗口被最小化，恢复它
		if (IsIconic(hwnd))
		{
			ShowWindow(hwnd, SW_RESTORE);
		}

		// 显示并激活窗口
		try
		{
			window.AppWindow().Show();
		}
		catch (...)
		{
			return false;
		}

		// 将窗口置于前台
		SetForegroundWindow(hwnd);

		// 确保窗口获得焦点
		SetFocus(hwnd);
		return true;
	}

	void App::HandleActivation(winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const& args)
	{
		CreateSetMainWindow();
		HWND hwnd = window
			? ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(window)
			: nullptr;

		// 根据激活类型处理不同的激活参数
		ExtendedActivationKind kind = args.Kind();

		if (kind == ExtendedActivationKind::Launch)
		{
			// 处理启动激活（包括命令行参数）
			auto launchArgs = args.Data().try_as<winrt::Windows::ApplicationModel::Activation::ILaunchActivatedEventArgs>();
			if (launchArgs)
			{
				// 在这里处理命令行参数
				// auto cmdLineArgs = launchArgs.Arguments();
				// OutputDebugStringW((L"Launch args: " + std::wstring(cmdLineArgs.c_str()) + L"\n").c_str());
			}
		}
		else if (kind == ExtendedActivationKind::AppNotification)
		{
		}
		else if (kind == ExtendedActivationKind::File)
		{
			// 处理文件激活
			auto fileArgs = args.Data().try_as<winrt::Windows::ApplicationModel::Activation::IFileActivatedEventArgs>();
			if (fileArgs)
			{
				// 可以在这里处理打开的文件
				// auto files = fileArgs.Files();
				// for (auto const& file : files)
				// {
				//     auto storageFile = file.try_as<winrt::Windows::Storage::IStorageFile>();
				//     // 处理文件
				// }
			}

			// 文件激活时闪烁提示
			FLASHWINFO fw = {};
			fw.cbSize = sizeof(FLASHWINFO);
			fw.hwnd = hwnd;
			fw.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
			fw.uCount = 3;
			fw.dwTimeout = 0;
			FlashWindowEx(&fw);
		}
		// 可以添加更多激活类型的处理
	}

	App::~App()
	{
		try
		{
			OutputDebugStringA("App: Destructor called, cleaning up...\n");

			// Remove tray icon (UI operation, OK on UI thread)
			if (trayIcon)
			{
				trayIcon.Remove();
				trayIcon = nullptr;
			}

			// Engines should already be shut down by ShutdownEngines().
			// Defensive: if somehow not, do a quick stop of RSS (lightweight).
			if (!s_enginesShutdown.load())
			{
				OutputDebugStringA("App: Warning - engines not yet shut down, doing emergency shutdown\n");
				ShutdownEngines();
			}

			OutputDebugStringA("App: Destructor completed\n");
		}
		catch (...)
		{
			OutputDebugStringA("App: Error in destructor\n");
		}
	}

	winrt::fire_and_forget App::HandleCloseStrategyAsync()
	{
		try
		{
			// Show dialog to ask user what to do
			if (!window)
			{
				ReallyClose();
				s_isHandlingClose = false;
				co_return;
			}

			auto content = window.Content();
			if (!content)
			{
				ReallyClose();
				s_isHandlingClose = false;
				co_return;
			}

			auto xamlRoot = content.XamlRoot();
			if (!xamlRoot)
			{
				ReallyClose();
				s_isHandlingClose = false;
				co_return;
			}

			auto dlg = winrt::OpenNet::UI::Xaml::View::Dialog::CloseToTrayDialog();
			dlg.XamlRoot(xamlRoot);

			auto result = co_await dlg.ShowAsync();

			// Save preference if "remember" was checked
			if (dlg.RememberChoice())
			{
				try
				{
					auto values = winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().LocalSettings().Values();
					values.Insert(L"Hide2TrayWhenCloseAsked", box_value(true));
					values.Insert(L"Hide2TrayWhenClose", box_value(result == winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary));
				}
				catch (...)
				{
					OutputDebugStringA("App: Failed to save close preference\n");
				}
			}

			if (result == winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
			{
				HideToTray();
			}
			else
			{
				ReallyClose();
			}
		}
		catch (...)
		{
			OutputDebugStringA("App: HandleCloseStrategyAsync error, falling back to exit\n");
			ReallyClose();
		}

		s_isHandlingClose = false;
	}

	void App::HideToTray()
	{
		if (window && !s_isExiting.load())
		{
			try
			{
				window.AppWindow().Hide();
			}
			catch (...)
			{
			}
		}
		OutputDebugStringA("App: MainWindow hidden to tray\n");
	}

	winrt::fire_and_forget App::ReallyClose()
	{
		// All exit sources (main-window close, tray Exit, App::Exit) converge
		// here. Only the first one may tear down XAML and background engines.
		if (s_isExiting.exchange(true))
		{
			co_return;
		}

		auto dispatcher = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

		// Remove the native notification icon before engine shutdown. This also
		// prevents late tray callbacks from trying to show an AppWindow which is
		// already closing.
		try
		{
			if (trayIcon)
			{
				trayIcon.Remove();
				trayIcon.Close();
				trayIcon = nullptr;
			}
		}
		catch (...)
		{
			trayIcon = nullptr;
		}

		// Shut down all engines on a background thread to avoid STA assertions
		co_await winrt::resume_background();
		ShutdownEngines();

		// Return to UI thread to call Exit()
		if (dispatcher)
		{
			dispatcher.TryEnqueue([]()
			{
				Microsoft::UI::Xaml::Application::Current().Exit();
			});
		}
	}

	void App::ShutdownEngines()
	{
		if (s_enginesShutdown.exchange(true))
			return;

		OutputDebugStringA("App: Shutting down engines...\n");

		// Stop RSS background updates (lightweight, just signals thread + joins)
		try
		{
			::OpenNet::Core::RSS::RSSManager::Instance().Stop();
		}
		catch (...)
		{
			OutputDebugStringA("App: RSS shutdown error\n");
		}

		try
		{
			::OpenNet::Core::WebUI::WebUIHost::Instance().Stop();
		}
		catch (...)
		{
			OutputDebugStringA("App: WebUI shutdown error\n");
		}

		// Shutdown P2PManager (torrent session uses abort() + proxy, non-blocking)
		try
		{
			::OpenNet::Core::P2PManager::Instance().Shutdown();
		}
		catch (...)
		{
			OutputDebugStringA("App: P2PManager shutdown error\n");
		}

		// Shutdown DownloadManager (Aria2 RPC + process termination)
		try
		{
			::OpenNet::Core::DownloadManager::Instance().Shutdown();
		}
		catch (...)
		{
			OutputDebugStringA("App: DownloadManager shutdown error\n");
		}

		OutputDebugStringA("App: Engine shutdown completed\n");
	}

	winrt::fire_and_forget App::InitializeTorrentCoreAsync()
	{
		try
		{
			co_await ::OpenNet::Core::P2PManager::Instance()
				.EnsureTorrentCoreInitializedAsync();
			OutputDebugStringA("App: libtorrent core initialized\n");
			winrt::OpenNet::UI::Xaml::View::implementation::InfoBarView::Show(
				L"BitTorrent engine",
				L"libtorrent initialized successfully.",
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success,
				3500);
		}
		catch (std::exception const& exception)
		{
			OutputDebugStringA((
				"App: Failed to initialize libtorrent core: "
				+ std::string(exception.what()) + "\n").c_str());
			winrt::OpenNet::UI::Xaml::View::implementation::InfoBarView::Show(
				L"BitTorrent engine failed",
				to_hstring(exception.what()),
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error,
				0);
		}
		catch (...)
		{
			OutputDebugStringA("App: Failed to initialize libtorrent core\n");
			winrt::OpenNet::UI::Xaml::View::implementation::InfoBarView::Show(
				L"BitTorrent engine failed",
				L"An unknown error occurred while initializing libtorrent.",
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error,
				0);
		}
	}

	winrt::fire_and_forget App::InitializeRSSManagerAsync()
	{
		try
		{
			auto& manager = ::OpenNet::Core::RSS::RSSManager::Instance();
			co_await manager.InitializeAsync();
			manager.Start();
			OutputDebugStringA("App: RSS Manager initialized and started\n");
		}
		catch (...)
		{
			OutputDebugStringA("App: Failed to initialize RSS Manager\n");
			winrt::OpenNet::UI::Xaml::View::implementation::InfoBarView::Show(
				L"RSS service failed",
				L"RSS background updates could not be started.",
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning,
				0);
		}
	}

	winrt::fire_and_forget App::InitializeWebUIAsync()
	{
		try
		{
			auto dispatcher =
				Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
			co_await ::OpenNet::Core::P2PManager::Instance()
				.EnsureTorrentCoreInitializedAsync();
			co_await winrt::resume_background();
			::OpenNet::Core::WebUI::WebUIOptions options;
			options.shutdownCallback = [dispatcher]
			{
				dispatcher.TryEnqueue([]
				{
					App::RequestExit();
				});
			};
			if (!::OpenNet::Core::WebUI::WebUIHost::Instance().Start(
				std::move(options)))
			{
				OutputDebugStringA("App: Failed to start WebUI Host\n");
				dispatcher.TryEnqueue([]
				{
					winrt::OpenNet::UI::Xaml::View::implementation::
						InfoBarView::Show(
							L"Web UI failed",
							L"The local qBittorrent-compatible Web UI could not be started.",
							Microsoft::UI::Xaml::Controls::
							InfoBarSeverity::Error,
							0);
				});
			}
			else
			{
				dispatcher.TryEnqueue([]
				{
					winrt::OpenNet::UI::Xaml::View::implementation::
						InfoBarView::Show(
							L"Web UI",
							L"The qBittorrent-compatible Web UI is running.",
							Microsoft::UI::Xaml::Controls::
							InfoBarSeverity::Success,
							4500);
				});
			}
		}
		catch (std::exception const& exception)
		{
			OutputDebugStringA((
				"App: Failed to initialize WebUI: "
				+ std::string(exception.what()) + "\n").c_str());
			winrt::OpenNet::UI::Xaml::View::implementation::InfoBarView::Show(
				L"Web UI failed",
				to_hstring(exception.what()),
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error,
				0);
		}
		catch (...)
		{
			OutputDebugStringA("App: Failed to initialize WebUI\n");
			winrt::OpenNet::UI::Xaml::View::implementation::InfoBarView::Show(
				L"Web UI failed",
				L"An unknown error occurred while starting the Web UI.",
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error,
				0);
		}
	}

}
