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
