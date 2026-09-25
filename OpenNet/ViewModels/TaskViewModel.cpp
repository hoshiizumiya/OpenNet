#include <Windows.h>

#include "XamlWorkaround.h"
#include "ViewModels/TaskViewModel.h"
#include "ViewModels/TaskViewModel.g.cpp"
#include "Core/DataGraph/SpeedGraphDatabase.h"

namespace winrt::OpenNet::ViewModels::implementation
{
	TaskViewModel::TaskViewModel()
	{
		m_size = L"-";
		m_downloadSize = L"-";
		m_uploadSize = L"-";
		m_totalDownloadSize = L"-";
		m_totalUploadSize = L"-";
		m_remaining = L"-";
		m_completedDate = L"-";
		m_shareRatio = L"0.00";
		m_seeds = L"-";
		m_peers = L"-";
		// Initialize the speed graph data with a valid PointCollection
		// WinRT PointCollection defaults to nullptr; must be explicitly created
		(void)m_speedGraphData.Points(); // Force initialization
	}

	winrt::Microsoft::UI::Xaml::Media::PointCollection TaskViewModel::SpeedGraphPoints()
	{
		return m_speedGraphData.Points();
	}

	void TaskViewModel::UpdateSpeedGraph(double percent, uint64_t speedKB)
	{
		// Persistence must not depend on the graph/UI update succeeding.
		// Store one sample for each integer progress boundary (0-100).
		auto const intPercent = static_cast<int>(percent);
		if (intPercent > m_lastSavedPercent && intPercent <= 100)
		{
			m_lastSavedPercent = intPercent;
			auto const taskId = winrt::to_string(m_taskId);
			if (!taskId.empty())
			{
				::OpenNet::Core::SpeedGraphDatabase::Instance().SavePoint(taskId, intPercent, speedKB);
			}
		}

		try
		{
			auto& points = m_speedGraphData.Points();
			auto const pointCount = points.Size();
			(void)m_speedGraphData.SetSpeed(
				percent, speedKB * 1024);  // Convert KB to bytes for internal representation
			if (points.Size() != pointCount)
			{
				RaisePropertyChanged(L"SpeedGraphPoints");
			}
		}
		catch (...)
		{
			// The persisted history remains valid even if graph projection/update fails.
			OutputDebugStringA("TaskViewModel::UpdateSpeedGraph: graph update exception caught\n");
		}
	}
}

