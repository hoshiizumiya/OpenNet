#pragma once

#include "ViewModels/MainViewModel.h"
#include <winrt/OpenNet.UI.Xaml.View.Pages.h>
#include "MainWindow.g.h"

namespace winrt::OpenNet::implementation
{
	struct MainWindow : MainWindowT<MainWindow>
	{
		MainWindow();

		void InvertAppThemeButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		// ViewModel (delegated to MainContentView)
		winrt::OpenNet::ViewModels::MainViewModel ViewModel();

		// Navigation (delegated to MainContentView)
		void Navigate(winrt::hstring const& tag);

		// Event handlers (XAML wired)
		void AppTitleBar_BackRequested(winrt::Microsoft::UI::Xaml::Controls::TitleBar const&,
									   winrt::Windows::Foundation::IInspectable const&);

		void Grid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		winrt::Windows::Foundation::IAsyncAction LoadBackground();

		Microsoft::UI::Xaml::Visibility IsDebug();
	private:
		void InitWindowStyle(winrt::Microsoft::UI::Xaml::Window const& window);
		void RootGridXamlRoot_Changed(winrt::Microsoft::UI::Xaml::XamlRoot sender, winrt::Microsoft::UI::Xaml::XamlRootChangedEventArgs args);
		winrt::event_token m_canGoBackChangedToken{};
	};
}

namespace winrt::OpenNet::factory_implementation
{
	struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
	{
	};
}
