#pragma once

import winrt.Microsoft.UI.Dispatching;

#include "UI/Xaml/View/InfoBarView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	struct InfoBarView : InfoBarViewT<InfoBarView>
	{
		InfoBarView();
		~InfoBarView();
		void InitializeComponent();
		static void Show(
			winrt::hstring const& title,
			winrt::hstring const& message,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity =
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational,
			std::uint32_t delayMilliseconds = 6000,
			winrt::hstring const& actionText = {},
			std::function<void()> action = {});
		void OnClearAllButtonClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnCollapseButtonClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnShowButtonClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		void Enqueue(
			winrt::hstring const& title,
			winrt::hstring const& message,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity,
			std::uint32_t delayMilliseconds,
			winrt::hstring const& actionText,
			std::function<void()> action);
		void Remove(
			winrt::Microsoft::UI::Xaml::Controls::InfoBar const& infoBar);
		void UpdateVisibility();
		static inline InfoBarView* s_current{};
		std::vector<
			winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer> m_timers;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
	struct InfoBarView : InfoBarViewT<InfoBarView, implementation::InfoBarView>
	{
	};
}
