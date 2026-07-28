#pragma once

#include "UI/Xaml/View/Windows/InfoOverlayWindow.g.h"

import winrt.Microsoft.UI.Dispatching;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct InfoOverlayWindow : InfoOverlayWindowT<InfoOverlayWindow>
	{
		InfoOverlayWindow();
		void OverlayRoot_Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		void RefreshStatistics();
		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct InfoOverlayWindow : InfoOverlayWindowT<InfoOverlayWindow, implementation::InfoOverlayWindow>
	{
	};
}
