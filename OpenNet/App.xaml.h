#pragma once

#include "App.xaml.g.h"
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.Windows.AppLifecycle.h>

namespace winrt::OpenNet::implementation
{
	struct App : AppT<App>
	{
		App();

		void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

		static void HandleActivation(winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const&);

		static inline winrt::Microsoft::UI::Xaml::Window window{ nullptr };
	};
}
