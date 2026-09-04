#pragma once

// workaround for XAML-generated .xaml.g.h files not supporting C++20 modules
import winrt.OpenNet.Helpers;
import winrt.OpenNet.UI.Xaml.View;
import winrt.XamlToolkit.Labs.WinUI;
#include "UI/Xaml/View/Pages/MainView.g.h"
#include "UI/Xaml/View/Pages/TasksPage.xaml.h"

import winrt.OpenNet.ViewModels;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct MainView : MainViewT<MainView>
	{
		~MainView();
		void InitializeComponent();
		void MainView_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void MainView_Unloaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::Windows::Foundation::IAsyncAction RefreshBackgroundMediaAsync(bool advance);
		void SetBackgroundPlaybackActive(bool active);
		void AttachBackgroundPresenters(winrt::Microsoft::UI::Xaml::Controls::Image const& image, winrt::Microsoft::UI::Xaml::Controls::MediaPlayerElement const& video);

		// ViewModel
		winrt::OpenNet::ViewModels::MainViewModel ViewModel();

		// Navigation
		void Navigate(winrt::Windows::UI::Xaml::Interop::TypeName const& pageType);
		bool CanGoBack();
		void GoBack();
		bool CanGoForward();
		void GoForward();
		winrt::OpenNet::UI::Xaml::View::Pages::TasksPage CurrentTasksPage();

		// Event: CanGoBackChanged
		winrt::event_token CanGoBackChanged(winrt::Windows::Foundation::EventHandler<bool> const& handler);
		void CanGoBackChanged(winrt::event_token const& token) noexcept;

		// Event handlers (XAML wired)
		void NavFrame_Navigated(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&);
		void NavFrame_NavigationFailed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Navigation::NavigationFailedEventArgs const&);
		void NavItem_More_Tapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& e);
		void SettingButton_PointerEntered(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
		void SettingButton_PointerExited(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
		void OpenPortTestWindow_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void NavView_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::NavigationView const&, winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const&);

	private:
		void StopBackgroundMedia();
		winrt::Microsoft::UI::Xaml::Controls::Image BackgroundImagePresenter() const;
		winrt::Microsoft::UI::Xaml::Controls::MediaPlayerElement BackgroundVideoPresenter() const;

		winrt::OpenNet::ViewModels::MainViewModel m_viewModel;
		winrt::event<winrt::Windows::Foundation::EventHandler<bool>> m_canGoBackChanged;
		winrt::Microsoft::UI::Xaml::DispatcherTimer m_backgroundTimer{ nullptr };
		winrt::event_token m_backgroundTimerToken{};
		winrt::event_token m_backgroundOptionsChangedToken{};
		winrt::event_token m_backgroundPresentersChangedToken{};
		winrt::Microsoft::UI::Xaml::Controls::Image m_backgroundImagePresenter{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::MediaPlayerElement m_backgroundVideoPresenter{ nullptr };
		bool m_backgroundPlaybackActive{ true };
		bool m_isUnloaded{ false };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct MainView : MainViewT<MainView, implementation::MainView>
	{
	};
}
