#pragma once

#include "MainWindow.g.h"

namespace winrt::OpenNetUnitTest::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
    };
}

namespace winrt::OpenNetUnitTest::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
