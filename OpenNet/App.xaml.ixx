module;

#include "App.xaml.g.h"
#include "UI/Shell/NotifyIconXamlHostWindow.xaml.h"

export module OpenNet.App;

import winrt.OpenNet.UI.Xaml.View.Pages;
import winrt.WinUI3Package;
import winrt.Windows.ApplicationModel.Activation;
import winrt.Microsoft.Windows.AppLifecycle;

export namespace winrt::OpenNet::implementation
{
	struct App : AppT<App>
	{
		App();
		~App();

		void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
		void Exit();

		bool CreateSetMainWindow();
		static void HandleActivation(winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const&);

		static inline winrt::Microsoft::UI::Xaml::Window window{ nullptr };
		static inline winrt::com_ptr<winrt::OpenNet::UI::Shell::implementation::NotifyIconXamlHostWindow> trayIcon;
		// Set to true before calling Application::Exit() so the Closing handler
		// does not cancel the close and hide the window to tray.
		// TODO: replace with XamlApplicationLifetime.ixx
		static inline bool s_isExiting{ false };

	private:
		static winrt::fire_and_forget HandleCloseStrategyAsync();
		static void HideToTray();
		static winrt::fire_and_forget ReallyClose();
		static void ShutdownEngines();
		static winrt::fire_and_forget InitializeRSSManagerAsync();

		static inline bool s_isHandlingClose{ false };
		static inline bool s_enginesShutdown{ false };
	};
}
