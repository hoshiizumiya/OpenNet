#pragma once

import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.Foundation.Collections;
import winrt.XamlToolkit.Labs.WinUI;

#include "Service/Notification/InfoBarOptions.h"
#include "Service/Notification/InfoBarService.h"
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
		winrt::fire_and_forget HandleInfoBarsCollectionChangedAsync(bool added);
		void UpdateBadge();

		winrt::Windows::Foundation::Collections::IObservableVector<OpenNet::Service::Notification::InfoBarOptions> m_infoBars{ nullptr };
		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer	m_clearTimer{ nullptr };
		winrt::event_token m_infoBarsChangedToken{};
		bool m_infoBarsSubscribed{};
	};
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
	struct InfoBarView : InfoBarViewT<InfoBarView, implementation::InfoBarView>
	{
	};
}
