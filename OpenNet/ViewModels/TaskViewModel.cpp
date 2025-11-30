#include "pch.h"
#include "ViewModels/TaskViewModel.h"
#include "ViewModels/TaskViewModel.g.cpp"

namespace winrt::OpenNet::ViewModels::implementation
{
    TaskViewModel::TaskViewModel()
    {
        // Initialize the speed graph data
    }

    winrt::Microsoft::UI::Xaml::Media::PointCollection TaskViewModel::SpeedGraphPoints()
    {
        return m_speedGraphData.Points();
    }

    void TaskViewModel::UpdateSpeedGraph(double percent, uint64_t speedKB)
    {
        m_speedGraphData.SetSpeed(percent, speedKB * 1024);  // Convert KB to bytes for internal representation
        RaisePropertyChanged(L"SpeedGraphPoints");
    }
}

