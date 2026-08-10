module;

#include "App.xaml.g.h"

export module OpenNet.App;

import std;
import winrt.OpenNet.UI.Shell;
import winrt.OpenNet.UI.Xaml.View.Pages;
import winrt.WinUI3Package;
import winrt.Microsoft.UI.Dispatching;
import winrt.Windows.ApplicationModel.Activation;
import winrt.Microsoft.Windows.AppLifecycle;

export namespace winrt::OpenNet::implementation
{
	struct AppActivationSnapshot
	{
		winrt::Microsoft::Windows::AppLifecycle::ExtendedActivationKind Kind{
			winrt::Microsoft::Windows::AppLifecycle::ExtendedActivationKind::Launch };
		winrt::hstring LaunchArguments;
		std::vector<winrt::hstring> TorrentPaths;
	};

	struct App : AppT<App>
	{
		App();
		~App();

		void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
		void Exit();
		static void RequestExit();
		static void CompleteFirstRun();

		static bool CreateSetMainWindow();
		static AppActivationSnapshot SnapshotActivation(
			winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const&);
		static void HandleActivation(AppActivationSnapshot const&);

		static inline winrt::Microsoft::UI::Xaml::Window window{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::Window guideWindow{ nullptr };
		static inline winrt::OpenNet::UI::Shell::NotifyIconXamlHostWindow trayIcon{ nullptr };
		// Set to true before calling Application::Exit() so the Closing handler
		// does not cancel the close and hide the window to tray.
		// TODO: replace with XamlApplicationLifetime.ixx
		static inline std::atomic_bool s_isExiting{ false };

	private:
		static winrt::fire_and_forget HandleCloseStrategyAsync();
		static void HideToTray();
		static winrt::fire_and_forget ReallyClose();
		static void ShutdownEngines();
		static winrt::fire_and_forget InitializeTorrentCoreAsync();
		static winrt::fire_and_forget InitializeRSSManagerAsync();
		static winrt::fire_and_forget InitializeWebUIAsync();
		static void StartIPFilterSubscriptionUpdates();
		static void StopIPFilterSubscriptionUpdates();
		static void EnsureMainWindow();
		static void StartMainExperience();

		static inline bool s_isHandlingClose{ false };
		static inline std::atomic_bool s_enginesShutdown{ false };
		static inline std::atomic_bool s_mainExperienceStarted{ false };
		static inline winrt::Microsoft::Windows::AppLifecycle::AppInstance
			s_appInstance{ nullptr };
		static inline winrt::event_token s_activatedToken{};
		static inline winrt::Microsoft::UI::Dispatching::DispatcherQueue
			s_uiDispatcher{ nullptr };
		static inline winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer
			s_ipFilterSubscriptionTimer{ nullptr };
	};
}
