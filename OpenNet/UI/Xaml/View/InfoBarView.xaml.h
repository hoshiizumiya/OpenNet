#pragma once

import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.Foundation.Collections;
import winrt.XamlToolkit.Labs.WinUI;

#include "Service/Notification/InfoBarOptions.h"
#include "UI/Xaml/View/InfoBarView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	struct InfoBarView : InfoBarViewT<InfoBarView>
	{
		InfoBarView();
		~InfoBarView();
		void InitializeComponent();

		winrt::Windows::Foundation::Collections::IObservableVector<
			OpenNet::Service::Notification::InfoBarOptions> InfoBars() const;

		static void Show(
			winrt::hstring const& title,
			winrt::hstring const& message,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity = winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational,
			std::uint32_t delayMilliseconds = 6000,
			winrt::hstring const& actionText = {},
			std::function<void()> action = {});

		void OnInfoBarClosed(
			winrt::Microsoft::UI::Xaml::Controls::InfoBar const& sender,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarClosedEventArgs const&);
		void OnClearAllButtonClick(
			winrt::Windows::Foundation::IInspectable const&,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		struct TimedInfoBar
		{
			OpenNet::Service::Notification::InfoBarOptions options{ nullptr };
			winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer timer{ nullptr };
		};

		void Enqueue(
			winrt::hstring const& title,
			winrt::hstring const& message,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity,
			std::uint32_t delayMilliseconds,
			winrt::hstring const& actionText,
			std::function<void()> action);
		void Remove(
			OpenNet::Service::Notification::InfoBarOptions const& options);
		void RemoveTimer(
			OpenNet::Service::Notification::InfoBarOptions const& options);
		void OnInfoBarsVectorChanged(
			winrt::Windows::Foundation::Collections::IObservableVector<
			OpenNet::Service::Notification::InfoBarOptions> const&,
			winrt::Windows::Foundation::Collections::IVectorChangedEventArgs const& args);
		void UnsubscribeInfoBars();
		winrt::fire_and_forget HandleInfoBarsCollectionChangedAsync(bool added);
		void UpdateBadge();
		void StopTimers();

		static inline winrt::weak_ref<InfoBarView> s_current;
		winrt::Windows::Foundation::Collections::IObservableVector<
			OpenNet::Service::Notification::InfoBarOptions> m_infoBars{
				winrt::single_threaded_observable_vector<
					OpenNet::Service::Notification::InfoBarOptions>() };
		std::vector<TimedInfoBar> m_timers;
		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer
			m_clearTimer{ nullptr };
		winrt::event_token m_infoBarsChangedToken{};
		bool m_infoBarsSubscribed{};
	};
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
	struct InfoBarView :
		InfoBarViewT<InfoBarView, implementation::InfoBarView>
	{
	};
}
