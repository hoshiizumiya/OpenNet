#pragma once

#include "UI/Xaml/Behavior/InfoBarDelayCloseBehavior.g.h"

import OpenNet.UI.Xaml.BehaviorBase;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Dispatching;
import winrt.XamlToolkit.WinUI.Interactivity;

namespace winrt::OpenNet::UI::Xaml::Behavior::implementation
{
	struct InfoBarDelayCloseBehavior : InfoBarDelayCloseBehaviorT<InfoBarDelayCloseBehavior>, winrt::XamlToolkit::WinUI::Behaviors::BehaviorBase<InfoBarDelayCloseBehavior, winrt::Microsoft::UI::Xaml::Controls::InfoBar>
	{
		InfoBarDelayCloseBehavior() = default;
		static winrt::Microsoft::UI::Xaml::DependencyProperty MilliSecondsDelayProperty();
		std::uint32_t MilliSecondsDelay() const;
		void MilliSecondsDelay(std::uint32_t value);
	protected:
		void OnAssociatedObjectLoaded() override;
		void OnAssociatedObjectUnloaded() override;
	private:
		void Stop();
		void OnInfoBarClosed(Microsoft::UI::Xaml::Controls::InfoBar const& sender, Microsoft::UI::Xaml::Controls::InfoBarClosedEventArgs const& args);

		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
	};
}

namespace winrt::OpenNet::UI::Xaml::Behavior::factory_implementation
{
	struct InfoBarDelayCloseBehavior : InfoBarDelayCloseBehaviorT<InfoBarDelayCloseBehavior, implementation::InfoBarDelayCloseBehavior>
	{
	};
}
