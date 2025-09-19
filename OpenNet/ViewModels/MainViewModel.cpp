#include "pch.h"
#include "MainViewModel.h"
#include "ViewModels/MainViewModel.g.cpp"

using namespace winrt;
using namespace Windows::Foundation;

namespace winrt::OpenNet::ViewModels::implementation
{
    // Summary: 构造函数，初始化默认状态和集合
    MainViewModel::MainViewModel()
        : m_statusText(L"初始化中 / Initializing")
        , m_isConnected(false)
        , m_appVersion(L"v0.1.0")
        , m_userName(L"Guest")
        , m_deviceName(L"This PC")
    {
        m_recentActivities = single_threaded_observable_vector<hstring>();
        m_recentActivities.Append(L"应用已启动 / App started");
    }

    // Summary: 初始化视图模型，设置就绪状态
    void MainViewModel::Initialize()
    {
        StatusText(L"就绪 / Ready");
    }

    // Summary: 更新状态文本并记录到活动列表
    // Param status: 要显示的新状态
    void MainViewModel::UpdateStatus(winrt::hstring const& status)
    {
        StatusText(status);
        if (m_recentActivities)
        {
            m_recentActivities.Append(status);
        }
    }
}