#pragma once

#include "UI/Xaml/Behavior/InfoBarDelayCloseBehavior.g.h"

import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Dispatching;
import winrt.XamlToolkit.WinUI.Interactivity;

namespace winrt::OpenNet::UI::Xaml::Behavior::implementation
{
	struct InfoBarDelayCloseBehavior : InfoBarDelayCloseBehaviorT<InfoBarDelayCloseBehavior>
	{
		InfoBarDelayCloseBehavior() = default;
		static winrt::Microsoft::UI::Xaml::DependencyProperty MilliSecondsDelayProperty();
		std::uint32_t MilliSecondsDelay() const;
		void MilliSecondsDelay(std::uint32_t value);
		winrt::Microsoft::UI::Xaml::DependencyObject AssociatedObject() const;
		void Attach(winrt::Microsoft::UI::Xaml::DependencyObject const& associatedObject);
		void Detach();

	private:
		void Start();
		void Stop();
		winrt::Microsoft::UI::Xaml::DependencyObject m_associatedObject{ nullptr };
		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
		winrt::event_token m_loadedToken{};
		winrt::event_token m_unloadedToken{};
		winrt::event_token m_closedToken{};
	};
}

namespace winrt::OpenNet::UI::Xaml::Behavior::factory_implementation
{
	struct InfoBarDelayCloseBehavior : InfoBarDelayCloseBehaviorT<
		InfoBarDelayCloseBehavior, implementation::InfoBarDelayCloseBehavior>
	{
	};
}
