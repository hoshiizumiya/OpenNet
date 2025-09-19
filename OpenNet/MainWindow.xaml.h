#pragma once

#include "ViewModels/MainViewModel.h"
#include "MainWindow.g.h"

using MainViewModel = winrt::OpenNet::ViewModels::MainViewModel;

namespace winrt::OpenNet::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        MainViewModel ViewModel();

		// Window State management methods
        winrt::Windows::Foundation::IAsyncAction SetIconAsync(winrt::Microsoft::UI::Windowing::AppWindow window);
        void InitWindowStyle(winrt::Microsoft::UI::Xaml::Window const& window);
        void SaveWindowState();
        void RestoreWindowState();

    private:
        MainViewModel m_viewModel;
        void NavigateToType(Windows::UI::Xaml::Interop::TypeName const& type, winrt::Windows::Foundation::IInspectable const& parameter);

    public:
        // Legacy name kept for compatibility
        void Navigate(winrt::hstring const& tag, winrt::Windows::Foundation::IInspectable const& parameter = nullptr);
        // Preferred explicit name
        void NavigateTag(winrt::hstring const& tag, winrt::Windows::Foundation::IInspectable const& parameter = nullptr);

        void AppTitleBar_BackRequested(winrt::Microsoft::UI::Xaml::Controls::TitleBar const& sender, winrt::Windows::Foundation::IInspectable const& args);
        void NavView_ItemInvoked(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender, winrt::Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs const& args);
        void NavFrame_Navigating(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Navigation::NavigatingCancelEventArgs const& args);
        void NavFrame_Navigated(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args);
    };
}

namespace winrt::OpenNet::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
