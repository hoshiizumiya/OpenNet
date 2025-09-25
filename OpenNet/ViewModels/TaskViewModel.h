#pragma once
#include "ViewModels/TaskViewModel.g.h"
#include "ViewModels/ObservableMixin.h"
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <string>

namespace winrt::OpenNet::ViewModels::implementation
{
    struct TaskViewModel : TaskViewModelT<TaskViewModel>, ::OpenNet::ViewModels::ObservableMixin<TaskViewModel>
    {
        TaskViewModel() = default;

        // Make mixin helpers visible
        using ::OpenNet::ViewModels::ObservableMixin<TaskViewModel>::SetProperty;
        using ::OpenNet::ViewModels::ObservableMixin<TaskViewModel>::RaisePropertyChanged;

        // Properties
        winrt::hstring Name() const { return m_name; }
        void Name(winrt::hstring const& v) { SetProperty(m_name, v, L"Name"); }

        winrt::hstring Size() const { return m_size; }
        void Size(winrt::hstring const& v) { SetProperty(m_size, v, L"Size"); }

        winrt::hstring Progress() const { return m_progress; }
        void Progress(winrt::hstring const& v) { SetProperty(m_progress, v, L"Progress"); }

        winrt::hstring DownloadRate() const { return m_downloadRate; }
        void DownloadRate(winrt::hstring const& v) { SetProperty(m_downloadRate, v, L"DownloadRate"); }

        winrt::hstring Remaining() const { return m_remaining; }
        void Remaining(winrt::hstring const& v) { SetProperty(m_remaining, v, L"Remaining"); }

    private:
        winrt::hstring m_name;
        winrt::hstring m_size;
        winrt::hstring m_progress;
        winrt::hstring m_downloadRate;
        winrt::hstring m_remaining;
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct TaskViewModel : TaskViewModelT<TaskViewModel, implementation::TaskViewModel>
    {
    };
}
