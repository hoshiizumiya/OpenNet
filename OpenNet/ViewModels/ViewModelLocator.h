#pragma once
#include "SettingsViewModel.h"
#include "ViewModels/SettingsViewModel.g.h"
#include "Core/DataGraph/SpeedGraphData.h"

class ViewModelLocator
{
	winrt::OpenNet::ViewModels::SettingsViewModel m_settingsViewModel{ nullptr };
	SpeedGraphData m_speedGraphData;
public:
	static ViewModelLocator& GetInstance();

	winrt::OpenNet::ViewModels::SettingsViewModel& SettingsViewModel();
	SpeedGraphData& SpeedGraphData();
};
