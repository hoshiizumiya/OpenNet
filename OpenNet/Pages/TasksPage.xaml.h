#pragma once

#include "Pages/TasksPage.g.h"
#include "ViewModels/TasksViewModel.h"

namespace winrt::OpenNet::Pages::implementation
{
    struct TasksPage : TasksPageT<TasksPage>
    {
        TasksPage();

        // Expose strongly-typed ViewModel for x:Bind
        winrt::OpenNet::ViewModels::TasksViewModel ViewModel() const { return m_viewModel; }

    private:
        winrt::OpenNet::ViewModels::TasksViewModel m_viewModel{ nullptr };
    };
}

namespace winrt::OpenNet::Pages::factory_implementation
{
    struct TasksPage : TasksPageT<TasksPage, implementation::TasksPage>
    {
    };
}
