#include "pch.h"
#include "NotifyIconContextMenu.xaml.h"
#if __has_include("UI/Shell/NotifyIconContextMenu.g.cpp")
#include "UI/Shell/NotifyIconContextMenu.g.cpp"
#endif

#include "App.xaml.h"
#include "Helpers/WindowHelper.h"
#include <winrt/WinUI3Package.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Storage.h>

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

	NotifyIconContextMenu::NotifyIconContextMenu()
	{
		InitializeComponent();

		m_trayIcon = winrt::WinUI3Package::TaskbarIcon();
		m_trayIcon.Guid(IconGuid());
		m_trayIcon.IconFile(Windows::ApplicationModel::Package::Current().InstalledLocation().Path() + L"\\Assets\\AppIcons\\StoreLogo.scale-150.png");
		m_trayIcon.ToolTip(L"OpenNet");
		m_trayIcon.LeftPressed([this]{ ShowMainWindow(); });

		// Create context menu
		MenuFlyout menuFlyout;

		MenuFlyoutItem showItem;
		showItem.Text(L"Show");
		showItem.Click([this](auto&&, auto&&) { ShowMainWindow(); });
		menuFlyout.Items().Append(showItem);

		menuFlyout.Items().Append(MenuFlyoutSeparator());

		MenuFlyoutItem exitItem;
		exitItem.Text(L"Exit");
		exitItem.Click([this](auto&&, auto&&) { ExitApplication(); });
		menuFlyout.Items().Append(exitItem);

		m_trayIcon.RightClickMenu(menuFlyout);
	}

	void NotifyIconContextMenu::Show()
	{
		if (m_trayIcon)
		{
			m_trayIcon.Show();
		}
	}

	void NotifyIconContextMenu::Remove()
	{
		if (m_trayIcon)
		{
			m_trayIcon.Remove();
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

		// Close the main window to trigger cleanup
		//auto window = winrt::OpenNet::implementation::App::window;
		//if (window)
		//{
		//	window.Close();
		//}

		// Exit the application
		Microsoft::UI::Xaml::Application::Current().Exit();
	}
}
