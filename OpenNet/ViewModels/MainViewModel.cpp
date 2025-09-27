#include "pch.h"
#include "MainViewModel.h"
#include "ViewModels/MainViewModel.g.cpp"
#include "Core/P2PManager.h"

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
		, m_portState(L"检测中")
    {
        m_recentActivities = single_threaded_observable_vector<hstring>();
        m_recentActivities.Append(L"应用已启动 / App started");
    }

    // Summary: 初始化视图模型，设置就绪状态
    void MainViewModel::Initialize()
    {
        StatusText(L"就绪 / Ready");

        InitializeTorrentCore();
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

    IAsyncAction MainViewModel::InitializeTorrentCore()
    {
        // 使用单例管理 torrent 核心，异步后台初始化
        auto ui = winrt::apartment_context();
        UpdateStatus(L"初始化 P2P Core...");
        co_await ::OpenNet::Core::P2PManager::Instance().EnsureTorrentCoreInitializedAsync();
        if (::OpenNet::Core::P2PManager::Instance().IsTorrentCoreInitialized())
        {
            co_await ui;
            UpdateStatus(L"P2P Core 已就绪");
        }
        else
        {
            co_await ui;
            UpdateStatus(L"P2P Core 初始化失败");
        }
    }
}