#pragma once

#include "UI/Shell/NotifyIconContextMenu.g.h"
#include <winrt/WinUI3Package.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace winrt::OpenNet::UI::Shell::implementation
{
    struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu>
    {
        NotifyIconContextMenu() = default;

        // Static GUID for system tray icon
        static winrt::guid IconGuid();

        // Tray icon control
        void Show();
        void Remove();
        void ShowMainWindow();
        void ExitApplication();
    };
}

namespace winrt::OpenNet::UI::Shell::factory_implementation
{
    struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu, implementation::NotifyIconContextMenu>
    {
    };
}
