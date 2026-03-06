#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "UI/Xaml/View/Pages/MainView.xaml.h"
#include "Helpers/WindowHelper.h"
#include "winrt/microsoft.ui.interop.h"
#include <wil/resource.h>
#include <resource.h>
#include <windows.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/WinUI3Package.h>

#include "Core/P2PManager.h"
#include "Core/DownloadManager.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Windowing;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace ::OpenNet::Helpers::WinUIWindowHelper;

namespace winrt::OpenNet::implementation
{
	MainWindow::MainWindow()
	{
		InitializeComponent();
		SetTitleBar(AppTitleBar());
		InitWindowStyle(*this);

		AppWindow().SetIcon(L"Assets/AppIcons/win3264.ico");

		// Listen for back-button state changes from MainView
		MainContentView().CanGoBackChanged([this](IInspectable const&, bool canGoBack)
		{
			AppTitleBar().IsBackButtonVisible(canGoBack);
		});

		Closed([this](auto&&, auto&&)
		{
			PlacementRestoration::Save(*this);

			// Stop ViewModel background thread before tearing down engines
			try
			{
				if (auto vm = ViewModel())
					vm.Shutdown();
			}
			catch (...) { OutputDebugStringA("MainWindow: ViewModel shutdown error\n"); }

			// Gracefully shut down download engines
			try { ::OpenNet::Core::P2PManager::Instance().Shutdown(); }
			catch (...) { OutputDebugStringA("MainWindow: P2PManager shutdown error\n"); }

			// DownloadManager::Shutdown() blocks on async — run on background thread
			try
			{
				std::thread bgShutdown([]()
				{
					try { ::OpenNet::Core::DownloadManager::Instance().Shutdown(); }
					catch (...) { OutputDebugStringA("MainWindow: DownloadManager shutdown error (bg)\n"); }
				});
				bgShutdown.join();
			}
			catch (...) { OutputDebugStringA("MainWindow: DownloadManager background shutdown error\n"); }
		});
	}

	winrt::OpenNet::ViewModels::MainViewModel MainWindow::ViewModel()
	{
		return MainContentView().ViewModel();
	}

	void MainWindow::Navigate(hstring const& tag)
	{
		MainContentView().Navigate(tag);
	}

	void MainWindow::InitWindowStyle(Window const& window)
	{
		window.ExtendsContentIntoTitleBar(true);
		if (auto appWindow = window.AppWindow())
		{
			appWindow.TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Tall);
			PlacementRestoration::Enable(*this);
#ifdef _DEBUG
			{
				AppTitleBar().Subtitle(L"Dev");
			}
#endif
		}
	}

	void MainWindow::AppTitleBar_BackRequested(Microsoft::UI::Xaml::Controls::TitleBar const&, IInspectable const&)
	{
		MainContentView().GoBack();
	}

	void MainWindow::Grid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);

		if (auto rootGrid = sender.try_as<FrameworkElement>())
		{
			if (auto xamlRoot = rootGrid.XamlRoot())
			{
				xamlRoot.Changed({ this, &MainWindow::RootGridXamlRoot_Changed });
			}
		}
	}

	void MainWindow::RootGridXamlRoot_Changed(XamlRoot /*sender*/, XamlRootChangedEventArgs /*args*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);
	}
}
