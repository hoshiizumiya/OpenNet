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

		// Create the TaskbarIcon programmatically
		m_trayIcon = winrt::WinUI3Package::TaskbarIcon();
		m_trayIcon.Guid(IconGuid());

		// Resolve icon path - TaskbarIcon requires actual file system path, not ms-appx:// URI
		try
		{
			auto packagePath = Windows::ApplicationModel::Package::Current().InstalledLocation().Path();
			auto iconPath = packagePath + L"\\Assets\\AppIcons\\Square44x44Logo.targetsize-32.png";
			m_trayIcon.IconFile(iconPath);
		}
		catch (...)
		{
			// Fallback: try using executable directory for unpackaged scenario
			wchar_t modulePath[MAX_PATH];
			if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) > 0)
			{
				std::wstring path(modulePath);
				auto lastSlash = path.rfind(L'\\');
				if (lastSlash != std::wstring::npos)
				{
					path = path.substr(0, lastSlash) + L"\\Assets\\AppIcons\\Square44x44Logo.targetsize-32.png";
					m_trayIcon.IconFile(path);
				}
			}
		}

		m_trayIcon.ToolTip(L"OpenNet");

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
		auto window = winrt::OpenNet::implementation::App::window;
		if (window)
		{
			window.Close();
		}

		// Exit the application
		Microsoft::UI::Xaml::Application::Current().Exit();
	}
}
