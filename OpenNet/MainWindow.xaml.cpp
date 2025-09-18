#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace Microsoft::UI::Xaml;
using MainViewModel = ::OpenNet::ViewModels::MainViewModel;

/*
“窗口样式/标题栏/应用图标”等仅与 UI 外观相关的代码，应放在 MainWindow.xaml.cpp（View 的 code-behind），不应放入 ViewModel。
ViewModel 只放与界面无关的状态、数据、命令（业务 / 导航状态等），不能引用 Microsoft::UI:: * 或 Windowing 之类的 UI API。
*/

namespace winrt::OpenNet::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

		SetTitleBar(AppTitleBar());

		// 初始化窗口样式
		InitWindowStyle(*this);

		// 加载收藏夹
		//LoadFavorites();

		// 恢复窗口状态
		//RestoreWindowState();

        // 初始化ViewModel / Initialize ViewModel
        m_viewModel = MainViewModel{};
        m_viewModel.Initialize();
    }

    MainViewModel MainWindow::ViewModel()
    {
        return m_viewModel;
    }

#pragma region Window_State_Management
	IAsyncAction MainWindow::SetIconAsync(Microsoft::UI::Windowing::AppWindow window)
	{
		if (!window)
			co_return;

		try
		{
			// TODO: 设置应用程序图标
			// 这里可以设置窗口图标
		}
		catch (...)
		{
			// 忽略图标设置错误
		}

		co_return;
	}

	void MainWindow::InitWindowStyle(Window const& window)
	{
		window.ExtendsContentIntoTitleBar(true);

		auto appWindow = window.AppWindow();
		if (appWindow)
		{
			appWindow.TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Tall);
		}

		// 异步设置图标
		SetIconAsync(appWindow);
	}
#pragma endregion


}
