#include "pch.h"
#include "NotifyIconContextMenu.xaml.h"
#if __has_include("UI/Shell/NotifyIconContextMenu.g.cpp")
#include "UI/Shell/NotifyIconContextMenu.g.cpp"
#endif

#include "App.xaml.h"
#include "Core/AppRuntime.h"
#include "Helpers/WindowHelper.h"
#include <winrt/WinUI3Package.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Shell::implementation
{
	// Static GUID for system tray icon - must be unique per application
	// {F8A9B3C7-2E4D-4F1A-9B8E-6C5D3A2B1E0F}
	winrt::guid NotifyIconContextMenu::IconGuid()
	{
		return { 0xf8a9b3c7, 0x2e4d, 0x4f1a, { 0x9b, 0x8e, 0x6c, 0x5d, 0x3a, 0x2b, 0x1e, 0x0f } };
	}

	winrt::hstring NotifyIconContextMenu::Title()
	{
		return ::OpenNet::Core::AppRuntime::GetDisplayName();
	}

	void NotifyIconContextMenu::CloseNotifyIconContextMenuWindowButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{

	}

	void NotifyIconContextMenu::Show()
	{
		if (trayIcon())
		{
			trayIcon().Show();
		}
	}

	void NotifyIconContextMenu::Remove()
	{
		if (trayIcon())
		{
			trayIcon().Remove();
		}
	}

	void NotifyIconContextMenu::ShowMainWindow()
	{
		auto window = winrt::OpenNet::implementation::App::window;
		if (window)
		{
			window.AppWindow().Show();

			HWND hwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(window);
			if (hwnd)
			{
				if (IsIconic(hwnd))
				{
					ShowWindow(hwnd, SW_RESTORE);
				}
				SetForegroundWindow(hwnd);
				SetFocus(hwnd);
			}
		}
	}

	void NotifyIconContextMenu::ExitApplication()
	{
		// Remove the tray icon
		Remove();

		// Allow the window to close (bypasses the hide-to-tray Closing handler)
		winrt::OpenNet::implementation::App::s_isExiting = true;
		// For test now
		auto window = winrt::OpenNet::implementation::App::window;
		if (window)
		{
			window.Close();
		}

		// Exit the application - now the Closing handler will not cancel the close,
		// the window closes properly, App::~App() runs, and all services shut down.
	}

	void NotifyIconContextMenu::HomeAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		ShowMainWindow();
	}

	void NotifyIconContextMenu::ExitAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		//auto app = winrt::OpenNet::App();
		//app.Exit();
		Microsoft::UI::Xaml::Application::Current().Exit();
		//ExitProcess(0);

	}
}
