#pragma once

#include "UI/Shell/NotifyIconContextMenu.g.h"

namespace winrt::OpenNet::UI::Shell::implementation
{
    struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu>
    {
        NotifyIconContextMenu();
    };
}

namespace winrt::OpenNet::UI::Shell::factory_implementation
{
    struct NotifyIconContextMenu : NotifyIconContextMenuT<NotifyIconContextMenu, implementation::NotifyIconContextMenu>
    {
    };
}
