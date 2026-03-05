#include "pch.h"
#include "MainViewModel.h"
#include "ViewModels/MainViewModel.g.cpp"
#include "Core/P2PManager.h"
#include "Core/DownloadManager.h"
#include "Core/NetworkDetector.h"

#include <chrono>
#include <format>

using namespace winrt;
using namespace Windows::Foundation;
using namespace std::chrono_literals;

namespace winrt::OpenNet::ViewModels::implementation
{
    // Summary: 构造函数，初始化默认状态和集合
    MainViewModel::MainViewModel()
        : m_statusText(L"初始化中 / Initializing"), m_isConnected(false), m_appVersion(L"v0.1.0"), m_userName(L"Guest"), m_portState(L"检测中")
    {
        m_dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        m_recentActivities = single_threaded_observable_vector<hstring>();
        m_recentActivities.Append(L"应用已启动 / App started");
    }

    MainViewModel::~MainViewModel()
    {
        m_stopSpeedRefresh.store(true);
        if (m_speedRefreshThread.joinable())
            m_speedRefreshThread.join();
    }

    // Summary: 初始化视图模型，设置就绪状态
    void MainViewModel::Initialize()
    {
        StatusText(L"就绪 / Ready");

        InitializeTorrentCore();

        // Start periodic speed refresh thread
        m_stopSpeedRefresh.store(false);
        m_speedRefreshThread = std::thread([this]()
                                           { SpeedRefreshThreadEntry(); });
    }

    // Summary: 更新状态文本并记录到活动列表
    // Param status: 要显示的新状态
    void MainViewModel::UpdateStatus(winrt::hstring const &status)
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
        auto dispatcher = m_dispatcher;
        UpdateStatus(L"初始化 P2P Core...");
        co_await ::OpenNet::Core::P2PManager::Instance().EnsureTorrentCoreInitializedAsync();
        if (::OpenNet::Core::P2PManager::Instance().IsTorrentCoreInitialized())
        {
            co_await wil::resume_foreground(dispatcher);
            UpdateStatus(L"P2P Core 已就绪");
        }
        else
        {
            co_await wil::resume_foreground(dispatcher);
            UpdateStatus(L"P2P Core 初始化失败");
        }
    }

    // Format bytes/sec to a human-readable string
    static std::wstring FormatSpeed(std::uint64_t bytesPerSec)
    {
        double kbs = bytesPerSec / 1024.0;
        if (kbs > 1024.0)
            return std::format(L"{:.1f} MB/s", kbs / 1024.0);
        else if (kbs > 1.0)
            return std::format(L"{:.0f} KB/s", kbs);
        else
            return std::format(L"{} B/s", bytesPerSec);
    }

    void MainViewModel::SpeedRefreshThreadEntry()
    {
        // Wait for TorrentCore to finish initializing (up to 30s)
        // before polling session stats, otherwise listenPort/dhtNodes will be 0.
        for (int i = 0; i < 60 && !m_stopSpeedRefresh.load(); ++i)
        {
            auto *core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
            if (core && core->IsRunning())
                break;
            std::this_thread::sleep_for(500ms);
        }

        while (!m_stopSpeedRefresh.load())
        {
            try
            {
                // Gather HTTP speeds from Aria2
                auto &dlMgr = ::OpenNet::Core::DownloadManager::Instance();
                std::uint64_t httpDl = dlMgr.TotalHttpDownloadSpeed();
                std::uint64_t httpUl = dlMgr.TotalHttpUploadSpeed();

                // Gather BT speeds from P2PManager
                std::uint64_t btDl = 0;
                std::uint64_t btUl = 0;
                int peersCount = 0;
                int dhtNodes = 0;
                int listenPort = 0;
                auto *torrentCore = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
                if (torrentCore && torrentCore->IsRunning())
                {
                    auto stats = torrentCore->GetSessionStats();
                    btDl = static_cast<std::uint64_t>(stats.totalDownloadRate);
                    btUl = static_cast<std::uint64_t>(stats.totalUploadRate);
                    peersCount = stats.numPeers;
                    dhtNodes = stats.dhtNodes;
                    listenPort = stats.listenPort;
                }

                std::uint64_t totalDl = httpDl + btDl;
                std::uint64_t totalUl = httpUl + btUl;

                auto speedText = std::format(L"\u2191 {} \u2193 {}",
                                             FormatSpeed(totalUl), FormatSpeed(totalDl));

                // Determine speed level for SwitchPresenter icon
                double dlMBps = totalDl / (1024.0 * 1024.0);
                std::wstring speedLevelText;
                if (dlMBps >= 10.0)
                    speedLevelText = L"High";
                else if (dlMBps >= 1.0)
                    speedLevelText = L"Medium";
                else
                    speedLevelText = L"Low";

                // Build port state string via actual port accessibility check (every 60s or on port change)
                auto now = std::chrono::steady_clock::now();
                bool needPortCheck = (listenPort > 0) &&
                    (listenPort != m_lastCheckedPort ||
                     std::chrono::duration_cast<std::chrono::seconds>(now - m_lastPortCheckTime).count() >= 60);
                if (needPortCheck)
                {
                    try
                    {
                        ::OpenNet::Core::NetworkDetector detector;
                        bool isOpen = detector.TestPortAccessibilityAsync(
                            static_cast<uint16_t>(listenPort), true).get();
                        m_cachedPortState = isOpen ? L"Open" : L"Blocked";
                    }
                    catch (...)
                    {
                        m_cachedPortState = L"Unknown";
                    }
                    m_lastCheckedPort = listenPort;
                    m_lastPortCheckTime = now;
                }
                std::wstring portStateText;
                if (listenPort > 0)
                    portStateText = m_cachedPortState;
                else
                    portStateText = L"Unknown";

                auto dispatcher = m_dispatcher;
                if (dispatcher)
                {
                    auto hspeed = winrt::hstring{speedText};
                    auto hport = winrt::hstring{portStateText};
                    auto hspeedLevel = winrt::hstring{speedLevelText};
                    auto peers = peersCount;
                    auto dht = dhtNodes;
                    auto port = listenPort;
                    dispatcher.TryEnqueue([this, hspeed, hport, hspeedLevel, peers, dht, port]()
                                          {
                        if (m_currentTransferSpeedText != hspeed)
                        {
                            m_currentTransferSpeedText = hspeed;
                            OnPropertyChanged(L"CurrentTransferSpeedText");
                        }
                        if (m_connectedPeersCount != peers)
                        {
                            m_connectedPeersCount = peers;
                            OnPropertyChanged(L"ConnectedPeersCount");
                        }
                        if (m_dhtNodeCount != dht)
                        {
                            m_dhtNodeCount = dht;
                            OnPropertyChanged(L"DhtNodeCount");
                        }
                        if (m_speedLevel != hspeedLevel)
                        {
                            m_speedLevel = hspeedLevel;
                            OnPropertyChanged(L"SpeedLevel");
                        }
                        if (m_listenPort != port)
                        {
                            m_listenPort = port;
                            OnPropertyChanged(L"ListenPort");
                        }
                        if (m_portState != hport)
                        {
                            m_portState = hport;
                            OnPropertyChanged(L"PortState");
                        } });
                }
            }
            catch (...)
            {
            }

            std::this_thread::sleep_for(1500ms);
        }
    }
}