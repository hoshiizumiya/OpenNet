#pragma once

import winrt.OpenNet.UI.Xaml.View;

#include "UI/Xaml/View/Windows/GuideWindow.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct GuideWindow : GuideWindowT<GuideWindow>
	{
		GuideWindow();
		void InitializeComponent();
		void OnGuideCompleted(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::Foundation::IInspectable const&);
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct GuideWindow : GuideWindowT<GuideWindow, implementation::GuideWindow>
	{
	};
}
