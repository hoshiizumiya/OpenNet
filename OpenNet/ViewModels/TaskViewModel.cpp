#include <Windows.h>

#include "XamlWorkaround.h"
#include "ViewModels/TaskViewModel.h"
#include "ViewModels/TaskViewModel.g.cpp"
#include <unordered_set>

import OpenNet.Core.AppSettingsDatabase;

namespace
{
	std::unordered_set<winrt::OpenNet::ViewModels::implementation::TaskViewModel*> s_progressItems;
	bool s_progressAppearanceLoaded{};
	bool s_progressEffectsEnabled{ true };
	int s_progressEffectScope{};
	winrt::Windows::UI::Color s_downloadColor{ 255, 76, 203, 137 };
	winrt::Windows::UI::Color s_seedingColor{ 255, 240, 82, 96 };
	winrt::Windows::UI::Color s_checkingColor{ 255, 69, 201, 232 };
	winrt::Windows::UI::Color s_baseColor{};

	winrt::Windows::UI::Color ReadColor(::OpenNet::Core::AppSettingsDatabase& db, char const* key, winrt::Windows::UI::Color fallback)
	{
		auto const argb = static_cast<std::uint32_t>(db.GetInt("ui", key)
													 .value_or((std::uint32_t(fallback.A) << 24) | (std::uint32_t(fallback.R) << 16)
															   | (std::uint32_t(fallback.G) << 8) | fallback.B));
		return { static_cast<std::uint8_t>(argb >> 24), static_cast<std::uint8_t>(argb >> 16),
			static_cast<std::uint8_t>(argb >> 8), static_cast<std::uint8_t>(argb) };
	}
}

namespace winrt::OpenNet::ViewModels::implementation
{
	TaskViewModel::TaskViewModel()
	{
		if (!s_progressAppearanceLoaded) ReloadProgressAppearance();
		s_progressItems.insert(this);
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
		m_transport = L"-";
		// Initialize the speed graph data with a valid PointCollection
		// WinRT PointCollection defaults to nullptr; must be explicitly created
		(void)m_speedGraphData.Points(); // Force initialization
	}

	TaskViewModel::~TaskViewModel()
	{
		s_progressItems.erase(this);
	}

	void TaskViewModel::ReloadProgressAppearance()
	{
		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		db.Initialize();
		s_progressEffectsEnabled = db.GetBool("ui", "task_progress_effects_enabled").value_or(true);
		s_progressEffectScope = std::clamp(static_cast<int>(db.GetInt("ui", "task_progress_effect_scope").value_or(0)), 0, 1);
		s_downloadColor = ReadColor(db, "task_progress_download_color", { 255, 76, 203, 137 });
		s_seedingColor = ReadColor(db, "task_progress_seeding_color", { 255, 240, 82, 96 });
		s_checkingColor = ReadColor(db, "task_progress_checking_color", { 255, 69, 201, 232 });
		s_baseColor = ReadColor(db, "task_progress_base_color", {});
		s_progressAppearanceLoaded = true;
		for (auto* item : s_progressItems)
		{
			item->RaisePropertyChanged(L"ProgressHighColor");
			item->RaisePropertyChanged(L"ProgressBaseColor");
			item->RaisePropertyChanged(L"ShowProgressEffect");
			item->RaisePropertyChanged(L"ShowProgressColumnEffect");
			item->RaisePropertyChanged(L"ShowProgressColumnText");
			item->RaisePropertyChanged(L"ShowProgressRowEffect");
		}
	}

	winrt::Windows::UI::Color TaskViewModel::ProgressHighColor() const
	{
		if (m_state == DownloadTaskState::Checking) return s_checkingColor;
		if (m_taskType == DownloadTaskType::BitTorrent && m_state == DownloadTaskState::Seeding) return s_seedingColor;
		return s_downloadColor;
	}

	bool TaskViewModel::ShowProgressEffect() const
	{
		bool const active = m_state == DownloadTaskState::Downloading
			|| m_state == DownloadTaskState::Checking
			|| (m_taskType == DownloadTaskType::BitTorrent && m_state == DownloadTaskState::Seeding);
		return s_progressEffectsEnabled && active && !m_targetPathMissing;
	}

	winrt::Windows::UI::Color TaskViewModel::ProgressBaseColor() const
	{
		return s_baseColor;
	}

	bool TaskViewModel::ShowProgressColumnEffect() const
	{
		return s_progressEffectScope == 0 && ShowProgressEffect();
	}

	bool TaskViewModel::ShowProgressColumnText() const
	{
		return !ShowProgressColumnEffect();
	}

	bool TaskViewModel::ShowProgressRowEffect() const
	{
		return s_progressEffectScope == 1 && ShowProgressEffect();
	}

	winrt::Microsoft::UI::Xaml::Media::PointCollection TaskViewModel::SpeedGraphPoints()
	{
		return m_speedGraphData.Points();
	}

	void TaskViewModel::UpdateSpeedGraph(double percent, uint64_t speedKB)
	{
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
			// Core persistence is independent from this UI projection.
			OutputDebugStringA("TaskViewModel::UpdateSpeedGraph: graph update exception caught\n");
		}
	}
}
