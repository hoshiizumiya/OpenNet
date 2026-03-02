#pragma once
#include "ViewModels/TaskViewModel.g.h"
#include "ViewModels/ObservableMixin.h"
#include "Core/DataGraph/SpeedGraphData.h"
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <string>

namespace winrt::OpenNet::ViewModels::implementation
{
    struct TaskViewModel : TaskViewModelT<TaskViewModel>, ::OpenNet::ViewModels::ObservableMixin<TaskViewModel>
    {
        TaskViewModel();

        // Make mixin helpers visible
        using ::OpenNet::ViewModels::ObservableMixin<TaskViewModel>::SetProperty;
        using ::OpenNet::ViewModels::ObservableMixin<TaskViewModel>::RaisePropertyChanged;

        // Properties
        winrt::hstring Name() const { return m_name; }
        void Name(winrt::hstring const &v) { SetProperty(m_name, v, L"Name"); }

        winrt::hstring Size() const { return m_size; }
        void Size(winrt::hstring const &v) { SetProperty(m_size, v, L"Size"); }

        winrt::hstring Progress() const { return m_progress; }
        void Progress(winrt::hstring const &v) { SetProperty(m_progress, v, L"Progress"); }

        winrt::hstring DownloadRate() const { return m_downloadRate; }
        void DownloadRate(winrt::hstring const &v) { SetProperty(m_downloadRate, v, L"DownloadRate"); }

        winrt::hstring UploadRate() const { return m_uploadRate; }
        void UploadRate(winrt::hstring const &v) { SetProperty(m_uploadRate, v, L"UploadRate"); }

        winrt::hstring Remaining() const { return m_remaining; }
        void Remaining(winrt::hstring const &v) { SetProperty(m_remaining, v, L"Remaining"); }

        winrt::hstring AddDate() const { return m_addDate; }
        void AddDate(winrt::hstring const &v) { SetProperty(m_addDate, v, L"AddDate"); }

        // Download task type
        winrt::OpenNet::ViewModels::DownloadTaskType TaskType() const { return m_taskType; }
        void TaskType(winrt::OpenNet::ViewModels::DownloadTaskType v) { SetProperty(m_taskType, v, L"TaskType"); }

        // Task identifier — libtorrent taskId or HTTP recordId
        winrt::hstring TaskId() const { return m_taskId; }
        void TaskId(winrt::hstring const &v) { SetProperty(m_taskId, v, L"TaskId"); }

        // Aria2 GID (only for HTTP tasks)
        winrt::hstring Gid() const { return m_gid; }
        void Gid(winrt::hstring const &v) { SetProperty(m_gid, v, L"Gid"); }

        // Numeric speed value for SpeedGraph
        uint64_t DownloadSpeedKB() const { return m_downloadSpeedKB; }
        void DownloadSpeedKB(uint64_t v) { SetProperty(m_downloadSpeedKB, v, L"DownloadSpeedKB"); }

        // Numeric progress percent for SpeedGraph
        double ProgressPercent() const { return m_progressPercent; }
        void ProgressPercent(double v) { SetProperty(m_progressPercent, v, L"ProgressPercent"); }

        // SpeedGraph points for visualization
        winrt::Microsoft::UI::Xaml::Media::PointCollection SpeedGraphPoints();

        // Update speed graph with new data
        void UpdateSpeedGraph(double percent, uint64_t speedKB);

    private:
        winrt::hstring m_name;
        winrt::hstring m_size;
        winrt::hstring m_progress;
        winrt::hstring m_downloadRate;
        winrt::hstring m_uploadRate;
        winrt::hstring m_remaining;
        winrt::hstring m_addDate;
        winrt::hstring m_gid;
        winrt::hstring m_taskId;
        winrt::OpenNet::ViewModels::DownloadTaskType m_taskType{winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent};
        uint64_t m_downloadSpeedKB{0};
        double m_progressPercent{0.0};
        SpeedGraphData m_speedGraphData;
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct TaskViewModel : TaskViewModelT<TaskViewModel, implementation::TaskViewModel>
    {
    };
}
