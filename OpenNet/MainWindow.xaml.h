#pragma once

#include "ViewModels/MainViewModel.h"
#include "MainWindow.g.h"

using MainViewModel = winrt::OpenNet::ViewModels::MainViewModel;

namespace winrt::OpenNet::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        // define a property to hold the ViewModel
        MainViewModel ViewModel();

		// Window State management methods
        winrt::Windows::Foundation::IAsyncAction SetIconAsync(winrt::Microsoft::UI::Windowing::AppWindow window);
        void InitWindowStyle(winrt::Microsoft::UI::Xaml::Window const& window);
        void SaveWindowState();
        void RestoreWindowState();

    private:
        MainViewModel m_viewModel;
    };
}

namespace winrt::OpenNet::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
