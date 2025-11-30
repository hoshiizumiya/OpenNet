#include "pch.h"
#include "Core/P2PManager.h"

using namespace winrt;
using namespace Windows::Foundation;

namespace OpenNet::Core
{
    P2PManager& P2PManager::Instance()
    {
        static P2PManager inst;
        return inst;
    }

    // 确保核心已经完成初始化
    IAsyncAction P2PManager::EnsureTorrentCoreInitializedAsync()
    {
        if (m_isTorrentCoreInitialized.load()) co_return;
        if (m_initializing.exchange(true))
        {
            while (!m_isTorrentCoreInitialized.load())
            {
                co_await winrt::resume_after(std::chrono::milliseconds(30));
            }
            co_return;
        }
        co_await winrt::resume_background();
        {
            std::scoped_lock lk(m_torrentMutex);
            
            // Initialize state manager first
            if (!m_stateManager)
            {
                m_stateManager = std::make_unique<OpenNet::Core::Torrent::TorrentStateManager>();
                if (!m_stateManager->Initialize())
                {
                    OutputDebugStringA("Failed to initialize TorrentStateManager\n");
                    // Continue anyway, persistence will just be disabled
                }
            }
            
            if (!m_torrentCore)
            {
                m_torrentCore = std::make_unique<OpenNet::Core::Torrent::LibtorrentHandle>();
                
                // Set state manager before initialization
                if (m_stateManager)
                {
                    m_torrentCore->SetStateManager(m_stateManager.get());
                }
                
                if (!m_torrentCore->Initialize())
                {
                    m_torrentCore.reset();
                    m_initializing.store(false);
                    co_return;
                }
                WireCoreCallbacks();
                m_torrentCore->Start();
            }
        }
        m_isTorrentCoreInitialized.store(true);
        m_initializing.store(false);
        
        // Load and resume saved tasks
        co_await LoadAndResumeSavedTasksAsync();
    }

    IAsyncOperation<bool> P2PManager::AddMagnetAsync(std::string magnetUri, std::string savePath)
    {
        co_await EnsureTorrentCoreInitializedAsync();
        std::scoped_lock lk(m_torrentMutex);
        if (!m_torrentCore) co_return false;
        co_return m_torrentCore->AddMagnet(magnetUri, savePath);
    }

    IAsyncAction P2PManager::LoadAndResumeSavedTasksAsync()
    {
        co_await winrt::resume_background();
        
        std::scoped_lock lk(m_torrentMutex);
        if (!m_stateManager || !m_torrentCore) co_return;

        auto tasks = m_stateManager->LoadAllTasks();
        for (auto const& task : tasks)
        {
            // Only resume non-completed, non-failed tasks
            if (task.status == 1 || task.status == 2) // Downloading or Paused
            {
                std::string resumedId = m_torrentCore->AddTorrentFromResumeData(task.taskId);
                if (!resumedId.empty())
                {
                    OutputDebugStringA(("Resumed task: " + task.taskId + "\n").c_str());
                }
            }
        }
    }

    std::vector<::OpenNet::Core::Torrent::TaskMetadata> P2PManager::GetAllTasks()
    {
        std::scoped_lock lk(m_torrentMutex);
        if (!m_stateManager) return {};
        return m_stateManager->LoadAllTasks();
    }

    IAsyncOperation<bool> P2PManager::ExportTasksAsync(std::wstring filePath)
    {
        co_await winrt::resume_background();
        std::scoped_lock lk(m_torrentMutex);
        if (!m_stateManager) co_return false;
        co_return m_stateManager->ExportToFile(filePath);
    }

    IAsyncOperation<bool> P2PManager::ImportTasksAsync(std::wstring filePath)
    {
        co_await winrt::resume_background();
        std::scoped_lock lk(m_torrentMutex);
        if (!m_stateManager) co_return false;
        bool result = m_stateManager->ImportFromFile(filePath);
        
        // Resume imported tasks
        if (result && m_torrentCore)
        {
            auto tasks = m_stateManager->LoadAllTasks();
            for (auto const& task : tasks)
            {
                if (task.status == 1 || task.status == 2)
                {
                    m_torrentCore->AddTorrentFromResumeData(task.taskId);
                }
            }
        }
        
        co_return result;
    }

    void P2PManager::SetProgressCallback(ProgressCb cb)
    {
        std::scoped_lock lk(m_cbMutex);
        m_progressCb = std::move(cb);
    }
    void P2PManager::SetFinishedCallback(FinishedCb cb)
    {
        std::scoped_lock lk(m_cbMutex);
        m_finishedCb = std::move(cb);
    }
    void P2PManager::SetErrorCallback(ErrorCb cb)
    {
        std::scoped_lock lk(m_cbMutex);
        m_errorCb = std::move(cb);
    }

    void P2PManager::WireCoreCallbacks()
    {
        if (!m_torrentCore) return;
        m_torrentCore->SetProgressCallback([this](const OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent& e)
        {
            std::scoped_lock lk(m_cbMutex);
            if (m_progressCb) m_progressCb(e);
        });
        m_torrentCore->SetFinishedCallback([this](const std::string& name)
        {
            std::scoped_lock lk(m_cbMutex);
            if (m_finishedCb) m_finishedCb(name);
        });
        m_torrentCore->SetErrorCallback([this](const std::string& err)
        {
            std::scoped_lock lk(m_cbMutex);
            if (m_errorCb) m_errorCb(err);
        });
    }
}
