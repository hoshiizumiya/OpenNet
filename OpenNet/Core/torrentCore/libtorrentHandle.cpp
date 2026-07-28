module;
#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/peer_info.hpp>
#include "Core/ClientFilter/ClientFilterManager.h"
#include <libtorrent/session_stats.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/socket_io.hpp>
#include <boost/asio/ip/address.hpp>

module OpenNet.Core.torrentCore.LibtorrentHandle;

import OpenNet.Core.torrentCore.TorrentStateManager;
import OpenNet.Core.TorrentSettings;
import winrt_base;

namespace lt = libtorrent;
using namespace std::chrono_literals;

namespace OpenNet::Core::Torrent
{
	LibtorrentHandle::LibtorrentHandle()
	{
	}
	LibtorrentHandle::~LibtorrentHandle()
	{
		// If still running, save resume data and stop gracefully
		if (m_running.load() && m_session)
		{
			SaveAllResumeData();
		}
		Stop();
	}

	bool LibtorrentHandle::Initialize()
	{
		if (m_session)
			return true; // 已初始化
		try
		{
			lt::settings_pack currentSettings;
			ConfigureDefaultSettings(currentSettings);

			// Restore session_params before constructing the session. In
			// particular, this is how libtorrent restores the saved DHT node
			// IDs and routing-table bootstrap nodes.
			if (m_stateManager)
			{
				if (auto savedParams = m_stateManager->LoadSessionParams())
				{
					// Session-state blobs from older launches may contain stale
					// listen/DHT settings. The dedicated TorrentSettings store
					// remains authoritative.
					savedParams->settings = std::move(currentSettings);
					m_session = std::make_unique<lt::session>(
						std::move(*savedParams));
				}
			}
			if (!m_session)
			{
				m_session = std::make_unique<lt::session>(
					std::move(currentSettings));
			}
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA((std::string("LibtorrentHandle: Session initialization failed: ") + ex.what() + "\n").c_str());
			std::lock_guard lk(m_cbMutex);
			if (m_errorCb)
				m_errorCb(std::string("Session init error: ") + ex.what());
			return false;
		}
		return true;
	}

	void LibtorrentHandle::ConfigureDefaultSettings(lt::settings_pack& pack)
	{
		// Load persistent settings and apply to pack
		auto& settingsMgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
		settingsMgr.Load();
		auto settings = settingsMgr.Get();

		// Use the shared builder that maps TorrentSettings -> settings_pack
		::OpenNet::Core::ApplyTorrentSettingsToSettingsPack(settings, pack);

		if (settings.enableDht)
		{
			// A saved routing table may be absent or stale. Explicit bootstrap
			// routers make a fresh application start converge instead of
			// leaving the displayed DHT node count at zero indefinitely.
			pack.set_str(
				lt::settings_pack::dht_bootstrap_nodes,
				"router.bittorrent.com:6881,"
				"router.utorrent.com:6881,"
				"dht.transmissionbt.com:6881,"
				"dht.libtorrent.org:25401");
		}
	}

	void LibtorrentHandle::Start()
	{
		if (!Initialize())
			return;
		if (m_running.load())
			return;
		m_stopRequested = false;
		m_running = true;
		m_thread = std::thread(&LibtorrentHandle::AlertLoop, this);
		try
		{
			// Prime the UI-visible counters immediately; the alert loop keeps
			// refreshing them at its normal two-second cadence.
			m_session->post_session_stats();
			m_session->post_dht_stats();
		}
		catch (...)
		{
		}
	}

	void LibtorrentHandle::Stop()
	{
		OutputDebugStringA("LibtorrentHandle: Stopping...\n");

		// Wait for all pending save_resume_data alerts to be processed
		// before signalling the alert loop to stop. This ensures resume
		// data is persisted before the session is torn down.
		if (m_session && m_pendingResumeDataCount.load() > 0)
		{
			OutputDebugStringA("LibtorrentHandle: Waiting for pending resume data saves...\n");
			auto start = std::chrono::steady_clock::now();
			constexpr auto kTimeout = std::chrono::seconds(10);

			while (m_pendingResumeDataCount.load() > 0)
			{
				auto elapsed = std::chrono::steady_clock::now() - start;
				if (elapsed > kTimeout)
				{
					OutputDebugStringA("LibtorrentHandle: Timeout waiting for resume data alerts\n");
					break;
				}
				std::this_thread::sleep_for(50ms);
			}
			OutputDebugStringA("LibtorrentHandle: All pending resume data saves completed\n");
		}

		// Signal the alert loop to stop as soon as possible.
		m_stopRequested = true;
		// Wake the alert loop so it can observe the stop request promptly.
		// post_torrent_updates() only posts an alert when active torrents exist,
		// so we also call post_session_stats() which unconditionally posts a
		// session_stats_alert, guaranteeing wait_for_alert() returns immediately.
		if (m_session)
		{
			try
			{
				m_session->post_torrent_updates();
				m_session->post_session_stats();
			}
			catch (...)
			{
			}
		}

		if (m_running.exchange(false))
		{
			if (m_thread.joinable())
			{
				OutputDebugStringA("LibtorrentHandle: Waiting for alert thread to finish...\n");

				// Wait with timeout to avoid hanging indefinitely
				bool joined = false;
				auto start = std::chrono::steady_clock::now();
				while (!joined)
				{
					auto elapsed = std::chrono::steady_clock::now() - start;
					if (elapsed > std::chrono::seconds(5))
					{
						OutputDebugStringA("LibtorrentHandle: Warning: Alert thread did not join within 5 seconds\n");
						// Detach the thread to avoid crash, but continue
						break;
					}

					if (m_thread.joinable())
					{
						m_thread.join();
						joined = true;
						OutputDebugStringA("LibtorrentHandle: Alert thread joined successfully\n");
					}
					else
					{
						std::this_thread::sleep_for(100ms);
					}
				}
			}
		}

		// Save session state and then clear session. Do this after the
		// alert thread has been joined so we don't contend for libtorrent
		// internal locks between threads.
		if (m_session && m_stateManager)
		{
			try
			{
				OutputDebugStringA("LibtorrentHandle: Saving session state\n");
				m_stateManager->SaveSessionState(*m_session);
			}
			catch (const std::exception& ex)
			{
				OutputDebugStringW((L"LibtorrentHandle: Error saving session: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
			}
		}

		// Clear session using abort() + session_proxy for non-blocking shutdown.
		// session::abort() starts an asynchronous shutdown (notifying trackers etc.)
		// and returns a session_proxy. Destroying the session after abort() is
		// immediate. The proxy destructor will synchronize the background threads.
		if (m_session)
		{
			try
			{
				OutputDebugStringA("LibtorrentHandle: Aborting session (non-blocking)\n");
				m_sessionProxy = m_session->abort();
				m_session.reset();
			}
			catch (const std::exception& ex)
			{
				OutputDebugStringW((L"LibtorrentHandle: Error aborting session: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
			}
			catch (...)
			{
				OutputDebugStringA("LibtorrentHandle: Unknown error aborting session\n");
			}
		}

		OutputDebugStringA("LibtorrentHandle: Stop completed\n");
	}

	bool LibtorrentHandle::AddMagnet(std::string const& magnetUri, std::string const& savePath, std::vector<int> const& filePriorities)
	{
		if (!Initialize())
			return false;
		try
		{
			lt::add_torrent_params atp = lt::parse_magnet_uri(magnetUri);
			atp.save_path = savePath; // 目标目录
			// Remove seed_mode flag for downloads
			atp.flags &= ~lt::torrent_flags::seed_mode;

			// Apply storage preallocation setting
			auto torrentSettings = ::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			if (torrentSettings.preallocateStorage)
			{
				atp.storage_mode = lt::storage_mode_allocate;
			}

			if (!filePriorities.empty())
			{
				atp.file_priorities.reserve(filePriorities.size());
				for (int p : filePriorities)
				{
					atp.file_priorities.push_back(static_cast<lt::download_priority_t>(
						static_cast<std::uint8_t>(std::clamp(p, 0, 7))));
				}
			}

			// Generate the stable application task ID. Persist only after
			// libtorrent accepted the torrent, otherwise a failed/duplicate
			// add would leave a blank ghost task in the database.
			std::string taskId = TorrentStateManager::GenerateTaskId();

			lt::torrent_handle handle = m_session->add_torrent(atp);

			// Store mapping
			{
				std::lock_guard lk(m_torrentMapMutex);
				m_taskIdToHandle[taskId] = handle;
				m_handleToTaskId[handle] = taskId;
			}

			if (m_stateManager)
			{
				TaskMetadata metadata;
				metadata.taskId = taskId;
				metadata.magnetUri = magnetUri;
				metadata.savePath = savePath;
				metadata.name = ""; // Updated when metadata is received
				metadata.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now().time_since_epoch())
					.count();
				metadata.status = 1; // Downloading
				m_stateManager->SaveTaskMetadata(metadata);
			}

			return true;
		}
		catch (std::exception const& ex)
		{
			std::lock_guard lk(m_cbMutex);
			if (m_errorCb)
				m_errorCb(std::string("AddMagnet error: ") + ex.what());
			return false;
		}
	}

	bool LibtorrentHandle::AddTorrentFile(std::string const& torrentFilePath, std::string const& savePath, std::vector<int> const& filePriorities)
	{
		if (!Initialize())
			return false;
		try
		{
			// Load torrent info from file
			lt::torrent_info ti(torrentFilePath);

			lt::add_torrent_params atp;
			atp.ti = std::make_shared<lt::torrent_info>(ti);
			atp.save_path = savePath;
			// Remove seed_mode flag for downloads
			atp.flags &= ~lt::torrent_flags::seed_mode;

			// Apply storage preallocation setting
			auto torrentSettings = ::OpenNet::Core::TorrentSettingsManager::Instance().Get();
			if (torrentSettings.preallocateStorage)
			{
				atp.storage_mode = lt::storage_mode_allocate;
			}

			if (!filePriorities.empty())
			{
				atp.file_priorities.reserve(filePriorities.size());
				for (int p : filePriorities)
				{
					atp.file_priorities.push_back(static_cast<lt::download_priority_t>(
						static_cast<std::uint8_t>(std::clamp(p, 0, 7))));
				}
			}

			// Generate the stable application task ID. Persist only after
			// libtorrent accepted the torrent.
			std::string taskId = TorrentStateManager::GenerateTaskId();

			lt::torrent_handle handle = m_session->add_torrent(atp);

			// Store mapping
			{
				std::lock_guard lk(m_torrentMapMutex);
				m_taskIdToHandle[taskId] = handle;
				m_handleToTaskId[handle] = taskId;
			}

			if (m_stateManager)
			{
				TaskMetadata metadata;
				metadata.taskId = taskId;
				metadata.magnetUri = ""; // Not a magnet; resume data carries torrent metadata
				metadata.savePath = savePath;
				metadata.name = ti.name();
				metadata.totalSize = ti.total_size();
				metadata.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::system_clock::now().time_since_epoch())
					.count();
				metadata.status = 1; // Downloading
				m_stateManager->SaveTaskMetadata(metadata);
			}

			return true;
		}
		catch (std::exception const& ex)
		{
			std::lock_guard lk(m_cbMutex);
			if (m_errorCb)
				m_errorCb(std::string("AddTorrentFile error: ") + ex.what());
			return false;
		}
	}

	std::string LibtorrentHandle::AddTorrentFromResumeData(std::string const& taskId)
	{
		if (!Initialize())
			return "";
		if (!m_stateManager)
			return "";

		try
		{
			auto paramsOpt = m_stateManager->LoadTaskResumeData(taskId);
			if (!paramsOpt.has_value())
			{
				// Try to load from metadata
				auto metadataOpt = m_stateManager->LoadTaskMetadata(taskId);
				if (!metadataOpt.has_value() || metadataOpt->magnetUri.empty())
				{
					return "";
				}

				// Re-add using magnet URI
				lt::add_torrent_params atp = lt::parse_magnet_uri(metadataOpt->magnetUri);
				atp.save_path = metadataOpt->savePath;
				atp.flags &= ~lt::torrent_flags::seed_mode;

				lt::torrent_handle handle = m_session->add_torrent(atp);

				{
					std::lock_guard lk(m_torrentMapMutex);
					m_taskIdToHandle[taskId] = handle;
					m_handleToTaskId[handle] = taskId;
				}

				return taskId;
			}

			lt::add_torrent_params atp = paramsOpt.value();
			lt::torrent_handle handle = m_session->add_torrent(atp);

			{
				std::lock_guard lk(m_torrentMapMutex);
				m_taskIdToHandle[taskId] = handle;
				m_handleToTaskId[handle] = taskId;
			}

			return taskId;
		}
		catch (std::exception const& ex)
		{
			std::lock_guard lk(m_cbMutex);
			if (m_errorCb)
				m_errorCb(std::string("AddTorrentFromResumeData error: ") + ex.what());
			return "";
		}
	}

	void LibtorrentHandle::PauseTorrent(std::string const& taskId)
	{
		lt::torrent_handle handle;
		{
			std::lock_guard lk(m_torrentMapMutex);
			auto it = m_taskIdToHandle.find(taskId);
			if (it == m_taskIdToHandle.end() || !it->second.is_valid())
				return;
			handle = it->second;
		}

		// libtorrent auto-manages torrents by default and may resume an
		// auto-managed torrent after pause(). Make this an explicit user-
		// managed pause and persist the state immediately.
		handle.unset_flags(lt::torrent_flags::auto_managed);
		handle.pause();
		if (m_stateManager)
		{
			m_stateManager->UpdateTaskStatus(taskId, 2); // Paused
		}
		RequestResumeDataForTorrent(handle);
	}

	void LibtorrentHandle::ResumeTorrent(std::string const& taskId)
	{
		lt::torrent_handle handle;
		{
			std::lock_guard lk(m_torrentMapMutex);
			auto it = m_taskIdToHandle.find(taskId);
			if (it == m_taskIdToHandle.end() || !it->second.is_valid())
				return;
			handle = it->second;
		}

		handle.set_flags(lt::torrent_flags::auto_managed);
		handle.resume();
		if (m_stateManager)
		{
			m_stateManager->UpdateTaskStatus(taskId, 1); // Downloading
		}
		RequestResumeDataForTorrent(handle);
	}

	void LibtorrentHandle::RemoveTorrent(std::string const& taskId, bool deleteFiles)
	{
		lt::torrent_handle handle;
		{
			std::lock_guard lk(m_torrentMapMutex);
			auto it = m_taskIdToHandle.find(taskId);
			if (it == m_taskIdToHandle.end())
				return;
			handle = it->second;
			m_handleToTaskId.erase(handle);
			m_taskIdToHandle.erase(it);
		}

		if (m_session && handle.is_valid())
		{
			lt::remove_flags_t flags = {};
			if (deleteFiles)
			{
				flags = lt::session::delete_files;
			}
			m_session->remove_torrent(handle, flags);
		}

		if (m_stateManager)
		{
			m_stateManager->DeleteTask(taskId);
		}
	}

	void LibtorrentHandle::SaveAllResumeData()
	{
		if (!m_session)
			return;

		std::lock_guard lk(m_torrentMapMutex);
		for (auto const& [taskId, handle] : m_taskIdToHandle)
		{
			if (handle.is_valid())
			{
				RequestResumeDataForTorrent(handle);
			}
		}
	}

	void LibtorrentHandle::SetProgressCallback(ProgressCallback cb)
	{
		std::lock_guard lk(m_cbMutex);
		m_progressCb = std::move(cb);
	}
	void LibtorrentHandle::SetFinishedCallback(FinishedCallback cb)
	{
		std::lock_guard lk(m_cbMutex);
		m_finishedCb = std::move(cb);
	}
	void LibtorrentHandle::SetErrorCallback(ErrorCallback cb)
	{
		std::lock_guard lk(m_cbMutex);
		m_errorCb = std::move(cb);
	}

	std::string LibtorrentHandle::GetTaskIdByName(std::string const& name) const
	{
		std::lock_guard lk(m_torrentMapMutex);
		for (auto const& [taskId, handle] : m_taskIdToHandle)
		{
			if (handle.is_valid())
			{
				try
				{
					auto status = handle.status();
					if (status.name == name)
					{
						return taskId;
					}
				}
				catch (...)
				{
				}
			}
		}
		return "";
	}

	void LibtorrentHandle::AlertLoop()
	{
		OutputDebugStringA("LibtorrentHandle: AlertLoop started\n");

		int emptyAlertCount = 0;
		const int maxEmptyCount = 3; // After 3 empty cycles, increase sleep time
		std::chrono::milliseconds sleepTime(50);
		std::vector<lt::alert*> alerts;

		while (!m_stopRequested.load())
		{
			if (!m_session)
			{
				OutputDebugStringA("LibtorrentHandle: No session in AlertLoop, breaking\n");
				break;
			}

			try
			{
				alerts.clear();
				// Wait with appropriate timeout based on activity
				m_session->wait_for_alert(sleepTime);
				m_session->pop_alerts(&alerts);

				if (!alerts.empty())
				{
					DispatchAlerts(alerts);
					// Reset counters and sleep time when we have activity
					emptyAlertCount = 0;
					sleepTime = std::chrono::milliseconds(50);
				}
				else
				{
					// Gradually increase sleep time if no activity
					++emptyAlertCount;
					if (emptyAlertCount >= maxEmptyCount)
					{
						sleepTime = std::chrono::milliseconds(200);
					}
				}

				// Only request updates if we have torrents and are still running
				if (m_session && !m_stopRequested.load())
				{
					bool hasTorrents = false;
					{
						std::lock_guard lk(m_torrentMapMutex);
						hasTorrents = !m_taskIdToHandle.empty();
					}
					const auto now = std::chrono::steady_clock::now();
					// post_torrent_updates() itself produces an alert. Posting
					// it on every loop iteration keeps wait_for_alert() awake
					// and turns this worker into a busy loop. One update per
					// second is responsive enough for both native and Web UI
					// consumers while allowing the thread to sleep.
					if (hasTorrents
						&& now - m_lastTorrentUpdateRequest
						>= std::chrono::seconds(1))
					{
						m_session->post_torrent_updates();
						m_lastTorrentUpdateRequest = now;
					}
					// Time-gated stats requests: only every 2 seconds to avoid
					// self-excitation (these calls generate alerts that reset
					// emptyAlertCount, preventing adaptive backoff from engaging)
					if (now - m_lastStatsRequest >= std::chrono::seconds(2))
					{
						m_session->post_session_stats();
						m_session->post_dht_stats();
						m_lastStatsRequest = now;
					}
					if (now - m_lastClientFilterCheck >= std::chrono::seconds(3))
					{
						EnforceClientFilters();
						m_lastClientFilterCheck = now;
					}
				}
			}
			catch (const std::exception& ex)
			{
				OutputDebugStringW((L"LibtorrentHandle: AlertLoop error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
				std::this_thread::sleep_for(100ms);
				sleepTime = std::chrono::milliseconds(50);
				emptyAlertCount = 0;
			}
			catch (...)
			{
				OutputDebugStringA("LibtorrentHandle: AlertLoop unknown error\n");
				std::this_thread::sleep_for(100ms);
				sleepTime = std::chrono::milliseconds(50);
				emptyAlertCount = 0;
			}
		}

		OutputDebugStringA("LibtorrentHandle: AlertLoop exiting\n");
	}

	void LibtorrentHandle::EnforceClientFilters()
	{
		auto& filter = ::OpenNet::Core::ClientFilterManager::Instance();
		if (!filter.IsEnabled())
			return;

		std::vector<std::pair<std::string, lt::torrent_handle>> torrents;
		{
			std::lock_guard lock(m_torrentMapMutex);
			torrents.reserve(m_taskIdToHandle.size());
			for (auto const& [taskId, handle] : m_taskIdToHandle)
			{
				if (handle.is_valid())
					torrents.emplace_back(taskId, handle);
			}
		}

		std::vector<::OpenNet::Core::ClientPeerObservation> observations;
		for (auto const& [taskId, handle] : torrents)
		{
			try
			{
				auto const status = handle.status();
				std::vector<lt::peer_info> peers;
				handle.get_peer_info(peers);
				observations.reserve(observations.size() + peers.size());
				for (auto const& peer : peers)
				{
					if (peer.client.empty() || peer.ip.address().is_unspecified())
						continue;
					observations.push_back({
						peer.client,
						peer.ip.address().to_string(),
						status.name.empty() ? taskId : status.name,
										   });
				}
			}
			catch (...)
			{
				// A torrent may disappear while the snapshot is being read.
			}
		}
		filter.EvaluatePeers(observations);
	}

	void LibtorrentHandle::DispatchAlerts(std::vector<lt::alert*> const& alerts)
	{
		for (lt::alert* a : alerts)
		{
			if (auto st = lt::alert_cast<lt::state_update_alert>(a))
			{
				ProgressCallback progressCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					progressCbCopy = m_progressCb;
				}

				if (progressCbCopy)
				{
					for (auto const& s : st->status)
					{
						std::string taskId;
						{
							std::lock_guard mapLk(m_torrentMapMutex);
							auto const it = m_handleToTaskId.find(s.handle);
							if (it != m_handleToTaskId.end())
								taskId = it->second;
						}

						// Ignore handles that do not belong to an OpenNet task.
						// This prevents anonymous session activity from creating
						// UI rows that cannot be controlled or persisted.
						if (taskId.empty())
							continue;

						ProgressEvent evt;
						evt.taskId = taskId;
						evt.progressPercent = static_cast<int>(s.progress_ppm / 10000); // 1e6 -> %
						evt.downloadRateKB = static_cast<int>(s.download_rate / 1000);
						evt.uploadRateKB = static_cast<int>(s.upload_rate / 1000);
						evt.name = s.name;
						progressCbCopy(evt);

						// Update progress in database
						if (m_stateManager)
						{
							m_stateManager->UpdateTaskProgress(taskId, s.total_done);
						}
					}
				}
			}
			else if (auto tf = lt::alert_cast<lt::torrent_finished_alert>(a))
			{
				// Completed downloads should remain stopped. In particular,
				// clear auto_managed before pausing or libtorrent may resume
				// the torrent automatically for seeding.
				tf->handle.unset_flags(lt::torrent_flags::auto_managed);
				tf->handle.pause();

				std::string taskId;
				{
					std::lock_guard mapLk(m_torrentMapMutex);
					auto const it = m_handleToTaskId.find(tf->handle);
					if (it != m_handleToTaskId.end())
						taskId = it->second;
				}

				if (m_stateManager && !taskId.empty())
				{
					m_stateManager->UpdateTaskStatus(taskId, 3); // Completed
				}

				// Save after changing flags so the completed/paused state is
				// what is restored on the next launch.
				RequestResumeDataForTorrent(tf->handle);

				FinishedCallback finishedCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					finishedCbCopy = m_finishedCb;
				}

				if (finishedCbCopy)
				{
					try
					{
						auto status = tf->handle.status();
						finishedCbCopy(taskId, status.name);
					}
					catch (...)
					{
					}
				}
			}
			else if (auto srd = lt::alert_cast<lt::save_resume_data_alert>(a))
			{
				HandleSaveResumeDataAlert(srd);
			}
			else if (auto srdf = lt::alert_cast<lt::save_resume_data_failed_alert>(a))
			{
				HandleSaveResumeDataFailedAlert(srdf);
			}
			else if (auto mapping = lt::alert_cast<lt::portmap_alert>(a))
			{
				std::lock_guard lock(m_portMappingMutex);
				std::string mechanism =
					mapping->map_transport == lt::portmap_transport::upnp
					? "UPnP" : "NAT-PMP/PCP";
				if (mapping->map_protocol == lt::portmap_protocol::tcp)
				{
					m_portMappingStatus.tcpExternalPort = mapping->external_port;
					m_portMappingStatus.tcpMechanism = mechanism;
				}
				else if (mapping->map_protocol == lt::portmap_protocol::udp)
				{
					m_portMappingStatus.udpExternalPort = mapping->external_port;
					m_portMappingStatus.udpMechanism = mechanism;
				}
				m_portMappingStatus.lastError.clear();
			}
			else if (auto mappingError = lt::alert_cast<lt::portmap_error_alert>(a))
			{
				std::lock_guard lock(m_portMappingMutex);
				m_portMappingStatus.lastError = mappingError->message();
			}
			else if (auto externalIp = lt::alert_cast<lt::external_ip_alert>(a))
			{
				std::lock_guard lock(m_portMappingMutex);
				m_portMappingStatus.externalAddress =
					externalIp->external_address.to_string();
			}
			else if (auto listenFailed = lt::alert_cast<lt::listen_failed_alert>(a))
			{
				std::lock_guard lock(m_listenStateMutex);
				m_lastListenError = listenFailed->message();
			}
			else if (lt::alert_cast<lt::listen_succeeded_alert>(a))
			{
				std::lock_guard lock(m_listenStateMutex);
				m_lastListenError.clear();
			}
			else if (auto se = lt::alert_cast<lt::session_error_alert>(a))
			{
				ErrorCallback errorCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					errorCbCopy = m_errorCb;
				}

				if (errorCbCopy)
				{
					errorCbCopy(se->message());
				}
			}
			else if (auto te = lt::alert_cast<lt::torrent_error_alert>(a))
			{
				// Update status in database
				if (m_stateManager)
				{
					std::lock_guard mapLk(m_torrentMapMutex);
					auto it = m_handleToTaskId.find(te->handle);
					if (it != m_handleToTaskId.end())
					{
						m_stateManager->UpdateTaskStatus(it->second, 4); // Failed
					}
				}

				ErrorCallback errorCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					errorCbCopy = m_errorCb;
				}

				if (errorCbCopy)
				{
					errorCbCopy(te->message());
				}
			}
			else if (auto fe = lt::alert_cast<lt::file_error_alert>(a))
			{
				ErrorCallback errorCbCopy;
				{
					std::lock_guard lk(m_cbMutex);
					errorCbCopy = m_errorCb;
				}

				if (errorCbCopy)
				{
					errorCbCopy(fe->message());
				}
			}
			else if (auto ma = lt::alert_cast<lt::metadata_received_alert>(a))
			{
				// Update task name when metadata is received
				if (m_stateManager && ma->handle.is_valid())
				{
					try
					{
						auto status = ma->handle.status();
						std::lock_guard mapLk(m_torrentMapMutex);
						auto it = m_handleToTaskId.find(ma->handle);
						if (it != m_handleToTaskId.end())
						{
							auto metaOpt = m_stateManager->LoadTaskMetadata(it->second);
							if (metaOpt.has_value())
							{
								TaskMetadata meta = metaOpt.value();
								meta.name = status.name;
								meta.totalSize = status.total_wanted;
								m_stateManager->SaveTaskMetadata(meta);
							}
						}
					}
					catch (...)
					{
					}
				}
			}
			else if (auto dsa = lt::alert_cast<lt::dht_stats_alert>(a))
			{
				// Cache DHT routing table node count from the alert
				int totalNodes = 0;
				for (auto const& bucket : dsa->routing_table)
				{
					totalNodes += bucket.num_nodes;
				}
				m_cachedDhtNodeCount.store(totalNodes);
			}
			else if (auto ssa = lt::alert_cast<lt::session_stats_alert>(a))
			{
				// Cache session-level counters for use in GetSessionStats()
				if (!m_sessionStatsMetricsResolved)
				{
					ResolveSessionStatsMetricIndices();
				}

				auto const& counters = ssa->counters();
				std::lock_guard lkStats(m_sessionStatsMutex);
				if (m_sessionStatsMetricIdxRecvBytes >= 0)
					m_sessionTotalDownload = counters[m_sessionStatsMetricIdxRecvBytes];
				if (m_sessionStatsMetricIdxSentBytes >= 0)
					m_sessionTotalUpload = counters[m_sessionStatsMetricIdxSentBytes];
				if (m_sessionStatsMetricIdxDhtNodes >= 0)
					m_cachedDhtNodeCount.store(static_cast<int>(counters[m_sessionStatsMetricIdxDhtNodes]));
			}
		}
	}

	void LibtorrentHandle::ResolveSessionStatsMetricIndices()
	{
		auto metrics = lt::session_stats_metrics();
		for (auto const& m : metrics)
		{
			if (m.name == std::string("net.recv_payload_bytes"))
				m_sessionStatsMetricIdxRecvBytes = m.value_index;
			else if (m.name == std::string("net.sent_payload_bytes"))
				m_sessionStatsMetricIdxSentBytes = m.value_index;
			else if (m.name == std::string("dht.dht_nodes"))
				m_sessionStatsMetricIdxDhtNodes = m.value_index;
		}
		m_sessionStatsMetricsResolved = true;
	}

	void LibtorrentHandle::HandleSaveResumeDataAlert(lt::save_resume_data_alert const* alert)
	{
		if (!m_stateManager)
		{
			m_pendingResumeDataCount.fetch_sub(1);
			return;
		}
		if (!alert || !alert->handle.is_valid())
		{
			m_pendingResumeDataCount.fetch_sub(1);
			return;
		}

		try
		{
			std::lock_guard lk(m_torrentMapMutex);
			auto it = m_handleToTaskId.find(alert->handle);
			if (it != m_handleToTaskId.end())
			{
				m_stateManager->SaveTaskResumeData(it->second, alert->params);
			}
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("HandleSaveResumeDataAlert error: " + std::string(ex.what()) + "\n").c_str());
		}
		m_pendingResumeDataCount.fetch_sub(1);
	}

	void LibtorrentHandle::HandleSaveResumeDataFailedAlert(lt::save_resume_data_failed_alert const* alert)
	{
		m_pendingResumeDataCount.fetch_sub(1);
		if (alert)
		{
			OutputDebugStringA(("Save resume data failed: " + alert->message() + "\n").c_str());
		}
	}

	void LibtorrentHandle::RequestResumeDataForTorrent(lt::torrent_handle const& handle)
	{
		if (handle.is_valid())
		{
			try
			{
				handle.save_resume_data(lt::torrent_handle::save_info_dict);
				m_pendingResumeDataCount.fetch_add(1);
			}
			catch (...)
			{
			}
		}
	}

	lt::settings_pack LibtorrentHandle::GetSettings() const
	{
		if (m_session)
		{
			return m_session->get_settings();
		}
		return lt::settings_pack{};
	}

	void LibtorrentHandle::ApplySettings(lt::settings_pack const& pack)
	{
		if (m_session)
		{
			m_session->apply_settings(pack);
		}
	}

	void LibtorrentHandle::SetIpFilter(lt::ip_filter const& filter)
	{
		if (m_session)
		{
			m_session->set_ip_filter(filter);
		}
	}

	LibtorrentHandle::PortMappingStatus LibtorrentHandle::GetPortMappingStatus() const
	{
		std::lock_guard lock(m_portMappingMutex);
		auto status = m_portMappingStatus;
		if (m_session)
		{
			auto settings = m_session->get_settings();
			status.upnpEnabled = settings.get_bool(lt::settings_pack::enable_upnp);
			status.natPmpEnabled = settings.get_bool(lt::settings_pack::enable_natpmp);
		}
		return status;
	}

	void LibtorrentHandle::RefreshPortMappings()
	{
		if (!m_session)
			return;
		{
			std::lock_guard lock(m_portMappingMutex);
			m_portMappingStatus.tcpExternalPort = 0;
			m_portMappingStatus.udpExternalPort = 0;
			m_portMappingStatus.tcpMechanism.clear();
			m_portMappingStatus.udpMechanism.clear();
			m_portMappingStatus.lastError.clear();
		}
		m_session->reopen_network_sockets(lt::session_handle::reopen_map_ports);
	}

	// ---------------------------------------------------------------
	//  Session-level aggregate statistics
	// ---------------------------------------------------------------
	LibtorrentHandle::SessionStats LibtorrentHandle::GetSessionStats() const
	{
		SessionStats stats{};
		if (!m_session)
			return stats;

		try
		{
			// Aggregate from all torrent handles
			auto torrents = m_session->get_torrents();
			stats.numTorrents = static_cast<int>(torrents.size());

			for (auto const& h : torrents)
			{
				if (!h.is_valid())
					continue;
				auto st = h.status(lt::torrent_handle::query_accurate_download_counters);
				stats.totalDownloadRate += st.download_rate;
				stats.totalUploadRate += st.upload_rate;
				stats.totalDownloaded += st.total_done;
				stats.totalUploaded += st.total_upload;
				stats.numPeers += st.num_peers;
			}

			// DHT nodes — use the cached value from dht_stats_alert / session_stats_alert
			stats.dhtNodes = m_cachedDhtNodeCount.load();

			// Only expose the port that libtorrent actually opened. The configured
			// listen_interfaces value is not proof that bind/listen succeeded.
			try
			{
				stats.isListening = m_session->is_listening();
				stats.listenPort = stats.isListening
					? static_cast<int>(m_session->listen_port())
					: 0;
			}
			catch (...)
			{
				stats.isListening = false;
				stats.listenPort = 0;
			}
			{
				std::lock_guard lock(m_listenStateMutex);
				stats.listenError = m_lastListenError;
			}

			// Session-level totals from session_stats_alert (more accurate than per-torrent sums)
			{
				std::lock_guard lkStats(m_sessionStatsMutex);
				if (m_sessionTotalDownload > 0)
					stats.totalDownloaded = m_sessionTotalDownload;
				if (m_sessionTotalUpload > 0)
					stats.totalUploaded = m_sessionTotalUpload;
			}
		}
		catch (...)
		{
		}

		return stats;
	}

	// ---------------------------------------------------------------
	//  Per-torrent detail
	// ---------------------------------------------------------------
	LibtorrentHandle::TorrentDetailInfo LibtorrentHandle::GetTorrentDetail(
		std::string const& taskId) const
	{
		TorrentDetailInfo info{};
		info.taskId = taskId;

		lt::torrent_handle handle;
		{
			std::lock_guard lk(m_torrentMapMutex);
			auto it = m_taskIdToHandle.find(taskId);
			if (it == m_taskIdToHandle.end())
				return info;
			handle = it->second;
		}

		if (!handle.is_valid())
			return info;

		try
		{
			auto st = handle.status();
			info.name = st.name;
			info.savePath = st.save_path;
			info.totalSize = st.total_wanted;
			info.totalDone = st.total_done;
			info.totalUploaded = st.total_upload;
			info.downloadRate = st.download_rate;
			info.uploadRate = st.upload_rate;
			info.progressPpm = static_cast<int>(st.progress_ppm);
			info.numPeers = st.num_peers;
			info.numSeeds = st.num_seeds;
			info.numConnections = st.num_connections;
			info.state = static_cast<int>(st.state);
			info.isPaused = (st.flags & lt::torrent_flags::paused) != lt::torrent_flags_t{};
			info.isAutoManaged =
				bool(st.flags & lt::torrent_flags::auto_managed);
			info.isSequential =
				bool(st.flags & lt::torrent_flags::sequential_download);
			info.isSuperSeeding =
				bool(st.flags & lt::torrent_flags::super_seeding);
			info.queuePosition =
				static_cast<int>(handle.queue_position());

			if (st.total_done > 0)
				info.shareRatio = static_cast<double>(st.total_upload) / st.total_done;

			// qBittorrent uses a stable 160-bit TorrentID. For v2 torrents,
			// libtorrent's get_best() returns the truncated v2 hash; hybrid
			// torrents retain both full hashes for the explicit API fields.
			const auto hashes = handle.info_hashes();
			if (hashes.has_v1())
			{
				std::ostringstream stream;
				stream << hashes.v1;
				info.infoHashV1 = stream.str();
			}
			if (hashes.has_v2())
			{
				std::ostringstream stream;
				stream << hashes.v2;
				info.infoHashV2 = stream.str();
			}
			{
				std::ostringstream stream;
				stream << hashes.get_best();
				info.apiHash = stream.str();
			}
			info.infoHash = !info.infoHashV1.empty()
				? info.infoHashV1
				: info.infoHashV2;

			if (auto ti = handle.torrent_file())
			{
				info.comment = ti->comment();
			}

			// Peers
			std::vector<lt::peer_info> ltPeers;
			handle.get_peer_info(ltPeers);
			info.peers.reserve(ltPeers.size());
			for (auto const& p : ltPeers)
			{
				TorrentPeerInfo pi;
				pi.ip = p.ip.address().to_string();
				pi.port = p.ip.port();
				pi.client = p.client;
				pi.downloadRateKB = static_cast<int>(p.down_speed / 1000);
				pi.uploadRateKB = static_cast<int>(p.up_speed / 1000);
				pi.totalDownloaded = p.total_download;
				pi.totalUploaded = p.total_upload;
				pi.progress = p.progress;
				pi.flags = static_cast<uint32_t>(p.flags);
				pi.connectionType = static_cast<int>(static_cast<std::uint8_t>(p.connection_type));
				pi.source = static_cast<int>(static_cast<std::uint8_t>(p.source));
				pi.isIncoming = (p.source & lt::peer_info::incoming) != lt::peer_source_flags_t{};
				pi.isConnecting =
					(p.flags & (lt::peer_info::connecting | lt::peer_info::handshake)) !=
					lt::peer_flags_t{};
				info.peers.push_back(std::move(pi));
			}

			// Trackers
			auto ltTrackers = handle.trackers();
			info.trackers.reserve(ltTrackers.size());
			for (auto const& t : ltTrackers)
			{
				TorrentTrackerInfo ti;
				ti.url = t.url;
				ti.tier = t.tier;

				// Determine tracker status from endpoints
				if (!t.endpoints.empty())
				{
					auto const& ep = t.endpoints.front();
					if (!ep.info_hashes.empty())
					{
						auto const& ih = ep.info_hashes.front();
						ti.numPeers = ih.scrape_complete + ih.scrape_incomplete;
						if (ih.fails > 0)
						{
							ti.status = "error";
							ti.message = ih.message;
						}
						else if (ih.updating)
						{
							ti.status = "updating";
						}
						else
						{
							ti.status = "working";
						}
					}
				}
				else
				{
					ti.status = "not contacted";
				}

				info.trackers.push_back(std::move(ti));
			}

			// Files
			if (auto ti = handle.torrent_file())
			{
				info.pieceSize = ti->piece_length();
				info.piecesNum = ti->num_pieces();
				const auto piecePriorities =
					handle.get_piece_priorities();
				info.firstLastPiecePriority =
					!piecePriorities.empty()
					&& static_cast<std::uint8_t>(
						piecePriorities.front()) > 4
					&& static_cast<std::uint8_t>(
						piecePriorities.back()) > 4;
				auto const& fs = ti->files();
				auto fileProgress = handle.file_progress(lt::torrent_handle::piece_granularity);
				auto filePriorities = handle.get_file_priorities();
				int numFiles = fs.num_files();

				info.files.reserve(numFiles);
				for (int i = 0; i < numFiles; ++i)
				{
					TorrentFileEntry fe;
					fe.path = fs.file_path(lt::file_index_t{ i });
					fe.size = fs.file_size(lt::file_index_t{ i });
					fe.fileIndex = i;
					if (fe.size > 0)
					{
						fe.firstPiece = static_cast<int>(
							fs.map_file(
								lt::file_index_t{ i }, 0, 0).piece);
						fe.lastPiece = static_cast<int>(
							fs.map_file(
								lt::file_index_t{ i },
								fe.size - 1, 0).piece);
					}
					if (i < static_cast<int>(fileProgress.size()))
						fe.bytesCompleted = fileProgress[i];
					if (i < static_cast<int>(filePriorities.size()))
						fe.priority = static_cast<int>(static_cast<std::uint8_t>(filePriorities[i]));
					info.files.push_back(std::move(fe));
				}
			}
		}
		catch (...)
		{
		}

		return info;
	}

	LibtorrentHandle::TorrentPieceInfo
		LibtorrentHandle::GetTorrentPieceInfo(
			std::string const& taskId) const
	{
		TorrentPieceInfo result;
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return result;
		}

		try
		{
			const auto& handle = item->second;
			const auto torrentInfo = handle.torrent_file();
			if (!torrentInfo)
				return result;

			result.pieceSize = torrentInfo->piece_length();
			const int pieceCount = torrentInfo->num_pieces();
			result.states.assign(
				static_cast<std::size_t>(pieceCount), 0);
			const auto status =
				handle.status(lt::torrent_handle::query_pieces);
			for (int index = 0;
				 index < pieceCount
				 && index < status.pieces.size();
				 ++index)
			{
				if (status.pieces[lt::piece_index_t{ index }])
					result.states[static_cast<std::size_t>(index)] = 2;
			}

			std::vector<lt::peer_info> peers;
			handle.get_peer_info(peers);
			for (const auto& peer : peers)
			{
				const int index =
					static_cast<int>(peer.downloading_piece_index);
				if (index >= 0 && index < pieceCount
					&& result.states[
						static_cast<std::size_t>(index)] == 0)
				{
					result.states[
						static_cast<std::size_t>(index)] = 1;
				}
			}

			handle.piece_availability(result.availability);
			result.hashes.reserve(
				static_cast<std::size_t>(pieceCount));
			for (int index = 0; index < pieceCount; ++index)
			{
				std::ostringstream stream;
				stream << torrentInfo->hash_for_piece(
					lt::piece_index_t{ index });
				result.hashes.push_back(stream.str());
			}
			for (const auto& seed : torrentInfo->web_seeds())
				result.webSeeds.push_back(seed.url);
		}
		catch (...)
		{
		}
		return result;
	}

	std::vector<std::uint8_t> LibtorrentHandle::ExportTorrentFile(
		std::string const& taskId) const
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return {};
		}

		try
		{
			const auto torrentInfo = item->second.torrent_file();
			if (!torrentInfo)
				return {};
			lt::create_torrent creator(*torrentInfo);
			std::vector<char> encoded;
			lt::bencode(std::back_inserter(encoded), creator.generate());
			return { encoded.begin(), encoded.end() };
		}
		catch (...)
		{
			return {};
		}
	}

	// ---------------------------------------------------------------
	//  Force recheck
	// ---------------------------------------------------------------
	void LibtorrentHandle::ForceRecheck(std::string const& taskId)
	{
		std::lock_guard lk(m_torrentMapMutex);
		auto it = m_taskIdToHandle.find(taskId);
		if (it != m_taskIdToHandle.end() && it->second.is_valid())
		{
			it->second.force_recheck();
		}
	}

	void LibtorrentHandle::ForceReannounce(std::string const& taskId)
	{
		std::lock_guard lk(m_torrentMapMutex);
		const auto it = m_taskIdToHandle.find(taskId);
		if (it != m_taskIdToHandle.end() && it->second.is_valid())
			it->second.force_reannounce();
	}

	int LibtorrentHandle::GetTorrentDownloadLimit(
		std::string const& taskId) const
	{
		std::lock_guard lk(m_torrentMapMutex);
		const auto it = m_taskIdToHandle.find(taskId);
		if (it == m_taskIdToHandle.end() || !it->second.is_valid())
			return 0;
		return it->second.download_limit();
	}

	int LibtorrentHandle::GetTorrentUploadLimit(
		std::string const& taskId) const
	{
		std::lock_guard lk(m_torrentMapMutex);
		const auto it = m_taskIdToHandle.find(taskId);
		if (it == m_taskIdToHandle.end() || !it->second.is_valid())
			return 0;
		return it->second.upload_limit();
	}

	void LibtorrentHandle::SetTorrentDownloadLimit(
		std::string const& taskId, const int limit)
	{
		std::lock_guard lk(m_torrentMapMutex);
		const auto it = m_taskIdToHandle.find(taskId);
		if (it != m_taskIdToHandle.end() && it->second.is_valid())
			it->second.set_download_limit(std::max(0, limit));
	}

	void LibtorrentHandle::SetTorrentUploadLimit(
		std::string const& taskId, const int limit)
	{
		std::lock_guard lk(m_torrentMapMutex);
		const auto it = m_taskIdToHandle.find(taskId);
		if (it != m_taskIdToHandle.end() && it->second.is_valid())
			it->second.set_upload_limit(std::max(0, limit));
	}

	void LibtorrentHandle::AddTrackers(
		std::string const& taskId,
		std::vector<std::string> const& urls)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		for (const auto& url : urls)
		{
			if (!url.empty())
				item->second.add_tracker(lt::announce_entry(url));
		}
	}

	void LibtorrentHandle::EditTracker(
		std::string const& taskId,
		std::string const& originalUrl,
		std::string const& newUrl,
		const std::optional<int> tier)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		auto trackers = item->second.trackers();
		for (auto& tracker : trackers)
		{
			if (tracker.url != originalUrl)
				continue;
			const auto existingTier = tracker.tier;
			if (!newUrl.empty() && newUrl != originalUrl)
				tracker = lt::announce_entry(newUrl);
			tracker.tier = tier
				? static_cast<std::uint8_t>(*tier)
				: existingTier;
		}
		item->second.replace_trackers(trackers);
	}

	void LibtorrentHandle::RemoveTrackers(
		std::string const& taskId,
		std::vector<std::string> const& urls)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		const std::unordered_set<std::string> removed(
			urls.begin(), urls.end());
		auto trackers = item->second.trackers();
		std::erase_if(trackers, [&removed](const auto& tracker)
		{
			return removed.contains(tracker.url);
		});
		item->second.replace_trackers(trackers);
	}

	void LibtorrentHandle::AddWebSeeds(
		std::string const& taskId,
		std::vector<std::string> const& urls)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		for (const auto& url : urls)
		{
			if (!url.empty())
				item->second.add_url_seed(url);
		}
	}

	void LibtorrentHandle::EditWebSeed(
		std::string const& taskId,
		std::string const& originalUrl,
		std::string const& newUrl)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		item->second.remove_url_seed(originalUrl);
		if (!newUrl.empty())
			item->second.add_url_seed(newUrl);
	}

	void LibtorrentHandle::RemoveWebSeeds(
		std::string const& taskId,
		std::vector<std::string> const& urls)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		for (const auto& url : urls)
			item->second.remove_url_seed(url);
	}

	bool LibtorrentHandle::AddPeer(
		std::string const& taskId,
		std::string const& endpoint)
	{
		std::string addressText;
		std::string_view portText;
		if (endpoint.starts_with('['))
		{
			const auto closing = endpoint.find(']');
			if (closing == std::string::npos
				|| closing + 2 > endpoint.size()
				|| endpoint[closing + 1] != ':')
			{
				return false;
			}
			addressText = endpoint.substr(1, closing - 1);
			portText = std::string_view(endpoint).substr(closing + 2);
		}
		else
		{
			const auto separator = endpoint.rfind(':');
			if (separator == std::string::npos)
				return false;
			addressText = endpoint.substr(0, separator);
			portText = std::string_view(endpoint).substr(separator + 1);
		}

		unsigned int port{};
		const auto [position, parseError] = std::from_chars(
			portText.data(), portText.data() + portText.size(), port);
		if (parseError != std::errc{}
			|| position != portText.data() + portText.size()
			|| port == 0 || port > 65535)
		{
			return false;
		}
		boost::system::error_code error;
		const auto address =
			boost::asio::ip::make_address(addressText, error);
		if (error)
			return false;
		const lt::tcp::endpoint parsed(
			address, static_cast<std::uint16_t>(port));
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return false;
		}
		item->second.connect_peer(parsed);
		return true;
	}

	void LibtorrentHandle::AdjustQueuePosition(
		std::string const& taskId,
		const std::string_view operation)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		if (operation == "increasePrio")
			item->second.queue_position_up();
		else if (operation == "decreasePrio")
			item->second.queue_position_down();
		else if (operation == "topPrio")
			item->second.queue_position_top();
		else if (operation == "bottomPrio")
			item->second.queue_position_bottom();
	}

	void LibtorrentHandle::SetAutoManaged(
		std::string const& taskId, const bool enabled)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		if (enabled)
			item->second.set_flags(lt::torrent_flags::auto_managed);
		else
			item->second.unset_flags(lt::torrent_flags::auto_managed);
	}

	void LibtorrentHandle::SetForceStart(
		std::string const& taskId, const bool enabled)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		if (enabled)
		{
			item->second.unset_flags(lt::torrent_flags::auto_managed);
			item->second.resume();
		}
		else
		{
			item->second.set_flags(lt::torrent_flags::auto_managed);
		}
	}

	void LibtorrentHandle::SetSuperSeeding(
		std::string const& taskId, const bool enabled)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item != m_taskIdToHandle.end()
			&& item->second.is_valid())
		{
			if (enabled)
			{
				item->second.set_flags(lt::torrent_flags::super_seeding);
			}
			else
			{
				item->second.unset_flags(lt::torrent_flags::super_seeding);
			}
		}
	}

	void LibtorrentHandle::ToggleSequentialDownload(
		std::string const& taskId)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		if (item->second.flags()
			& lt::torrent_flags::sequential_download)
		{
			item->second.unset_flags(
				lt::torrent_flags::sequential_download);
		}
		else
		{
			item->second.set_flags(
				lt::torrent_flags::sequential_download);
		}
	}

	void LibtorrentHandle::ToggleFirstLastPiecePriority(
		std::string const& taskId)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item == m_taskIdToHandle.end()
			|| !item->second.is_valid())
		{
			return;
		}
		auto priorities = item->second.get_piece_priorities();
		if (priorities.empty())
			return;
		const bool enabled =
			static_cast<std::uint8_t>(priorities.front()) > 4
			&& static_cast<std::uint8_t>(priorities.back()) > 4;
		const auto priority = static_cast<lt::download_priority_t>(
			static_cast<std::uint8_t>(enabled ? 4 : 7));
		priorities.front() = priority;
		priorities.back() = priority;
		item->second.prioritize_pieces(priorities);
	}

	void LibtorrentHandle::MoveStorage(
		std::string const& taskId,
		std::string const& path)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item != m_taskIdToHandle.end()
			&& item->second.is_valid())
		{
			item->second.move_storage(path);
		}
	}

	void LibtorrentHandle::RenameFile(
		std::string const& taskId,
		const int fileIndex,
		std::string const& newPath)
	{
		std::lock_guard lock(m_torrentMapMutex);
		const auto item = m_taskIdToHandle.find(taskId);
		if (item != m_taskIdToHandle.end()
			&& item->second.is_valid()
			&& fileIndex >= 0)
		{
			item->second.rename_file(
				lt::file_index_t{ fileIndex }, newPath);
		}
	}

	// ---------------------------------------------------------------
	//  Set file priorities
	// ---------------------------------------------------------------
	void LibtorrentHandle::SetFilePriorities(
		std::string const& taskId,
		std::vector<int> const& priorities)
	{
		std::lock_guard lk(m_torrentMapMutex);
		auto it = m_taskIdToHandle.find(taskId);
		if (it == m_taskIdToHandle.end() || !it->second.is_valid())
			return;

		std::vector<lt::download_priority_t> ltPri;
		ltPri.reserve(priorities.size());
		for (int p : priorities)
		{
			ltPri.push_back(static_cast<lt::download_priority_t>(
				static_cast<std::uint8_t>(std::clamp(p, 0, 7))));
		}
		it->second.prioritize_files(ltPri);
	}

	// ---------------------------------------------------------------
	//  Torrent count
	// ---------------------------------------------------------------
	int LibtorrentHandle::GetTorrentCount() const
	{
		std::lock_guard lk(m_torrentMapMutex);
		return static_cast<int>(m_taskIdToHandle.size());
	}
}
