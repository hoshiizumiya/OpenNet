#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using MainViewModel = ::OpenNet::ViewModels::MainViewModel;

namespace winrt::OpenNet::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        // 初始化ViewModel / Initialize ViewModel
        m_viewModel = MainViewModel{};
        m_viewModel.Initialize();
    }

    MainViewModel MainWindow::ViewModel()
    {
        return m_viewModel;
    }
}
