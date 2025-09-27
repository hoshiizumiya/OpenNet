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
            if (!m_torrentCore)
            {
                m_torrentCore = std::make_unique<OpenNet::Core::Torrent::LibtorrentHandle>();
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
    }

    IAsyncOperation<bool> P2PManager::AddMagnetAsync(std::string magnetUri, std::string savePath)
    {
        co_await EnsureTorrentCoreInitializedAsync();
        std::scoped_lock lk(m_torrentMutex);
        if (!m_torrentCore) co_return false;
        co_return m_torrentCore->AddMagnet(magnetUri, savePath);
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
