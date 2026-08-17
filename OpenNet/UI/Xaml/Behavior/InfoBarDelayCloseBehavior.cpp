#include "XamlWorkaround.h"
#include "InfoBarDelayCloseBehavior.h"
#if __has_include("UI/Xaml/Behavior/InfoBarDelayCloseBehavior.g.cpp")
#include "UI/Xaml/Behavior/InfoBarDelayCloseBehavior.g.cpp"
#endif

import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Controls;

using namespace winrt;
using namespace winrt::Microsoft::UI::Dispatching;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::Behavior::implementation
{
	DependencyProperty InfoBarDelayCloseBehavior::MilliSecondsDelayProperty()
	{
		static auto property = DependencyProperty::Register(
			L"MilliSecondsDelay", xaml_typename<std::uint32_t>(),
			xaml_typename<OpenNet::UI::Xaml::Behavior::InfoBarDelayCloseBehavior>(),
			PropertyMetadata{ box_value(std::uint32_t{}) });
		return property;
	}

	std::uint32_t InfoBarDelayCloseBehavior::MilliSecondsDelay() const
	{
		return unbox_value<std::uint32_t>(GetValue(MilliSecondsDelayProperty()));
	}

	void InfoBarDelayCloseBehavior::MilliSecondsDelay(std::uint32_t const value)
	{
		SetValue(MilliSecondsDelayProperty(), box_value(value));
		if (m_associatedObject) Start();
	}

	DependencyObject InfoBarDelayCloseBehavior::AssociatedObject() const
	{
		return m_associatedObject;
	}

	void InfoBarDelayCloseBehavior::Attach(DependencyObject const& associatedObject)
	{
		if (m_associatedObject == associatedObject) return;
		if (m_associatedObject) throw hresult_illegal_method_call();
		auto infoBar = associatedObject.try_as<InfoBar>();
		if (!infoBar) throw hresult_invalid_argument(L"InfoBarDelayCloseBehavior requires an InfoBar.");
		m_associatedObject = associatedObject;
		m_loadedToken = infoBar.Loaded([this](auto const&, auto const&) { Start(); });
		m_unloadedToken = infoBar.Unloaded([this](auto const&, auto const&) { Stop(); });
		m_closedToken = infoBar.Closed([this](auto const&, InfoBarClosedEventArgs const& args)
		{
			if (args.Reason() == InfoBarCloseReason::CloseButton) Stop();
		});
		if (infoBar.IsLoaded()) Start();
	}

	void InfoBarDelayCloseBehavior::Detach()
	{
		Stop();
		if (auto infoBar = m_associatedObject.try_as<InfoBar>())
		{
			if (m_loadedToken.value) infoBar.Loaded(m_loadedToken);
			if (m_unloadedToken.value) infoBar.Unloaded(m_unloadedToken);
			if (m_closedToken.value) infoBar.Closed(m_closedToken);
		}
		m_loadedToken = {};
		m_unloadedToken = {};
		m_closedToken = {};
		m_associatedObject = nullptr;
	}

	void InfoBarDelayCloseBehavior::Start()
	{
		Stop();
		auto infoBar = m_associatedObject.try_as<InfoBar>();
		auto const delay = MilliSecondsDelay();
		if (!infoBar || delay == 0) return;
		m_timer = infoBar.DispatcherQueue().CreateTimer();
		m_timer.IsRepeating(false);
		m_timer.Interval(std::chrono::milliseconds(delay));
		auto weakTarget = make_weak(infoBar);
		m_timer.Tick([weakTarget](DispatcherQueueTimer const& timer, auto const&)
		{
			timer.Stop();
			if (auto target = weakTarget.get()) target.IsOpen(false);
		});
		m_timer.Start();
	}

	void InfoBarDelayCloseBehavior::Stop()
	{
		if (m_timer) m_timer.Stop();
		m_timer = nullptr;
	}
}
