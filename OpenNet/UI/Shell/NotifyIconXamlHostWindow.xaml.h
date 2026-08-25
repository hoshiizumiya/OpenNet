#pragma once

#include "UI/Shell/NotifyIconXamlHostWindow.g.h"

import OpenNet.Helpers.WindowExBase;

namespace winrt::OpenNet::UI::Shell::implementation
{
	struct NotifyIconXamlHostWindow : NotifyIconXamlHostWindowT<NotifyIconXamlHostWindow>, WindowExBase<NotifyIconXamlHostWindow>
	{
		NotifyIconXamlHostWindow();
		void InitializeComponent();
		// Static GUID for system tray icon
		static winrt::guid IconGuid();

		void ShowMainWindow();
		void Show();
		void Remove();

	private:
		bool m_removed{ false };
	};
}

namespace winrt::OpenNet::UI::Shell::factory_implementation
{
	struct NotifyIconXamlHostWindow : NotifyIconXamlHostWindowT<NotifyIconXamlHostWindow, implementation::NotifyIconXamlHostWindow>
	{
	};
}
