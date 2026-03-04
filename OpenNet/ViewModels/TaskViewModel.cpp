#include "pch.h"
#include "ViewModels/TaskViewModel.h"
#include "ViewModels/TaskViewModel.g.cpp"

namespace winrt::OpenNet::ViewModels::implementation
{
    TaskViewModel::TaskViewModel()
    {
        // Initialize the speed graph data with a valid PointCollection
        // WinRT PointCollection defaults to nullptr; must be explicitly created
        m_speedGraphData.Points(); // Force initialization
    }

    winrt::Microsoft::UI::Xaml::Media::PointCollection TaskViewModel::SpeedGraphPoints()
    {
        return m_speedGraphData.Points();
    }

    void TaskViewModel::UpdateSpeedGraph(double percent, uint64_t speedKB)
    {
        try
        {
            m_speedGraphData.SetSpeed(percent, speedKB * 1024);  // Convert KB to bytes for internal representation
            RaisePropertyChanged(L"SpeedGraphPoints");
        }
        catch (...)
        {
            // Prevent crash propagation through WinRT ABI boundary
            OutputDebugStringA("TaskViewModel::UpdateSpeedGraph: exception caught\n");
        }
    }
}

