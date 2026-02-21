#pragma once

#include "App.xaml.g.h"
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.Windows.AppLifecycle.h>
#include "UI\Xaml\Media\Backdrop\InputActiveDesktopAcrylicBackdrop.h"

namespace winrt::OpenNet::implementation
{
	struct App : AppT<App>
	{
		App();
		~App();

		void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

		static void HandleActivation(winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const&);

		static inline winrt::Microsoft::UI::Xaml::Window window{ nullptr };

	private:
		void OnClosing(winrt::Microsoft::UI::Xaml::Window const& sender,
					   winrt::Microsoft::UI::Xaml::WindowEventArgs const& args);
	};
}
