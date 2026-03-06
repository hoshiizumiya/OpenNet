#pragma once

#include "App.xaml.g.h"
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include <winrt/OpenNet.UI.Shell.h>

namespace winrt::OpenNet::implementation
{
	struct App : AppT<App>
	{
		App();
		~App();

		void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

		static void HandleActivation(winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const&);

		static inline winrt::Microsoft::UI::Xaml::Window window{ nullptr };
		static inline winrt::OpenNet::UI::Shell::NotifyIconContextMenu trayIcon{ nullptr };
		// Set to true before calling Application::Exit() so the Closing handler
		// does not cancel the close and hide the window to tray.
		static inline bool s_isExiting{ false };

	private:
		void OnClosing(winrt::Microsoft::UI::Xaml::Window const& sender,
					   winrt::Microsoft::UI::Xaml::WindowEventArgs const& args);
		static winrt::fire_and_forget InitializeRSSManagerAsync();
	};
}
