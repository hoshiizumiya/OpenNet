#pragma once
import winrt.Microsoft.UI.Xaml;

namespace OpenNet::UI
{
	// A single library control owns its bindings, input and both appearances.
	struct MaterialAppearance
	{
		static winrt::hstring Kind(winrt::Microsoft::UI::Xaml::FrameworkElement const& element);
		static bool IsGlass(winrt::Microsoft::UI::Xaml::FrameworkElement const& element);
		static void Track(winrt::Microsoft::UI::Xaml::FrameworkElement const& element);
		static void Release(winrt::Microsoft::UI::Xaml::FrameworkElement const& element);
	};
}
