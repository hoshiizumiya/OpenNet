#pragma once

import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.OpenNet.Service.Notification;
import winrt.Windows.Foundation.Collections;
import winrt.XamlToolkit.Labs.WinUI;
import OpenNet.Service.Notification.InfoBarService;

#include "UI/Xaml/View/InfoBarView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	struct InfoBarView : InfoBarViewT<InfoBarView>
	{
		InfoBarView();
		~InfoBarView();
		void InitializeComponent();

		winrt::Windows::Foundation::Collections::IObservableVector<OpenNet::Service::Notification::InfoBarOptions> InfoBars() const;

		void OnInfoBarClosed(winrt::Microsoft::UI::Xaml::Controls::InfoBar const& sender, winrt::Microsoft::UI::Xaml::Controls::InfoBarClosedEventArgs const&);
		void OnClearAllButtonClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		void Remove(OpenNet::Service::Notification::InfoBarOptions const& options);
		void OnInfoBarsVectorChanged(winrt::Windows::Foundation::Collections::IObservableVector<OpenNet::Service::Notification::InfoBarOptions> const&, winrt::Windows::Foundation::Collections::IVectorChangedEventArgs const& args);
		void SubscribeInfoBars();
		void UnsubscribeInfoBars();
		void SynchronizeState();
		winrt::fire_and_forget HandleInfoBarsCollectionChangedAsync(bool added, std::uint64_t version);
		void UpdateBadge();

		winrt::Windows::Foundation::Collections::IObservableVector<OpenNet::Service::Notification::InfoBarOptions> m_infoBars{ nullptr };
		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer	m_clearTimer{ nullptr };
		winrt::event_token m_infoBarsChangedToken{};
		bool m_infoBarsSubscribed{};
		std::uint64_t m_transitionVersion{};
	};
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
	struct InfoBarView : InfoBarViewT<InfoBarView, implementation::InfoBarView>
	{
	};
}
