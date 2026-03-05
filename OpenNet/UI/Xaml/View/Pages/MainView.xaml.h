#pragma once

#include "ViewModels/MainViewModel.h"
#include "UI/Xaml/View/Pages/MainView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct MainView : MainViewT<MainView>
	{
		MainView();

		// ViewModel
		winrt::OpenNet::ViewModels::MainViewModel ViewModel();

		// Navigation
		void Navigate(winrt::hstring const& tag);
		bool CanGoBack();
		void GoBack();

		// Event: CanGoBackChanged
		winrt::event_token CanGoBackChanged(winrt::Windows::Foundation::EventHandler<bool> const& handler);
		void CanGoBackChanged(winrt::event_token const& token) noexcept;

		// Page open helpers
		void openHomePage();
		void openContactsPage();
		void openTasksPage();
		void openFilesPage();
		void openNetworkSettingsPage();
		void openServersPage();
		void openRSSPage();
		void openNatToolsPage();
		void openSettingsPage();

		// Event handlers (XAML wired)
		void NavView_ItemInvoked(winrt::Microsoft::UI::Xaml::Controls::NavigationView const&,
		                         winrt::Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs const&);
		void NavFrame_Navigating(winrt::Windows::Foundation::IInspectable const&,
		                         winrt::Microsoft::UI::Xaml::Navigation::NavigatingCancelEventArgs const&);
		void NavFrame_Navigated(winrt::Windows::Foundation::IInspectable const&,
		                        winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&);
		void NavView_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::NavigationView const&,
		                              winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const&);

	private:
		void UpdateNavigationSelection(winrt::hstring const& tag);

		winrt::OpenNet::ViewModels::MainViewModel m_viewModel{ nullptr };
		winrt::event<winrt::Windows::Foundation::EventHandler<bool>> m_canGoBackChanged;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct MainView : MainViewT<MainView, implementation::MainView>
	{
	};
}
