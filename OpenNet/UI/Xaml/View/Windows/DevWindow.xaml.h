#pragma once
import winrt.WinUI3Package;

#include "UI/Xaml/View/Windows/DevWindow.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct DevWindow : DevWindowT<DevWindow>
	{
		DevWindow();

		void TriggerXamlException_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		winrt::fire_and_forget OpenOperationProgressDialog_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct DevWindow : DevWindowT<DevWindow, implementation::DevWindow>
	{
	};
}
