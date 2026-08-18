#include "XamlWorkaround.h"
#include "InfoBarDelayCloseBehavior.h"
#if __has_include("UI/Xaml/Behavior/InfoBarDelayCloseBehavior.g.cpp")
#include "UI/Xaml/Behavior/InfoBarDelayCloseBehavior.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Dispatching;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::Behavior::implementation
{
	DependencyProperty InfoBarDelayCloseBehavior::MilliSecondsDelayProperty()
	{
		static auto property = DependencyProperty::Register(
			L"MilliSecondsDelay",
			xaml_typename<std::uint32_t>(),
			xaml_typename<OpenNet::UI::Xaml::Behavior::InfoBarDelayCloseBehavior>(),
			PropertyMetadata{ box_value(std::uint32_t{}) });

		return property;
	}

	std::uint32_t InfoBarDelayCloseBehavior::MilliSecondsDelay() const
	{
		return unbox_value<std::uint32_t>(GetValue(MilliSecondsDelayProperty()));
	}

	void InfoBarDelayCloseBehavior::MilliSecondsDelay(std::uint32_t value)
	{
		SetValue(MilliSecondsDelayProperty(), box_value(value));
	}

	void InfoBarDelayCloseBehavior::OnAssociatedObjectLoaded()
	{
		Stop();

		auto infoBar = AssociatedObject();
		auto const delay = MilliSecondsDelay();

		if (!infoBar || delay == 0)
		{
			return;
		}

		m_timer = infoBar.DispatcherQueue().CreateTimer();
		m_timer.IsRepeating(false);
		m_timer.Interval(
			std::chrono::milliseconds{ delay });

		auto weakInfoBar = make_weak(infoBar);

		m_timer.Tick(
			[weakInfoBar](
				DispatcherQueueTimer const& timer,
				IInspectable const&)
		{
			timer.Stop();

			if (auto infoBar = weakInfoBar.get())
			{
				infoBar.IsOpen(false);
			}
		});

		m_timer.Start();
	}

	void InfoBarDelayCloseBehavior::OnAssociatedObjectUnloaded()
	{
		Stop();
	}


	void InfoBarDelayCloseBehavior::Stop()
	{
		if (m_timer)
		{
			m_timer.Stop();
			m_timer = nullptr;
		}
	}
}