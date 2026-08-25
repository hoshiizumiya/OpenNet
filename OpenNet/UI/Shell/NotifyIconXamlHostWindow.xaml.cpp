#include "XamlWorkaround.h"
#include "NotifyIconXamlHostWindow.xaml.h"
#if __has_include("UI/Shell/NotifyIconXamlHostWindow.g.cpp")
#include "UI/Shell/NotifyIconXamlHostWindow.g.cpp"
#endif

import OpenNet.Helpers.WindowHelper;
import OpenNet.App;

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Shell::implementation
{
	NotifyIconXamlHostWindow::NotifyIconXamlHostWindow()
	{
	}

	void NotifyIconXamlHostWindow::InitializeComponent()
	{
		NotifyIconXamlHostWindowT::InitializeComponent();
		InitializeWindowExBase(false, false);
		AppWindow().Hide();
		trayIcon().Guid(IconGuid());
	}

	// Static GUID for system tray icon - must be unique per application
	// {F8A9B3C7-2E4D-4F1A-9B8E-6C5D3A2B1E0F}
	winrt::guid NotifyIconXamlHostWindow::IconGuid()
	{
		return { 0xf8a9b3c7, 0x2e4d, 0x4f1a, { 0x9b, 0x8e, 0x6c, 0x5d, 0x3a, 0x2b, 0x1e, 0x0f } };
	}

	void NotifyIconXamlHostWindow::ShowMainWindow()
	{
		if (winrt::OpenNet::implementation::App::s_isExiting.load())
		{
			return;
		}
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::ShowMainWindow();
	}

	void NotifyIconXamlHostWindow::Show()
	{
		if (trayIcon() && !m_removed)
		{
			try
			{
				trayIcon().Show();
			}
			catch (...)
			{
			}
		}
	}

	void NotifyIconXamlHostWindow::Remove()
	{
		if (m_removed)
		{
			return;
		}
		m_removed = true;
		if (trayIcon())
		{
			try
			{
				trayIcon().Remove();
			}
			catch (...)
			{
			}
		}
	}
}
