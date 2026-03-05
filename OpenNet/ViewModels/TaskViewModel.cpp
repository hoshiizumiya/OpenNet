#include "pch.h"
#include "ViewModels/TaskViewModel.h"
#include "ViewModels/TaskViewModel.g.cpp"
#include "Core/DataGraph/SpeedGraphDatabase.h"

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

            // Persist at each 1% boundary
            int intPercent = static_cast<int>(percent);
            if (intPercent > m_lastSavedPercent && intPercent <= 100)
            {
                m_lastSavedPercent = intPercent;
                auto taskId = winrt::to_string(m_taskId);
                if (!taskId.empty())
                {
                    ::OpenNet::Core::SpeedGraphDatabase::Instance().SavePoint(taskId, intPercent, speedKB);
                }
            }
        }
        catch (...)
        {
            // Prevent crash propagation through WinRT ABI boundary
            OutputDebugStringA("TaskViewModel::UpdateSpeedGraph: exception caught\n");
        }
    }
}

