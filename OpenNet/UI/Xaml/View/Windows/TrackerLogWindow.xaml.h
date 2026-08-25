#pragma once

#include "UI/Xaml/View/Windows/TrackerLogWindow.g.h"

import OpenNet.Helpers.WindowExBase;
import winrt.Microsoft.UI.Xaml;
import winrt.Windows.Foundation.Collections;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct TrackerLogWindow : TrackerLogWindowT<TrackerLogWindow>, WindowExBase<TrackerLogWindow>
	{
		TrackerLogWindow();
		TrackerLogWindow(
			winrt::hstring const& taskId,
			winrt::hstring const& taskName,
			winrt::hstring const& trackerUrl);

		void AlwaysOnTopToggle_Toggled(
			winrt::Windows::Foundation::IInspectable const&,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		void InitializeWindow();
		void RefreshLog();
		winrt::hstring m_taskId;
		winrt::hstring m_trackerUrl;
		winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
		winrt::event_token m_timerToken{};
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_entries{ nullptr };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct TrackerLogWindow : TrackerLogWindowT<TrackerLogWindow, implementation::TrackerLogWindow>
	{
	};
}
