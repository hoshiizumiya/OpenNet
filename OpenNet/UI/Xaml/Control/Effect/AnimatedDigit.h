#pragma once

#include "UI/Xaml/Control/Effect/AnimatedDigit.g.h"

import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Media.Animation;

namespace winrt::OpenNet::UI::Xaml::Control::Effect::implementation
{
	struct AnimatedDigit : AnimatedDigitT<AnimatedDigit>
	{
		AnimatedDigit() = default;

		void OnApplyTemplate();
		void Value(std::int32_t value);
		std::int32_t Value() const noexcept;
		winrt::hstring CurrentValue() const;
		winrt::hstring NextValue() const;

		winrt::event_token PropertyChanged(
			winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
		void PropertyChanged(winrt::event_token const& token) noexcept;

		static bool AnimationsEnabled() noexcept;
		static void AnimationsEnabled(bool enabled) noexcept;

	private:
		void SetCurrentValue(std::int32_t value);
		void SetNextValue(std::int32_t value);
		void RaisePropertyChanged(wchar_t const* propertyName);
		void ApplyValue(bool animate, std::int32_t previousValue = -1);

		bool m_isFirst{ true };
		std::int32_t m_value{ -1 };
		std::int32_t m_currentValue{ -1 };
		std::int32_t m_nextValue{ -1 };
		winrt::Microsoft::UI::Xaml::Media::Animation::DoubleAnimation m_increaseCurrentAnimation{ nullptr };
		winrt::Microsoft::UI::Xaml::Media::Animation::DoubleAnimation m_increaseNextAnimation{ nullptr };
		winrt::Microsoft::UI::Xaml::Media::Animation::DoubleAnimation m_decreaseCurrentAnimation{ nullptr };
		winrt::Microsoft::UI::Xaml::Media::Animation::DoubleAnimation m_decreaseNextAnimation{ nullptr };
		winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::Effect::factory_implementation
{
	struct AnimatedDigit : AnimatedDigitT<AnimatedDigit, implementation::AnimatedDigit>
	{
	};
}
