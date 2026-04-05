#pragma once

#include "Web/WebView2/WebView2Version.h"

namespace OpenNet::Core
{
	class AppRuntime
	{
	public:
		static winrt::hstring DeviceId()
		{
			return InitializeDeviceId();
		}
		static ::OpenNet::Web::WebView2::WebView2Version WebView2Version();
		static winrt::hstring GetDisplayName();

	private:
		static winrt::hstring InitializeDeviceId();
		static ::OpenNet::Web::WebView2::WebView2Version InitializeWebView2();
	};
}
