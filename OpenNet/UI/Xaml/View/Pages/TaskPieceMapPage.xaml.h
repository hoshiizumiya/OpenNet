#pragma once

import OpenNet.ViewModels.ObservableMixin;
#include "UI/Xaml/View/Pages/TaskPieceMapPage.g.h"
#include "ViewModels/TasksViewModel.h"

import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Data;
import std;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskPieceMapPage : TaskPieceMapPageT<TaskPieceMapPage>, ::OpenNet::ViewModels::ObservableMixin<TaskPieceMapPage>
	{
		TaskPieceMapPage();
		~TaskPieceMapPage();

		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);
		void RefreshButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		void RefreshPieceMap();
		void Unsubscribe();
		void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
		void OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
		static winrt::hstring FormatBytes(std::int64_t value);
		static winrt::Microsoft::UI::Xaml::Controls::Border CreatePieceElement(
			std::size_t index,
			int state,
			int availability,
			winrt::hstring const& hash);

		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_vmPropertyChangedToken{};
		winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
		winrt::event_token m_timerTickToken{};
		winrt::hstring m_renderedTaskId;
		std::vector<int> m_renderedStates;
		std::vector<int> m_renderedAvailability;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskPieceMapPage : TaskPieceMapPageT<TaskPieceMapPage, implementation::TaskPieceMapPage>
	{
	};
}
