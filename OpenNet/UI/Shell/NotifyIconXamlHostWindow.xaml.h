#pragma once

#include "UI/Shell/NotifyIconXamlHostWindow.g.h"

namespace winrt::OpenNet::UI::Shell::implementation
{
    struct NotifyIconXamlHostWindow : NotifyIconXamlHostWindowT<NotifyIconXamlHostWindow>
    {
        NotifyIconXamlHostWindow();
		// Static GUID for system tray icon
		static winrt::guid IconGuid();

		void ShowMainWindow();
		void Show();
		void Remove();
    };
}

namespace winrt::OpenNet::UI::Shell::factory_implementation
{
    struct NotifyIconXamlHostWindow : NotifyIconXamlHostWindowT<NotifyIconXamlHostWindow, implementation::NotifyIconXamlHostWindow>
    {
    };
}
