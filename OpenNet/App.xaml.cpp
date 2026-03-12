#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "UI/Shell/NotifyIconContextMenu.xaml.h"
#include "UI/Xaml/View/Dialog/CloseToTrayDialog.h"
#include "Helpers/WindowHelper.h"
#include "Helpers/ThemeHelper.h"
#include "Core/P2PManager.h"
#include "Core/DownloadManager.h"
#include "Core/RSS/RSSManager.h"
#include "Core/GeoIP/GeoIPManager.h"

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.Windows.Storage.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <sentry.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Windowing;
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

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
		UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
		{
			if (IsDebuggerPresent())
			{
				auto errorMessage = e.Message();
				OutputDebugStringW((L"UnhandledException: " + std::wstring(errorMessage.c_str()) + L"\r\n").c_str());
				__debugbreak();
			}
		});
#endif
	}

	/// <summary>
	/// Invoked when the application is launched.
	/// </summary>
	/// <param name="e">Details about the launch request and process.</param>
	void App::OnLaunched([[maybe_unused]] Microsoft::UI::Xaml::LaunchActivatedEventArgs const& e)
	{
		// Create main window
		window = make<MainWindow>();
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);

		// Apply saved theme to the window
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);

		// Create and show system tray icon (assign to static member, not a local)
		trayIcon = OpenNet::UI::Shell::NotifyIconContextMenu();
		trayIcon.Show();

		// Register window closing event - close strategy (hide to tray / ask / exit)
		window.AppWindow().Closing([](auto const&, winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
		{
			// If we are in an intentional exit, allow the window to close
			if (App::s_isExiting)
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
			catch (...) {}

			// First time: need to show dialog — cancel close and go async
			args.Cancel(true);
			App::s_isHandlingClose = true;
			HandleCloseStrategyAsync();
		});

		// Initialize RSS Manager early so feeds update in the background
		// regardless of whether the user navigates to the RSS page
		InitializeRSSManagerAsync();

		// Initialize GeoIP database (non-blocking, small CSV load)
		try
		{
			::OpenNet::Core::GeoIPManager::Instance().Initialize();
		}
		catch (...) { OutputDebugStringA("App: GeoIP init failed\n"); }

		window.Activate();

#if _DEBUG
		// Language Hot-Reload support
		auto supportedLanguages = Windows::Globalization::ApplicationLanguages::Languages();

		std::wstring debugOut;
		debugOut.reserve(256);
		debugOut += L"Supported languages:\n";
		for (auto const& lang : supportedLanguages)
		{
			debugOut += std::wstring(lang.c_str());
			debugOut += L"\n";
		}
		OutputDebugStringW(debugOut.c_str());
#endif
		// Handle initial activation arguments
		auto activationArgs = AppInstance::GetCurrent().GetActivatedEventArgs();
		HandleActivation(activationArgs);
	}

	// To do: Custom activation, Windows integration
	void App::HandleActivation(winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const& args)
	{
		// 检查窗口是否存在
		if (!window)
		{
			// 如果窗口不存在，创建新窗口
			// 这种情况发生在重新激活时窗口已关闭的情况
			window = make<MainWindow>();
			::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);

			// Apply theme to new window
			::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);
		}

		// 获取窗口句柄
		HWND hwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(window);
		if (!hwnd)
		{
			return;
		}

		// 如果窗口被最小化，恢复它
		if (IsIconic(hwnd))
		{
			ShowWindow(hwnd, SW_RESTORE);
		}

		// 显示并激活窗口
		window.AppWindow().Show();

		// 将窗口置于前台
		SetForegroundWindow(hwnd);

		// 确保窗口获得焦点
		SetFocus(hwnd);

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
			if (!s_enginesShutdown)
			{
				OutputDebugStringA("App: Warning - engines not yet shut down, doing emergency shutdown\n");
				ShutdownEngines();
			}

			sentry_close();
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
				catch (...) { OutputDebugStringA("App: Failed to save close preference\n"); }
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
		if (window)
		{
			window.AppWindow().Hide();
		}
		OutputDebugStringA("App: MainWindow hidden to tray\n");
	}

	winrt::fire_and_forget App::ReallyClose()
	{
		s_isExiting = true;

		// Capture dispatcher before leaving the UI thread
		auto dispatcher = window.DispatcherQueue();

		// Shut down all engines on a background thread to avoid STA assertions
		co_await winrt::resume_background();
		ShutdownEngines();

		// Return to UI thread to call Exit()
		dispatcher.TryEnqueue([]()
		{
			Microsoft::UI::Xaml::Application::Current().Exit();
		});
	}

	void App::ShutdownEngines()
	{
		if (s_enginesShutdown)
			return;
		s_enginesShutdown = true;

		OutputDebugStringA("App: Shutting down engines...\n");

		// Stop RSS background updates (lightweight, just signals thread + joins)
		try { ::OpenNet::Core::RSS::RSSManager::Instance().Stop(); }
		catch (...) { OutputDebugStringA("App: RSS shutdown error\n"); }

		// Shutdown P2PManager (torrent session uses abort() + proxy, non-blocking)
		try { ::OpenNet::Core::P2PManager::Instance().Shutdown(); }
		catch (...) { OutputDebugStringA("App: P2PManager shutdown error\n"); }

		// Shutdown DownloadManager (Aria2 RPC + process termination)
		try { ::OpenNet::Core::DownloadManager::Instance().Shutdown(); }
		catch (...) { OutputDebugStringA("App: DownloadManager shutdown error\n"); }

		OutputDebugStringA("App: Engine shutdown completed\n");
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
		}
	}

}
