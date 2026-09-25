#include "XamlWorkaround.h"
#include "WebUISettingsViewModel.h"
#include "ViewModels/WebUISettingsViewModel.g.cpp"

#include "Core/WebUI/WebUIControl.h"

import OpenNet.Core.AppSettingsDatabase;

namespace winrt::OpenNet::ViewModels::implementation
{
    void WebUISettingsViewModel::Load()
    {
        auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
        database.Initialize();

        Enabled(database.GetBool("webui_host", "enabled").value_or(true));
        ApiKey(winrt::to_hstring(
            database.GetString("webui_http", "api_key").value_or("")));
        RefreshRuntimeState();
    }

    void WebUISettingsViewModel::RefreshRuntimeState()
    {
        SetProperty(
            m_isRunning,
            ::OpenNet::Core::WebUI::IsWebUIRunning(),
            L"IsRunning");
    }
}
