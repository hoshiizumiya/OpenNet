#pragma once

#include "UI/Shell/NotifyIconContextMenu.g.h"
#include <winrt/WinUI3Package.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace winrt::OpenNet::UI::Shell::implementation
{
	struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu>
	{
		NotifyIconContextMenu()
		{
			InitializeComponent();
			trayIcon().Guid(IconGuid());
		}

		// Static GUID for system tray icon
		static winrt::guid IconGuid();

		winrt::hstring Title();
		void CloseNotifyIconContextMenuWindowButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

		// Tray icon control
		void Show();
		void Remove();
		void ShowMainWindow();
		void ExitApplication();
		void HomeAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void ExitAppBarButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
	};
}

namespace winrt::OpenNet::UI::Shell::factory_implementation
{
	struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu, implementation::NotifyIconContextMenu>
	{
	};
}
