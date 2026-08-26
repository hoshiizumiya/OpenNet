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
		std::mutex s_instancesMutex;
		std::vector<winrt::weak_ref<AnimatedDigit>> s_instances;

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
		{
			std::lock_guard lock(s_instancesMutex);
			std::erase_if(s_instances, [](auto const& instance)
			{
				return !instance.get();
			});
			s_instances.emplace_back(get_weak());
		}
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

	std::int32_t AnimatedDigit::Value() const noexcept
	{
		return m_value;
	}

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
		event_token token{ ++m_nextPropertyChangedToken };
		m_propertyChangedHandlers.emplace_back(token, handler);
		return token;
	}

	void AnimatedDigit::PropertyChanged(event_token const& token) noexcept
	{
		std::erase_if(m_propertyChangedHandlers, [&](auto const& entry)
		{
			return entry.first == token;
		});
	}

	void AnimatedDigit::RaisePropertyChanged(wchar_t const* propertyName)
	{
		auto const args = PropertyChangedEventArgs{ propertyName };
		auto const handlers = m_propertyChangedHandlers;
		for (auto const& [token, handler] : handlers)
		{
			(void)token;
			try
			{
				handler(*this, args);
			}
			catch (...)
			{
			}
		}
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
					AnimatedDigitsSetting).value_or(true), std::memory_order_relaxed);
			}
			catch (...)
			{
				s_animationsEnabled.store(true, std::memory_order_relaxed);
			}
		});
		return s_animationsEnabled.load(std::memory_order_relaxed);
	}

	void AnimatedDigit::AnimationsEnabled(bool const enabled) noexcept
	{
		(void)AnimationsEnabled();
		s_animationsEnabled.store(enabled, std::memory_order_relaxed);
		if (enabled) return;

		std::vector<winrt::com_ptr<AnimatedDigit>> instances;
		{
			std::lock_guard lock(s_instancesMutex);
			for (auto const& weak : s_instances)
			{
				if (auto instance = weak.get()) instances.push_back(std::move(instance));
			}
			std::erase_if(s_instances, [](auto const& instance)
			{
				return !instance.get();
			});
		}
		for (auto const& instance : instances) instance->ApplyValue(false);
	}
}
