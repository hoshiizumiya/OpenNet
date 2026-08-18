module;
#include "Core/WebUI/WebUIHost.h"
#include "XamlWorkaround.h"
#include "MainWindow.xaml.h"
#include "UI/Shell/NotifyIconXamlHostWindow.xaml.h"
#include "UI/Xaml/View/Dialog/CloseToTrayDialog.h"
#include "UI/Xaml/View/Windows/DevWindow.xaml.h"
#include "UI/Xaml/View/Windows/GuideWindow.xaml.h"
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.xaml.h"
#include "UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.xaml.h"
#include "UI/Xaml/Control/Effect/TextMorphEffect.h"

module OpenNet.App;

import OpenNet.Core.ExceptionService.ExceptionHandling;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.DownloadManager;
import OpenNet.Core.GeoIP.GeoIPManager;
import OpenNet.Core.P2PManager;
import OpenNet.Core.RSS.RSSManager;
import OpenNet.Core.Setting.LocalSetting;
import OpenNet.Core.Setting.SettingKeys;
import OpenNet.Core.Torrent.TrackerManager;
import OpenNet.Core.Utils.Message;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import OpenNet.Service.Notification.InfoBarService;
import OpenNet.ViewModels.Guide.GuideState;
import winrt.Windows.ApplicationModel.Activation;
import winrt.Windows.Storage;
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

		// App is constructed by Application::Start on the XAML thread, so this is
		// the first reliable place to capture the UI dispatcher for redirected
		// singleton activations.
		s_uiDispatcher = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
		s_appInstance = AppInstance::GetCurrent();
		s_activatedToken = s_appInstance.Activated([](auto const&, AppActivationArguments const& args)
		{
			try
			{
				// AppActivationArguments::Data can be a proxy owned by the redirecting
				// process.  Read it while the Activated callback is still active and
				// enqueue only apartment-neutral strings.
				auto snapshot = SnapshotActivation(args);
				if (s_uiDispatcher)
				{
					s_uiDispatcher.TryEnqueue(
						[snapshot = std::move(snapshot)]() noexcept
					{
						try
						{
							HandleActivation(snapshot);
						}
						catch (...)
						{
							// Exceptions escaping a DispatcherQueueHandler cause XAML to
							// fail fast. Activation failures are reported by the handler.
						}
					});
				}
			}
			catch (...)
			{
				// The remote activation broker may already be unavailable. Never let
				// that failure unwind through the AppInstance event callback.
			}
		});
	}

	/// <summary>
	/// Invoked when the application is launched.
	/// </summary>
	/// <param name="e">Details about the launch request and process.</param>
	void App::OnLaunched([[maybe_unused]] Microsoft::UI::Xaml::LaunchActivatedEventArgs const& e)
	{
		using ::OpenNet::Core::Setting::LocalSetting;
		using namespace ::OpenNet::Core::Setting;
		auto const guideState = LocalSetting::Get(
			SettingKeys::GuideState,
			::OpenNet::ViewModels::Guide::GuideState::Language);
		if (guideState < ::OpenNet::ViewModels::Guide::GuideState::Completed)
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
		::OpenNet::Core::Setting::LocalSetting::Set(
			::OpenNet::Core::Setting::SettingKeys::GuideState,
			::OpenNet::ViewModels::Guide::GuideState::Completed);
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		database.SetBool("webui_host", "initialized", true);
		StartMainExperience();
		guideWindow = nullptr;
	}

	void App::EnsureMainWindow()
	{
		if (window)
		{
			return;
		}

		window = make<MainWindow>();
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);

		window.AppWindow().Closing([](
			auto const&,
			winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
		{
			if (App::s_isExiting.load())
			{
				return;
			}

			if (!App::trayIcon)
			{
				return;
			}

			if (App::s_isHandlingClose)
			{
				args.Cancel(true);
				return;
			}

			try
			{
				auto values = winrt::Microsoft::Windows::Storage::
					ApplicationData::GetDefault().LocalSettings().Values();
				if (values.HasKey(L"Hide2TrayWhenCloseAsked")
					&& unbox_value<bool>(
						values.Lookup(L"Hide2TrayWhenCloseAsked")))
				{
					bool hide = values.HasKey(L"Hide2TrayWhenClose")
						&& unbox_value<bool>(
							values.Lookup(L"Hide2TrayWhenClose"));
					args.Cancel(true);
					if (hide)
					{
						HideToTray();
					}
					else
					{
						ReallyClose();
					}
					return;
				}
			}
			catch (...)
			{
			}

			args.Cancel(true);
			App::s_isHandlingClose = true;
			HandleCloseStrategyAsync();
		});
	}

	void App::StartMainExperience()
	{
		EnsureMainWindow();
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
		StartIPFilterSubscriptionUpdates();

#if _DEBUG
		auto devWindow = winrt::make<
			winrt::OpenNet::UI::Xaml::View::Windows::implementation::DevWindow>();
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(devWindow);
		devWindow.Activate();
#endif

		try
		{
			HandleActivation(SnapshotActivation(
				AppInstance::GetCurrent().GetActivatedEventArgs()));
		}
		catch (...)
		{
		}
	}

	void App::StartIPFilterSubscriptionUpdates()
	{
		using SubscriptionPage = winrt::OpenNet::UI::Xaml::View::Pages::
			SettingsPages::implementation::IPFilterSettingsPage;
		(void)SubscriptionPage::RunSubscriptionUpdateAsync(false, true);

		if (s_ipFilterSubscriptionTimer)
			return;
		auto dispatcher =
			Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
		if (!dispatcher)
			return;
		s_ipFilterSubscriptionTimer = dispatcher.CreateTimer();
		s_ipFilterSubscriptionTimer.IsRepeating(true);
		s_ipFilterSubscriptionTimer.Interval(std::chrono::minutes(15));
		s_ipFilterSubscriptionTimer.Tick([](auto const&, auto const&)
		{
			(void)SubscriptionPage::RunSubscriptionUpdateAsync(false, true);
		});
		s_ipFilterSubscriptionTimer.Start();
	}

	void App::StopIPFilterSubscriptionUpdates()
	{
		if (!s_ipFilterSubscriptionTimer)
			return;
		s_ipFilterSubscriptionTimer.Stop();
		s_ipFilterSubscriptionTimer = nullptr;
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
			EnsureMainWindow();
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

	/// <summary>
	AppActivationSnapshot App::SnapshotActivation(
		winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const& args)
	{
		AppActivationSnapshot snapshot;
		snapshot.Kind = args.Kind();
		auto data = args.Data();

		if (snapshot.Kind == ExtendedActivationKind::Launch)
		{
			if (auto launchArgs = data.try_as<
				winrt::Windows::ApplicationModel::Activation::ILaunchActivatedEventArgs>())
			{
				snapshot.LaunchArguments = launchArgs.Arguments();
			}
		}
		else if (snapshot.Kind == ExtendedActivationKind::File)
		{
			if (auto fileArgs = data.try_as<
				winrt::Windows::ApplicationModel::Activation::IFileActivatedEventArgs>())
			{
				for (auto const& file : fileArgs.Files())
				{
					try
					{
						auto storageFile = file.try_as<winrt::Windows::Storage::StorageFile>();
						if (!storageFile) continue;
						std::wstring name{ storageFile.Name().c_str() };
						std::transform(name.begin(), name.end(), name.begin(), std::towlower);
						if (name.ends_with(L".torrent"))
						{
							snapshot.TorrentPaths.emplace_back(storageFile.Path());
						}
					}
					catch (...)
					{
					}
				}
			}
		}

		return snapshot;
	}

	/// <summary>
	/// Handles an apartment-neutral snapshot of the activation request.
	/// </summary>
	void App::HandleActivation(AppActivationSnapshot const& args)
	{
		CreateSetMainWindow();
		HWND hwnd = window
			? ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(window)
			: nullptr;

		// 根据激活类型处理不同的激活参数
		// Handle different activation kinds based on the activation arguments
		ExtendedActivationKind kind = args.Kind;

		if (kind == ExtendedActivationKind::Launch)
		{
			// Command-line arguments are already copied into args.LaunchArguments.
		}
		else if (kind == ExtendedActivationKind::AppNotification)
		{
		}
		else if (kind == ExtendedActivationKind::File)
		{
			// File activation (e.g., when a user opens a file associated with the app)
			// https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.windows.applifecycle.appactivationarguments.data
			for (auto const& path : args.TorrentPaths)
			{
				try
				{
					auto checkWindow = winrt::make_self<winrt::OpenNet::UI::Xaml::View::Windows::implementation::TorrentCheckModalWindow>(path);
					::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(*checkWindow);
					checkWindow->Activate();
				}
				catch (winrt::hresult_error const& error)
				{
					::OpenNet::Service::Notification::InfoBarService::Instance().Show(
						L"Torrent file", error.message(),
						Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error, 0);
				}
			}

			// File activation when flashing notification
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
			if (s_appInstance && s_activatedToken.value)
			{
				s_appInstance.Activated(s_activatedToken);
				s_activatedToken = {};
			}
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
		StopIPFilterSubscriptionUpdates();

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
			co_await ::OpenNet::Core::Torrent::TrackerManager::Instance()
				.InitializeAsync();
			co_await ::OpenNet::Core::P2PManager::Instance()
				.EnsureTorrentCoreInitializedAsync();
			OutputDebugStringA("App: libtorrent core initialized\n");
			::OpenNet::Service::Notification::InfoBarService::Instance().Show(
				ResourceGetString(L"AppBitTorrentEngine"),
				ResourceGetString(L"AppLibtorrentInitialized"),
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success,
				3500);
		}
		catch (std::exception const& exception)
		{
			OutputDebugStringA((
				"App: Failed to initialize libtorrent core: "
				+ std::string(exception.what()) + "\n").c_str());
			::OpenNet::Service::Notification::InfoBarService::Instance().Show(
				ResourceGetString(L"AppBitTorrentEngineFailed"),
				to_hstring(exception.what()),
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error,
				0);
		}
		catch (...)
		{
			OutputDebugStringA("App: Failed to initialize libtorrent core\n");
			::OpenNet::Service::Notification::InfoBarService::Instance().Show(
				ResourceGetString(L"AppBitTorrentEngineFailed"),
				ResourceGetString(L"AppLibtorrentInitializationUnknownError"),
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
			::OpenNet::Service::Notification::InfoBarService::Instance().Show(
				ResourceGetString(L"AppRSSServiceFailed"),
				ResourceGetString(L"AppRSSBackgroundUpdatesFailed"),
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
					::OpenNet::Service::Notification::InfoBarService::Instance().Show(
							ResourceGetString(L"AppWebUIFailed"),
							ResourceGetString(L"AppWebUIStartFailed"),
							Microsoft::UI::Xaml::Controls::
							InfoBarSeverity::Error,
							0);
				});
			}
			else
			{
				dispatcher.TryEnqueue([]
				{
					::OpenNet::Service::Notification::InfoBarService::Instance().Show(
							ResourceGetString(L"AppWebUI"),
							ResourceGetString(L"AppWebUIRunning"),
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
			::OpenNet::Service::Notification::InfoBarService::Instance().Show(
				ResourceGetString(L"AppWebUIFailed"),
				to_hstring(exception.what()),
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error,
				0);
		}
		catch (...)
		{
			OutputDebugStringA("App: Failed to initialize WebUI\n");
			::OpenNet::Service::Notification::InfoBarService::Instance().Show(
				ResourceGetString(L"AppWebUIFailed"),
				ResourceGetString(L"AppWebUIUnknownStartError"),
				Microsoft::UI::Xaml::Controls::InfoBarSeverity::Error,
				0);
		}
	}

}
