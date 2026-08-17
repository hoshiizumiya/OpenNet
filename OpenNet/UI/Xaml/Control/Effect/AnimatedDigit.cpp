#include "XamlWorkaround.h"
#include "AnimatedDigit.h"
#if __has_include("UI/Xaml/Control/Effect/AnimatedDigit.g.cpp")
#include "UI/Xaml/Control/Effect/AnimatedDigit.g.cpp"
#endif

import OpenNet.Core.AppSettingsDatabase;
import winrt.Microsoft.UI.Xaml;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Data;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;

namespace winrt::OpenNet::UI::Xaml::Control::Effect::implementation
{
	namespace
	{
		constexpr auto AnimatedDigitsSetting = "animated_digits_enabled";
		std::atomic_bool s_animationsEnabled{};
		std::once_flag s_loadSetting;

		bool IsDigit(std::int32_t const value) noexcept
		{
			return value >= L'0' && value <= L'9';
		}
	}

	void AnimatedDigit::OnApplyTemplate()
	{
		base_type::OnApplyTemplate();
		m_increaseCurrentAnimation = GetTemplateChild(L"IncreaseCurrentDigitTranslateAnimation").try_as<DoubleAnimation>();
		m_increaseNextAnimation = GetTemplateChild(L"IncreaseNextDigitTranslateAnimation").try_as<DoubleAnimation>();
		m_decreaseCurrentAnimation = GetTemplateChild(L"DecreaseCurrentDigitTranslateAnimation").try_as<DoubleAnimation>();
		m_decreaseNextAnimation = GetTemplateChild(L"DecreaseNextDigitTranslateAnimation").try_as<DoubleAnimation>();
		ApplyValue(false);
	}

	void AnimatedDigit::Value(std::int32_t const value)
	{
		if (m_value == value && !m_isFirst)
		{
			return;
		}
		auto const previousValue = m_value;
		m_value = value;
		ApplyValue(AnimationsEnabled() && !m_isFirst && IsDigit(value), previousValue);
		m_isFirst = false;
	}

	std::int32_t AnimatedDigit::Value() const noexcept { return m_value; }

	void AnimatedDigit::ApplyValue(bool const animate, std::int32_t const previousValue)
	{
		if (!animate || !IsDigit(m_value))
		{
			VisualStateManager::GoToState(*this, L"Normal", true);
			SetCurrentValue(m_value);
			SetNextValue(-1);
			return;
		}

		auto const fontSize = FontSize();
		if (m_value > previousValue)
		{
			if (m_increaseCurrentAnimation)
			{
				m_increaseCurrentAnimation.From(0.0);
				m_increaseCurrentAnimation.To(-fontSize - 10.0);
			}
			if (m_increaseNextAnimation)
			{
				m_increaseNextAnimation.From(fontSize);
				m_increaseNextAnimation.To(0.0);
			}
			SetCurrentValue(previousValue);
			VisualStateManager::GoToState(*this, L"Normal", true);
			SetNextValue(m_value);
			VisualStateManager::GoToState(*this, L"IncreaseState", true);
		}
		else
		{
			if (m_decreaseCurrentAnimation)
			{
				m_decreaseCurrentAnimation.From(-fontSize - 10.0);
				m_decreaseCurrentAnimation.To(0.0);
			}
			if (m_decreaseNextAnimation)
			{
				m_decreaseNextAnimation.From(0.0);
				m_decreaseNextAnimation.To(fontSize);
			}
			SetNextValue(previousValue);
			VisualStateManager::GoToState(*this, L"Normal", true);
			SetCurrentValue(m_value);
			VisualStateManager::GoToState(*this, L"DecreaseState", true);
		}
	}

	hstring AnimatedDigit::CurrentValue() const
	{
		return m_currentValue < 0 ? hstring{} : hstring{ static_cast<wchar_t>(m_currentValue) };
	}

	hstring AnimatedDigit::NextValue() const
	{
		return m_nextValue < 0 ? hstring{} : hstring{ static_cast<wchar_t>(m_nextValue) };
	}

	void AnimatedDigit::SetCurrentValue(std::int32_t const value)
	{
		if (std::exchange(m_currentValue, value) != value) RaisePropertyChanged(L"CurrentValue");
	}

	void AnimatedDigit::SetNextValue(std::int32_t const value)
	{
		if (std::exchange(m_nextValue, value) != value) RaisePropertyChanged(L"NextValue");
	}

	event_token AnimatedDigit::PropertyChanged(PropertyChangedEventHandler const& handler)
	{
		return m_propertyChanged.add(handler);
	}

	void AnimatedDigit::PropertyChanged(event_token const& token) noexcept
	{
		m_propertyChanged.remove(token);
	}

	void AnimatedDigit::RaisePropertyChanged(wchar_t const* propertyName)
	{
		m_propertyChanged(*this, PropertyChangedEventArgs{ propertyName });
	}

	bool AnimatedDigit::AnimationsEnabled() noexcept
	{
		std::call_once(s_loadSetting, []
		{
			try
			{
				auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
				database.Initialize();
				s_animationsEnabled.store(database.GetBool(
					::OpenNet::Core::AppSettingsDatabase::CAT_UI,
					AnimatedDigitsSetting).value_or(false), std::memory_order_relaxed);
			}
			catch (...) { s_animationsEnabled.store(false, std::memory_order_relaxed); }
		});
		return s_animationsEnabled.load(std::memory_order_relaxed);
	}

	void AnimatedDigit::AnimationsEnabled(bool const enabled) noexcept
	{
		(void)AnimationsEnabled();
		s_animationsEnabled.store(enabled, std::memory_order_relaxed);
	}
}
