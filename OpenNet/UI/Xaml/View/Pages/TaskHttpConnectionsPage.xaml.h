#pragma once

#include "UI/Xaml/View/Pages/TaskHttpConnectionsPage.g.h"
#include "ViewModels/TasksViewModel.h"
#include "ViewModels/DisplayItems.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskHttpConnectionsPage : TaskHttpConnectionsPageT<TaskHttpConnectionsPage>
	{
		TaskHttpConnectionsPage();
		~TaskHttpConnectionsPage() = default;

		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);
		void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);

	private:
		void StartRefreshTimer();
		void StopRefreshTimer() noexcept;
		void Unsubscribe();
		void OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
		winrt::fire_and_forget RefreshAsync();

		winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
		winrt::event_token m_viewModelToken{};
		winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
		winrt::event_token m_timerToken{};
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_items{ nullptr };
		std::atomic_bool m_isActive{};
		std::atomic_bool m_refreshInFlight{};
		std::atomic_uint64_t m_generation{};
		std::size_t m_configuredConnections{ 8 };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskHttpConnectionsPage : TaskHttpConnectionsPageT<TaskHttpConnectionsPage, implementation::TaskHttpConnectionsPage>
	{
	};
}
