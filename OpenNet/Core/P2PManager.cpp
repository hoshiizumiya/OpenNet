module;
#include <Windows.h>

module OpenNet.Core.P2PManager;

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Content.CanonicalV2Swarm;
import OpenNet.Core.Content.ContentDirectoryClient;
import OpenNet.Core.Torrent.TrackerManager;
import OpenNet.Core.TorrentSettings;

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

	::OpenNet::Core::Torrent::LibtorrentHandle::ListenStatus
		P2PManager::GetListenStatus()
	{
		std::scoped_lock lock(m_torrentMutex);
		return m_torrentCore
			? m_torrentCore->GetListenStatus()
			: ::OpenNet::Core::Torrent::LibtorrentHandle::ListenStatus{};
	}

	::OpenNet::Core::Torrent::LibtorrentHandle::LongSeedSessionResult
		P2PManager::OpenLongSeedSession(
			std::string const& sessionId,
			std::vector<std::uint8_t> const& metainfo,
			std::filesystem::path const& localFilePath)
	{
		std::scoped_lock lock(m_torrentMutex);
		return m_torrentCore
			? m_torrentCore->OpenLongSeedSession(
				sessionId, metainfo, localFilePath)
			: ::OpenNet::Core::Torrent::LibtorrentHandle::LongSeedSessionResult{
				false, {}, "Torrent core is unavailable" };
	}

	::OpenNet::Core::Torrent::LibtorrentHandle::LongSeedSessionResult
		P2PManager::OpenLongSeedDownloadSession(
			std::string const& sessionId,
			std::vector<std::uint8_t> const& metainfo,
			std::filesystem::path const& targetFilePath,
			std::vector<std::string> const& urlSeeds)
	{
		std::scoped_lock lock(m_torrentMutex);
		return m_torrentCore
			? m_torrentCore->OpenLongSeedDownloadSession(
				sessionId, metainfo, targetFilePath, urlSeeds)
			: ::OpenNet::Core::Torrent::LibtorrentHandle::LongSeedSessionResult{
				false, {}, "Torrent core is unavailable" };
	}

	bool P2PManager::ConnectLongSeedPeer(
		std::string const& sessionId,
		std::string const& address,
		std::uint16_t port,
		bool preferUtp)
	{
		std::scoped_lock lock(m_torrentMutex);
		return m_torrentCore
			&& m_torrentCore->ConnectLongSeedPeer(
				sessionId, address, port, preferUtp);
	}

	::OpenNet::Core::Torrent::LibtorrentHandle::LongSeedSessionStatus
		P2PManager::GetLongSeedSessionStatus(
			std::string const& sessionId)
	{
		std::scoped_lock lock(m_torrentMutex);
		return m_torrentCore
			? m_torrentCore->GetLongSeedSessionStatus(sessionId)
			: ::OpenNet::Core::Torrent::LibtorrentHandle::LongSeedSessionStatus{};
	}

	void P2PManager::CloseLongSeedSession(std::string const& sessionId)
	{
		std::scoped_lock lock(m_torrentMutex);
		if (m_torrentCore)
			m_torrentCore->CloseLongSeedSession(sessionId);
	}

	winrt::Windows::Foundation::IAsyncOperation<bool>
		P2PManager::StartLongSeedDownloadAsync(
			::OpenNet::Core::Content::ContentIdentity identity,
			std::filesystem::path targetFilePath,
			std::uint32_t maxPeers,
			std::string sessionId,
			std::vector<std::string> urlSeeds)
	{
		if (!identity.IsWellFormed()
			|| identity.algorithm
				!= ::OpenNet::Core::Content::ContentIdentityAlgorithm::Bep52FileRootSha256
			|| targetFilePath.empty())
			co_return false;

		co_await EnsureTorrentCoreInitializedAsync();
		co_await winrt::resume_background();

		::OpenNet::Core::Content::ContentDirectoryClient client;
		std::optional<::OpenNet::Core::Content::ContentLookupResult> lookup;
		auto& settingsDb = ::OpenNet::Core::AppSettingsDatabase::Instance();
		settingsDb.Initialize();
		auto const selfNodeId = settingsDb.GetString(
			"content_directory", "node_id");
		auto const deadline =
			std::chrono::steady_clock::now() + std::chrono::seconds(12);

		do
		{
			lookup = client.Lookup(identity, selfNodeId, maxPeers, true);
			if (!lookup) co_return false;

			bool const hasReadyPeer = std::ranges::any_of(
				lookup->peers,
				[](auto const& peer) { return peer.ready; });
			if (lookup->manifestAvailable
				&& lookup->canonicalProtocolVersion == 1
				&& !lookup->canonicalInfoHashV2.empty()
				&& (hasReadyPeer || !urlSeeds.empty()))
				break;

			auto const delay = std::chrono::milliseconds(
				std::clamp<std::uint32_t>(
					lookup->retryAfterMilliseconds, 250, 2000));
			std::this_thread::sleep_for(delay);
		}
		while (std::chrono::steady_clock::now() < deadline);

		if (!lookup || !lookup->manifestAvailable)
			co_return false;

		auto manifest = client.GetManifest(lookup->contentId);
		if (!manifest || manifest->empty())
			co_return false;
		if (!::OpenNet::Core::Content::CanonicalV2Swarm::ValidateManifest(
			identity,
			lookup->size,
			*manifest,
			lookup->canonicalInfoHashV2))
			co_return false;

		if (sessionId.empty())
		{
			sessionId = "download:" + lookup->contentId + ":"
				+ std::to_string(
					std::hash<std::wstring>{}(
						targetFilePath.lexically_normal().wstring()));
		}
		auto opened = OpenLongSeedDownloadSession(
			sessionId, *manifest, targetFilePath, urlSeeds);
		if (!opened.succeeded
			|| opened.infoHashV2 != lookup->canonicalInfoHashV2)
		{
			CloseLongSeedSession(sessionId);
			co_return false;
		}

		std::size_t connected{};
		for (bool utpPass : { true, false })
		{
			for (auto const& peer : lookup->peers)
			{
				if (!peer.ready) continue;
				for (auto const& endpoint : peer.endpoints)
				{
					bool const isUtp =
						endpoint.transport
						== ::OpenNet::Core::Content::ContentPeerTransport::Utp;
					if (isUtp != utpPass) continue;
					if (ConnectLongSeedPeer(
						sessionId,
						endpoint.address,
						endpoint.port,
						isUtp))
						++connected;
				}
			}
			if (connected != 0) break;
		}

		if (connected == 0 && urlSeeds.empty())
		{
			CloseLongSeedSession(sessionId);
			co_return false;
		}
		co_return true;
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
						m_stateManager.reset();
						throw winrt::hresult_error(E_FAIL, L"Failed to initialize torrent persistence.");
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
					if (!m_torrentCore->Start() || !m_torrentCore->IsRunning())
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
			// interfaces. Do not unconditionally reopen healthy sockets here: that
			// used to tear down the just-started DHT UDP socket and could interrupt
			// magnet metadata discovery during its most important bootstrap window.
			co_await winrt::resume_after(std::chrono::seconds(2));

			std::scoped_lock lk(m_torrentMutex);
			if (m_torrentCore && m_torrentCore->IsRunning())
			{
				auto const listen = m_torrentCore->GetListenStatus();
				if (!listen.isListening || listen.port == 0)
				{
					// One bounded recovery attempt is useful for a transient bind failure.
					// Healthy sessions are left untouched.
					m_torrentCore->RefreshPortMappings();
					OutputDebugStringA("P2PManager: Retrying failed startup listen bind\n");
				}
				else
				{
					OutputDebugStringA("P2PManager: Startup listeners are ready\n");
				}
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
		// Start tracker loading and the libtorrent session together. The network
		// session must not wait behind unrelated local tracker-list I/O because a
		// warm DHT table directly determines magnet metadata discovery latency.
		auto trackerInitialization = ::OpenNet::Core::Torrent::TrackerManager::Instance()
			.InitializeAsync();
		co_await EnsureTorrentCoreInitializedAsync();
		co_await trackerInitialization;
		std::scoped_lock lk(m_torrentMutex);
		if (!m_torrentCore) co_return false;
		co_return m_torrentCore->AddMagnet(
			magnetUri, savePath, filePriorities, extraTrackers, startImmediately)
			.Succeeded();
	}

	IAsyncOperation<bool> P2PManager::AddTorrentFileAsync(std::string torrentFilePath, std::string savePath, std::vector<int> const& filePriorities, std::vector<std::string> const& extraTrackers, bool startImmediately, bool seedMode)
	{
		auto trackerInitialization = ::OpenNet::Core::Torrent::TrackerManager::Instance()
			.InitializeAsync();
		co_await EnsureTorrentCoreInitializedAsync();
		co_await trackerInitialization;
		std::scoped_lock lk(m_torrentMutex);
		if (!m_torrentCore) co_return false;
		co_return m_torrentCore->AddTorrentFile(
			torrentFilePath, savePath, filePriorities, extraTrackers,
			startImmediately, seedMode).Succeeded();
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
			if (task.status == 1 || task.status == 2 || task.status == 3 || task.status == 4)
			{
				std::string resumedId = m_torrentCore->AddTorrentFromResumeData(task.taskId);
				if (!resumedId.empty())
				{
					OutputDebugStringA(("Resumed task: " + task.taskId + " (status=" + std::to_string(task.status) + ")\n").c_str());

					// Ensure paused and completed tasks stay stopped even when
					// older resume data did not preserve that state.
					auto const policy = m_stateManager->LoadTaskSettings(task.taskId);
					if (task.status == 2 || task.status == 4
						|| (task.status == 3 && (!policy || policy->completionAction != 1)))
					{
						m_torrentCore->PauseTorrent(task.taskId);
						if ((task.status == 3 || task.status == 4)
							&& m_stateManager)
						{
							m_stateManager->UpdateTaskStatus(
								task.taskId, task.status);
						}
					}
					else if (task.status == 3)
					{
						m_torrentCore->ResumeTorrent(task.taskId);
						m_stateManager->UpdateTaskStatus(task.taskId, 3);
					}
				}
			}
		}
		m_torrentCore->RestoreQueuePositions();
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
				if (task.status >= 1 && task.status <= 4)
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
