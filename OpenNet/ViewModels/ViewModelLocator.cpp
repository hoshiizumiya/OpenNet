#include "pch.h"
#include "ViewModelLocator.h"

ViewModelLocator& ViewModelLocator::GetInstance()
{
    static ViewModelLocator s_locator;
    return s_locator;
}


//winrt::OpenNet::ViewModels::SettingsViewModel& ViewModelLocator::SettingsViewModel()
//{
//    if (!m_settingsViewModel)
//        m_settingsViewModel = winrt::OpenNet::ViewModels::SettingsViewModel();
//    return m_settingsViewModel;
//}

SpeedGraphData& ViewModelLocator::SpeedGraphData()
{
    return m_speedGraphData;
}