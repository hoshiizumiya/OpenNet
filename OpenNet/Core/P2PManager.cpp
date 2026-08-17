module;
#include <Windows.h>

module OpenNet.Core.P2PManager;

import OpenNet.Core.Torrent.TrackerManager;

using namespace winrt;
using namespace Windows::Foundation;

namespace OpenNet::Core
{
	P2PManager& P2PManager::Instance()
	{
		static P2PManager inst;
		return inst;
	}

	::OpenNet::Core::Torrent::LibtorrentHandle::SessionStats P2PManager::GetSessionStats()
	{
		std::scoped_lock lock(m_torrentMutex);
		return m_torrentCore
			? m_torrentCore->GetSessionStats()
			: ::OpenNet::Core::Torrent::LibtorrentHandle::SessionStats{};
	}

	::OpenNet::Core::Torrent::LibtorrentHandle::SessionStats
		P2PManager::GetPerformanceStats()
	{
		std::scoped_lock lock(m_torrentMutex);
		return m_torrentCore
			? m_torrentCore->GetPerformanceStats()
			: ::OpenNet::Core::Torrent::LibtorrentHandle::SessionStats{};
	}

	std::vector<::OpenNet::Core::Torrent::LibtorrentHandle::TorrentPeerInfo>
		P2PManager::GetTorrentPeers(std::string const& taskId)
	{
		std::scoped_lock lock(m_torrentMutex);
		return m_torrentCore
			? m_torrentCore->GetTorrentPeers(taskId)
			: std::vector<::OpenNet::Core::Torrent::LibtorrentHandle::TorrentPeerInfo>{};
	}

	// 确保核心已经完成初始化
	IAsyncAction P2PManager::EnsureTorrentCoreInitializedAsync()
	{
		std::shared_ptr<std::promise<void>> completionSource;
		std::shared_future<void> completion;
		bool ownsInitialization = false;

		{
			std::scoped_lock lifecycleLock(m_lifecycleMutex);
			switch (m_initializationState)
			{
				case InitializationState::Initialized:
					co_return;
				case InitializationState::ShuttingDown:
					throw winrt::hresult_error(RO_E_CLOSED, L"The torrent core is shutting down.");
				case InitializationState::Initializing:
					completion = m_initializationCompletion;
					break;
				case InitializationState::Uninitialized:
					completionSource = std::make_shared<std::promise<void>>();
					completion = completionSource->get_future().share();
					m_initializationCompletion = completion;
					m_initializationState = InitializationState::Initializing;
					ownsInitialization = true;
					break;
			}
		}

		co_await winrt::resume_background();

		if (!ownsInitialization)
		{
			// Propagate the initializer's result to every concurrent caller.
			completion.get();
			co_return;
		}

		bool createdCore = false;
		try
		{
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
					createdCore = true;

					// Set state manager before initialization
					if (m_stateManager)
					{
						m_torrentCore->SetStateManager(m_stateManager.get());
					}

					if (!m_torrentCore->Initialize())
					{
						throw winrt::hresult_error(E_FAIL, L"Failed to initialize the libtorrent session.");
					}
					WireCoreCallbacks();
					m_torrentCore->Start();
					if (!m_torrentCore->IsRunning())
					{
						throw winrt::hresult_error(E_FAIL, L"Failed to start the libtorrent alert loop.");
					}
					ProbePortAfterStartupAsync();
				}
			}

			{
				std::scoped_lock lifecycleLock(m_lifecycleMutex);
				if (m_initializationState == InitializationState::ShuttingDown)
				{
					throw winrt::hresult_error(RO_E_CLOSED, L"The torrent core was shut down during initialization.");
				}
				m_isTorrentCoreInitialized.store(true);
				m_initializationState = InitializationState::Initialized;
			}

			completionSource->set_value();
		}
		catch (...)
		{
			auto initializationError = std::current_exception();

			if (createdCore)
			{
				std::scoped_lock lk(m_torrentMutex);
				if (m_torrentCore)
				{
					m_torrentCore->Stop();
					m_torrentCore.reset();
				}
			}

			{
				std::scoped_lock lifecycleLock(m_lifecycleMutex);
				m_isTorrentCoreInitialized.store(false);
				if (m_initializationState != InitializationState::ShuttingDown)
				{
					// A later user action may start a fresh attempt. Existing
					// waiters still hold this attempt's failed shared future.
					m_initializationState = InitializationState::Uninitialized;
				}
			}

			completionSource->set_exception(initializationError);
			std::rethrow_exception(initializationError);
		}

		// Core readiness is independent from restoring persisted tasks.
		try
		{
			co_await LoadAndResumeSavedTasksAsync();
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("P2PManager: Failed to restore saved tasks: " + std::string(ex.what()) + "\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("P2PManager: Unknown error while restoring saved tasks\n");
		}
	}

	winrt::fire_and_forget P2PManager::ProbePortAfterStartupAsync()
	{
		try
		{
			// Give the freshly-created session time to bind its configured
			// interfaces, then explicitly reopen/probe sockets and port mappings.
			// This also recovers sessions which initially reported listen port 0.
			co_await winrt::resume_after(std::chrono::seconds(2));

			std::scoped_lock lk(m_torrentMutex);
			if (m_torrentCore && m_torrentCore->IsRunning())
			{
				m_torrentCore->RefreshPortMappings();
				OutputDebugStringA("P2PManager: Startup listen-port probe requested\n");
			}
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA((
				"P2PManager: Startup listen-port probe failed: " +
				std::string(ex.what()) + "\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("P2PManager: Startup listen-port probe failed\n");
		}
	}

	IAsyncOperation<bool> P2PManager::AddMagnetAsync(
		std::string magnetUri,
		std::string savePath,
		std::vector<int> const& filePriorities,
		std::vector<std::string> const& extraTrackers,
		bool startImmediately)
	{
		co_await ::OpenNet::Core::Torrent::TrackerManager::Instance()
			.InitializeAsync();
		co_await EnsureTorrentCoreInitializedAsync();
		std::scoped_lock lk(m_torrentMutex);
		if (!m_torrentCore) co_return false;
		co_return m_torrentCore->AddMagnet(
			magnetUri, savePath, filePriorities, extraTrackers, startImmediately);
	}

	IAsyncOperation<bool> P2PManager::AddTorrentFileAsync(
		std::string torrentFilePath,
		std::string savePath,
		std::vector<int> const& filePriorities,
		std::vector<std::string> const& extraTrackers,
		bool startImmediately)
	{
		co_await ::OpenNet::Core::Torrent::TrackerManager::Instance()
			.InitializeAsync();
		co_await EnsureTorrentCoreInitializedAsync();
		std::scoped_lock lk(m_torrentMutex);
		if (!m_torrentCore) co_return false;
		co_return m_torrentCore->AddTorrentFile(
			torrentFilePath, savePath, filePriorities, extraTrackers, startImmediately);
	}

	IAsyncAction P2PManager::LoadAndResumeSavedTasksAsync()
	{
		co_await winrt::resume_background();

		std::scoped_lock lk(m_torrentMutex);
		if (!m_stateManager || !m_torrentCore) co_return;

		auto tasks = m_stateManager->LoadAllTasks();
		for (auto const& task : tasks)
		{
			// Load all active tasks (downloading, paused, completed) into the session.
			// Paused/completed torrents are restored in their correct state via resume data flags.
			// Without loading them, "Resume" button won't work for paused tasks
			// since they wouldn't exist in the libtorrent session.
			if (task.status == 1 || task.status == 2 || task.status == 3)
			{
				std::string resumedId = m_torrentCore->AddTorrentFromResumeData(task.taskId);
				if (!resumedId.empty())
				{
					OutputDebugStringA(("Resumed task: " + task.taskId + " (status=" + std::to_string(task.status) + ")\n").c_str());

					// Ensure paused and completed tasks stay stopped even when
					// older resume data did not preserve that state.
					if (task.status == 2 || task.status == 3)
					{
						m_torrentCore->PauseTorrent(task.taskId);
						if (task.status == 3 && m_stateManager)
						{
							m_stateManager->UpdateTaskStatus(task.taskId, 3);
						}
					}
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
		m_torrentCore->SetProgressCallback([this](const ::OpenNet::Core::Torrent::LibtorrentHandle::ProgressEvent& e)
		{
			std::scoped_lock lk(m_cbMutex);
			if (m_progressCb) m_progressCb(e);
		});
		m_torrentCore->SetFinishedCallback([this](const std::string& taskId, const std::string& name)
		{
			std::scoped_lock lk(m_cbMutex);
			if (m_finishedCb) m_finishedCb(taskId, name);
		});
		m_torrentCore->SetErrorCallback([this](const std::string& err)
		{
			std::scoped_lock lk(m_cbMutex);
			if (m_errorCb) m_errorCb(err);
		});
	}

	void P2PManager::Shutdown()
	{
		// 这个方法需要安全地关闭torrent核心
		try
		{
			{
				std::scoped_lock lifecycleLock(m_lifecycleMutex);
				if (m_initializationState == InitializationState::ShuttingDown)
				{
					return;
				}
				m_initializationState = InitializationState::ShuttingDown;
				m_isTorrentCoreInitialized.store(false);
			}

			std::scoped_lock lk(m_torrentMutex);

			OutputDebugStringA("P2PManager: Shutting down...\n");

			// Save all resume data then stop the core.
			// Stop() internally waits for pending resume data alerts
			// and saves session state before destroying the session.
			if (m_torrentCore)
			{
				OutputDebugStringA("P2PManager: Saving all resume data...\n");
				m_torrentCore->SaveAllResumeData();

				// Stop core (waits for resume data alerts, saves session state)
				OutputDebugStringA("P2PManager: Stopping torrent core...\n");
				m_torrentCore->Stop();
			}

			// 清空回调以避免在shutdown期间调用它们
			{
				std::scoped_lock cbLk(m_cbMutex);
				m_progressCb = nullptr;
				m_finishedCb = nullptr;
				m_errorCb = nullptr;
			}

			// 释放torrent核心资源
			m_torrentCore.reset();

			// 可选：保存状态管理器数据
			if (m_stateManager)
			{
				// 状态管理器通常会自己处理持久化
			}

			OutputDebugStringA("P2PManager: Shutdown completed successfully\n");
		}
		catch (const std::exception& ex)
		{
			OutputDebugStringW((L"P2PManager: Shutdown error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("P2PManager: Unknown error during shutdown\n");
		}
	}
}
