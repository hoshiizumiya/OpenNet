#pragma once

import winrt.OpenNet.UI.Xaml.Control.Progress.Storage;
import winrt.OpenNet.UI.Xaml.Control.Progress.HttpSegment;
#include "Controls/SpeedGraph/SpeedGraph.xaml.h"
#include "UI/Xaml/View/Pages/TaskSummaryPage.g.h"
#include "ViewModels/TasksViewModel.h"

import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Data;
import std;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskSummaryPage : TaskSummaryPageT<TaskSummaryPage>
	{
		TaskSummaryPage();
		~TaskSummaryPage();

		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);

	private:
		void RefreshSummary();
		void ResetSummary();
		void Unsubscribe();
		void StopRefreshTimer() noexcept;
		void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
		void OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		static winrt::hstring FormatBytes(std::int64_t value);
		static winrt::hstring FormatRate(std::int64_t value);
		static winrt::hstring FormatDuration(std::int64_t seconds);
		static winrt::hstring FormatTimestamp(std::int64_t timestamp);
		static winrt::hstring TorrentStateText(int state, bool paused);

		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_vmPropertyChangedToken{};
		winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
		winrt::event_token m_timerTickToken{};
		std::atomic_bool m_isActive{};
		winrt::hstring m_graphTaskId;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskSummaryPage : TaskSummaryPageT<TaskSummaryPage, implementation::TaskSummaryPage>
	{
	};
}
