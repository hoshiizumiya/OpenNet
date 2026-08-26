#pragma once

#include "UI/Xaml/Control/Effect/AnimatedNumber.g.h"

import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Media.Animation;

namespace winrt::OpenNet::UI::Xaml::Control::Effect::implementation
{
	struct AnimatedNumber : AnimatedNumberT<AnimatedNumber>
	{
		AnimatedNumber() = default;
		winrt::hstring Value() const;
		void Value(winrt::hstring const& value);
		void OnApplyTemplate();

	private:
		void UpdateCharacters();
		winrt::Microsoft::UI::Xaml::Controls::Panel m_rootPanel{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::TextBlock m_staticText{ nullptr };
		winrt::hstring m_value;
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::Effect::factory_implementation
{
	struct AnimatedNumber : AnimatedNumberT<AnimatedNumber, implementation::AnimatedNumber>
	{
	};
}
