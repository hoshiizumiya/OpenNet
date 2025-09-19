#pragma once

#include "Pages/TasksPage.g.h"

namespace winrt::OpenNet::Pages::implementation
{
    struct TasksPage : TasksPageT<TasksPage>
    {
        TasksPage();
    };
}

namespace winrt::OpenNet::Pages::factory_implementation
{
    struct TasksPage : TasksPageT<TasksPage, implementation::TasksPage>
    {
    };
}
