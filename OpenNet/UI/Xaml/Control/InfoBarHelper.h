#pragma once

#include "UI/Xaml/Control/InfoBarHelper.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	struct InfoBarHelper : InfoBarHelperT<InfoBarHelper>
	{
		InfoBarHelper() = default;

		static winrt::Microsoft::UI::Xaml::DependencyProperty IsTextSelectionEnabledProperty();
		static bool GetIsTextSelectionEnabled(winrt::Microsoft::UI::Xaml::DependencyObject const& obj);
		static void SetIsTextSelectionEnabled(winrt::Microsoft::UI::Xaml::DependencyObject const& obj, bool enabled);

	private:
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_isTextSelectionEnabledProperty;
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::factory_implementation
{
	struct InfoBarHelper : InfoBarHelperT<InfoBarHelper, implementation::InfoBarHelper>
	{
	};
}
