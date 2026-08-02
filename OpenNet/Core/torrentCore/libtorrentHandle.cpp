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
#include <libtorrent/close_reason.hpp>
#include <libtorrent/error_code.hpp>
#include "Core/ClientFilter/ClientFilterManager.h"
#include "Core/IPFilter/IPFilterManager.h"
#include <libtorrent/session_stats.hpp>
#include <libtorrent/ip_filter.hpp>
#include <libtorrent/socket_io.hpp>
#include <libtorrent/time.hpp>
#include <boost/asio/ip/address.hpp>

module OpenNet.Core.torrentCore.LibtorrentHandle;

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.IO.FileSystem;
import OpenNet.Core.Torrent.TrackerManager;
import OpenNet.Core.torrentCore.TorrentStateManager;
import OpenNet.Core.TorrentSettings;
import winrt_base;

namespace lt = libtorrent;
using namespace std::chrono_literals;

namespace OpenNet::Core::Torrent
{
	namespace
	{
		std::optional<int> NextTrackerAnnounceSeconds(
			lt::torrent_handle const& handle,
			std::string_view const url)
		{
			try
			{
				auto const now = lt::time_point_cast<lt::seconds32>(
					lt::clock_type::now());
				std::optional<int> result;
				for (auto const& tracker : handle.trackers())
				{
					if (tracker.url != url) continue;
					for (auto const& endpoint : tracker.endpoints)
					{
						for (auto const& hash : endpoint.info_hashes)
						{
							if (hash.next_announce == (lt::time_point32::min)()
								|| hash.next_announce == (lt::time_point32::max)())
								continue;
							auto const remaining = static_cast<int>(std::max<
																	std::int64_t>(0, lt::total_seconds(
																		hash.next_announce - now)));
							result = result
								? std::min(*result, remaining)
								: remaining;
						}
					}
				}
				return result;
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		std::string FormatTrackerInterval(int seconds)
		{
			auto const hours = seconds / 3600;
			seconds %= 3600;
			auto const minutes = seconds / 60;
			seconds %= 60;
			return std::format("{}:{:02}:{:02}", hours, minutes, seconds);
		}

		std::vector<std::string> TrackersForNewTask(
			std::vector<std::string> const& taskTrackers)
		{
			std::vector<std::string> result = taskTrackers;
			auto& manager = TrackerManager::Instance();
			if (manager.AutoAddToNewTorrents())
			{
				for (auto const& tracker : manager.GetEnabledTrackers())
				{
					auto url = winrt::to_string(winrt::hstring{ tracker.url });
					if (!url.empty())
					{
						result.push_back(std::move(url));
					}
				}
			}

			std::vector<std::string> unique;
			std::unordered_set<std::string> seen;
			for (auto const& url : result)
			{
				if (!url.empty() && seen.insert(url).second)
				{
					unique.push_back(url);
				}
			}
			return unique;
		}

		bool IsLibtorrentError(
			lt::error_code const& error,
			lt::errors::error_code_enum value)
		{
			return error
				&& error.category() == lt::libtorrent_category()
				&& error.value() == static_cast<int>(value);
		}

		std::string PeerDisconnectReason(
			lt::peer_disconnected_alert const& alert)
		{
			if (alert.reason == lt::close_reason_t::upload_to_upload
				|| IsLibtorrentError(
					alert.error, lt::errors::upload_upload_connection)
				|| IsLibtorrentError(alert.error, lt::errors::torrent_finished))
			{
				return "both_finished";
			}
			if (alert.reason == lt::close_reason_t::blocked
				|| IsLibtorrentError(
					alert.error, lt::errors::banned_by_ip_filter))
			{
				return "ip_filter";
			}
			if (IsLibtorrentError(alert.error, lt::errors::peer_banned)
				|| IsLibtorrentError(
					alert.error, lt::errors::too_many_corrupt_pieces))
			{
				return "anti_leech";
			}
			if (alert.error)
				return alert.error.message();

			switch (alert.reason)
			{
				case lt::close_reason_t::duplicate_peer_id:
					return "duplicate_peer_id";
				case lt::close_reason_t::torrent_removed:
					return "torrent_removed";
				case lt::close_reason_t::no_memory:
					return "no_memory";
				case lt::close_reason_t::port_blocked:
					return "port_blocked";
				case lt::close_reason_t::not_interested_upload_only:
					return "not_interested_upload_only";
				case lt::close_reason_t::timeout:
					return "timeout";
				case lt::close_reason_t::timed_out_interest:
					return "timed_out_interest";
				case lt::close_reason_t::timed_out_activity:
					return "timed_out_activity";
				case lt::close_reason_t::timed_out_handshake:
					return "timed_out_handshake";
				case lt::close_reason_t::timed_out_request:
					return "timed_out_request";
				case lt::close_reason_t::protocol_blocked:
					return "protocol_blocked";
				case lt::close_reason_t::peer_churn:
					return "peer_churn";
				case lt::close_reason_t::too_many_connections:
					return "too_many_connections";
				case lt::close_reason_t::too_many_files:
					return "too_many_files";
				default:
					return "connection_closed";
			}
		}

		void ApplyTrackers(
			lt::torrent_handle const& handle,
			std::vector<std::string> const& taskTrackers)
		{
			if (!handle.is_valid())
			{
				return;
			}

			std::unordered_set<std::string> existing;
			for (auto const& tracker : handle.trackers())
			{
				existing.insert(tracker.url);
			}
			for (auto const& url : TrackersForNewTask(taskTrackers))
			{
				if (existing.insert(url).second)
				{
					handle.add_tracker(lt::announce_entry(url));
				}
			}
		}

		std::wstring SafeTorrentFileStem(lt::torrent_info const& info)
		{
			std::ostringstream hashText;
			hashText << info.info_hashes();
			std::wstring stem = winrt::to_hstring(hashText.str()).c_str();
			for (auto& ch : stem)
			{
				if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"'
					|| ch == L'/' || ch == L'\\' || ch == L'|'
					|| ch == L'?' || ch == L'*' || std::iswspace(ch))
				{
					ch = L'_';
				}
			}
			return stem.empty() ? L"metadata" : stem;
		}

		void WriteTorrentFile(
			lt::torrent_handle const& handle,
			std::string const& downloadPath,
			bool copyToDownloadDirectory)
		{
			if (!handle.is_valid())
			{
				return;
			}
			auto info = handle.torrent_file();
			if (!info || !info->is_valid())
			{
				return;
			}

			try
			{
				lt::create_torrent torrent(*info);
				auto bytes = torrent.generate_buf();
				auto appDataDirectory = std::filesystem::path(
					winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW())
					/ L"Torrents";
				std::filesystem::create_directories(appDataDirectory);
				auto fileName = SafeTorrentFileStem(*info) + L".torrent";
				auto appDataFile = appDataDirectory / fileName;

				std::ofstream output(appDataFile, std::ios::binary | std::ios::trunc);
				output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
				output.close();

				if (copyToDownloadDirectory && !downloadPath.empty())
				{
					auto targetDirectory = std::filesystem::path(
						winrt::to_hstring(downloadPath).c_str());
					std::filesystem::create_directories(targetDirectory);
					std::filesystem::copy_file(
						appDataFile,
						targetDirectory / fileName,
						std::filesystem::copy_options::overwrite_existing);
				}
			}
			catch (std::exception const& ex)
			{
				OutputDebugStringA((
					"LibtorrentHandle: Failed to persist .torrent metadata: "
					+ std::string(ex.what()) + "\n").c_str());
			}
		}
	}

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

	bool LibtorrentHandle::AddMagnet(
		std::string const& magnetUri,
		std::string const& savePath,
		std::vector<int> const& filePriorities,
		std::vector<std::string> const& extraTrackers,
		bool startImmediately)
	{
		if (!Initialize())
			return false;
		try
		{
			lt::add_torrent_params atp = lt::parse_magnet_uri(magnetUri);
			atp.save_path = savePath; // 目标目录
			// Remove seed_mode flag for downloads
			atp.flags &= ~lt::torrent_flags::seed_mode;
			if (!startImmediately)
			{
				atp.flags &= ~lt::torrent_flags::auto_managed;
				atp.flags |= lt::torrent_flags::paused;
			}

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
			ApplyTrackers(handle, extraTrackers);

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
				metadata.status = startImmediately ? 1 : 2;
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

	bool LibtorrentHandle::AddTorrentFile(
		std::string const& torrentFilePath,
		std::string const& savePath,
		std::vector<int> const& filePriorities,
		std::vector<std::string> const& extraTrackers,
		bool startImmediately)
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
			if (!startImmediately)
			{
				atp.flags &= ~lt::torrent_flags::auto_managed;
				atp.flags |= lt::torrent_flags::paused;
			}

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
			ApplyTrackers(handle, extraTrackers);

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
				metadata.status = startImmediately ? 1 : 2;
				m_stateManager->SaveTaskMetadata(metadata);
			}

			auto& settingsDb = ::OpenNet::Core::AppSettingsDatabase::Instance();
			settingsDb.Initialize();
			WriteTorrentFile(
				handle,
				savePath,
				settingsDb.GetBool(
					::OpenNet::Core::AppSettingsDatabase::CAT_TORRENT,
					"saveTorrentCopyToDownloadDirectory").value_or(false));

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
				ApplyTrackers(handle, {});

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
		{
			std::lock_guard lock(m_peerEventMutex);
			m_peerEvents.erase(taskId);
		}
		{
			std::lock_guard lock(m_trackerLogMutex);
			m_trackerLogs.erase(taskId);
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
					if (now - m_lastIpFilterMaintenance
						>= std::chrono::seconds(5))
					{
						::OpenNet::Core::IPFilterManager::Instance()
							.MaintainTemporaryBans();
						m_lastIpFilterMaintenance = now;
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

	void LibtorrentHandle::RecordPeerEvent(
		lt::torrent_handle const& handle,
		lt::tcp::endpoint const& endpoint,
		std::string reason,
		bool isBan)
	{
		if (endpoint.address().is_unspecified())
			return;

		std::string taskId;
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const found = m_handleToTaskId.find(handle);
			if (found == m_handleToTaskId.end())
				return;
			taskId = found->second;
		}

		PeerConnectionEvent event;
		event.ip = endpoint.address().to_string();
		event.port = endpoint.port();
		event.reason = std::move(reason);
		event.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		event.isBan = isBan;

		std::lock_guard lock(m_peerEventMutex);
		auto& events = m_peerEvents[taskId];
		auto const existing = std::find_if(
			events.begin(), events.end(),
			[&](auto const& current)
		{
			return current.ip == event.ip && current.port == event.port;
		});
		if (existing != events.end())
		{
			// A ban alert is commonly followed by a generic disconnect alert.
			// Keep the stronger terminal state instead of downgrading it.
			if (existing->isBan && !event.isBan)
				return;
			events.erase(existing);
		}
		events.push_front(std::move(event));
		while (events.size() > 256)
			events.pop_back();
	}

	void LibtorrentHandle::ClearPeerEvent(
		lt::torrent_handle const& handle,
		lt::tcp::endpoint const& endpoint)
	{
		std::string taskId;
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const found = m_handleToTaskId.find(handle);
			if (found == m_handleToTaskId.end())
				return;
			taskId = found->second;
		}

		auto const ip = endpoint.address().to_string();
		auto const port = endpoint.port();
		std::lock_guard lock(m_peerEventMutex);
		auto const found = m_peerEvents.find(taskId);
		if (found == m_peerEvents.end())
			return;
		std::erase_if(
			found->second,
			[&](auto const& event)
		{
			return event.ip == ip && event.port == port;
		});
	}

	void LibtorrentHandle::RecordTrackerLog(
		lt::torrent_handle const& handle,
		std::string const& trackerUrl,
		std::string content,
		bool const isError)
	{
		if (trackerUrl.empty() || content.empty())
			return;

		std::string taskId;
		{
			std::lock_guard lock(m_torrentMapMutex);
			auto const found = m_handleToTaskId.find(handle);
			if (found == m_handleToTaskId.end())
				return;
			taskId = found->second;
		}

		TrackerLogEntry entry;
		entry.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		entry.content = std::move(content);
		entry.isError = isError;

		std::lock_guard lock(m_trackerLogMutex);
		auto& entries = m_trackerLogs[taskId][trackerUrl];
		entries.push_back(std::move(entry));
		while (entries.size() > 500)
			entries.pop_front();
	}

	void LibtorrentHandle::DispatchAlerts(std::vector<lt::alert*> const& alerts)
	{
		for (lt::alert* a : alerts)
		{
			if (auto connected = lt::alert_cast<lt::peer_connect_alert>(a))
			{
				ClearPeerEvent(connected->handle, connected->endpoint);
			}
			else if (auto ban = lt::alert_cast<lt::peer_ban_alert>(a))
			{
				RecordPeerEvent(
					ban->handle, ban->endpoint, "anti_leech", true);
			}
			else if (auto disconnected =
					 lt::alert_cast<lt::peer_disconnected_alert>(a))
			{
				auto reason = PeerDisconnectReason(*disconnected);
				auto const isBan =
					reason == "ip_filter" || reason == "anti_leech";
				RecordPeerEvent(
					disconnected->handle,
					disconnected->endpoint,
					std::move(reason),
					isBan);
			}
			else if (auto peerError =
					 lt::alert_cast<lt::peer_error_alert>(a))
			{
				auto reason = peerError->error
					? peerError->error.message()
					: std::string{ "peer_error" };
				auto const isBan =
					IsLibtorrentError(
						peerError->error, lt::errors::banned_by_ip_filter)
					|| IsLibtorrentError(
						peerError->error, lt::errors::peer_banned)
					|| IsLibtorrentError(
						peerError->error, lt::errors::too_many_corrupt_pieces);
				if (isBan)
				{
					reason = IsLibtorrentError(
						peerError->error, lt::errors::banned_by_ip_filter)
						? "ip_filter"
						: "anti_leech";
				}
				RecordPeerEvent(
					peerError->handle,
					peerError->endpoint,
					std::move(reason),
					isBan);
			}
			else if (auto trackerError =
					 lt::alert_cast<lt::tracker_error_alert>(a))
			{
				auto content = std::string{ "Tracker connection error: " };
				if (trackerError->error)
					content += trackerError->error.message();
				if (auto const reason = trackerError->failure_reason();
					reason && *reason)
				{
					if (trackerError->error)
						content += " - ";
					content += reason;
				}
				RecordTrackerLog(
					trackerError->handle,
					trackerError->tracker_url(),
					std::move(content), true);
			}
			else if (auto warning =
					 lt::alert_cast<lt::tracker_warning_alert>(a))
			{
				RecordTrackerLog(
					warning->handle,
					warning->tracker_url(),
					std::string{ "Tracker warning: " }
				+ warning->warning_message(), true);
			}
			else if (auto scrape =
					 lt::alert_cast<lt::scrape_reply_alert>(a))
			{
				RecordTrackerLog(
					scrape->handle,
					scrape->tracker_url(),
					"Tracker returned info: complete = "
					+ std::to_string(scrape->complete)
					+ ", incomplete = "
					+ std::to_string(scrape->incomplete));
			}
			else if (auto scrapeFailed =
					 lt::alert_cast<lt::scrape_failed_alert>(a))
			{
				auto content = std::string{ "Tracker scrape error: " };
				if (scrapeFailed->error)
					content += scrapeFailed->error.message();
				else if (auto const message = scrapeFailed->error_message())
					content += message;
				RecordTrackerLog(
					scrapeFailed->handle,
					scrapeFailed->tracker_url(),
					std::move(content), true);
			}
			else if (auto reply =
					 lt::alert_cast<lt::tracker_reply_alert>(a))
			{
				auto const trackerUrl = std::string{ reply->tracker_url() };
				RecordTrackerLog(
					reply->handle,
					trackerUrl,
					"Logged in; Tracker returned "
					+ std::to_string(reply->num_peers) + " peers");
				if (auto const remaining = NextTrackerAnnounceSeconds(
					reply->handle, trackerUrl))
				{
					RecordTrackerLog(
						reply->handle, trackerUrl,
						"Schedule next announce in "
						+ FormatTrackerInterval(*remaining));
				}
			}
			else if (auto announce =
					 lt::alert_cast<lt::tracker_announce_alert>(a))
			{
				RecordTrackerLog(
					announce->handle,
					announce->tracker_url(),
					std::string{ "Announce " } + announce->tracker_url());
				RecordTrackerLog(
					announce->handle,
					announce->tracker_url(),
					"Start connecting...");
			}
			else if (auto st = lt::alert_cast<lt::state_update_alert>(a))
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
						evt.isPaused =
							(s.flags & lt::torrent_flags::paused)
							!= lt::torrent_flags_t{};
						evt.isFinished = s.is_finished;
						evt.isSeeding = s.is_seeding;
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
				// Update the task and persist a reusable .torrent file as soon
				// as a magnet has received its metadata.
				if (m_stateManager && ma->handle.is_valid())
				{
					try
					{
						auto status = ma->handle.status();
						std::string taskId;
						{
							std::lock_guard mapLk(m_torrentMapMutex);
							auto it = m_handleToTaskId.find(ma->handle);
							if (it != m_handleToTaskId.end())
							{
								taskId = it->second;
							}
						}
						if (!taskId.empty())
						{
							auto metaOpt = m_stateManager->LoadTaskMetadata(taskId);
							if (metaOpt.has_value())
							{
								TaskMetadata meta = metaOpt.value();
								meta.name = status.name;
								meta.totalSize = status.total_wanted;
								m_stateManager->SaveTaskMetadata(meta);

								auto& settingsDb =
									::OpenNet::Core::AppSettingsDatabase::Instance();
								settingsDb.Initialize();
								WriteTorrentFile(
									ma->handle,
									meta.savePath,
									settingsDb.GetBool(
										::OpenNet::Core::AppSettingsDatabase::CAT_TORRENT,
										"saveTorrentCopyToDownloadDirectory")
									.value_or(false));
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
				auto const metrics = lt::session_stats_metrics();
				std::lock_guard lkStats(m_sessionStatsMutex);
				if (m_sessionMetricValues.empty())
					m_sessionMetricValues.reserve(metrics.size());
				for (auto const& metric : metrics)
				{
					if (metric.value_index >= 0
						&& metric.value_index < static_cast<int>(counters.size()))
					{
						m_sessionMetricValues[metric.name] =
							counters[metric.value_index];
					}
				}
				if (m_sessionStatsMetricIdxRecvBytes >= 0)
					m_sessionTotalDownload = counters[m_sessionStatsMetricIdxRecvBytes];
				if (m_sessionStatsMetricIdxSentBytes >= 0)
					m_sessionTotalUpload = counters[m_sessionStatsMetricIdxSentBytes];
				if (m_sessionStatsMetricIdxDhtNodes >= 0)
					m_cachedDhtNodeCount.store(static_cast<int>(counters[m_sessionStatsMetricIdxDhtNodes]));
				if (m_sessionStatsMetricIdxDiskBlocksInUse >= 0)
					m_sessionDiskBlocksInUse = counters[m_sessionStatsMetricIdxDiskBlocksInUse];
				if (m_sessionStatsMetricIdxDhtBytesReceived >= 0)
					m_sessionDhtBytesReceived = counters[m_sessionStatsMetricIdxDhtBytesReceived];
				if (m_sessionStatsMetricIdxDhtBytesSent >= 0)
					m_sessionDhtBytesSent = counters[m_sessionStatsMetricIdxDhtBytesSent];
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
			else if (m.name == std::string("disk.disk_blocks_in_use"))
				m_sessionStatsMetricIdxDiskBlocksInUse = m.value_index;
			else if (m.name == std::string("dht.dht_bytes_in"))
				m_sessionStatsMetricIdxDhtBytesReceived = m.value_index;
			else if (m.name == std::string("dht.dht_bytes_out"))
				m_sessionStatsMetricIdxDhtBytesSent = m.value_index;
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

	LibtorrentHandle::ListenStatus LibtorrentHandle::GetListenStatus() const
	{
		ListenStatus status{};
		if (!m_session)
		{
			return status;
		}

		try
		{
			status.isListening = m_session->is_listening();
			status.port = status.isListening
				? static_cast<int>(m_session->listen_port())
				: 0;
		}
		catch (...)
		{
			status.isListening = false;
			status.port = 0;
		}

		{
			std::lock_guard lock(m_listenStateMutex);
			status.error = m_lastListenError;
		}
		return status;
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
				stats.numSeeds += st.num_seeds;
				if ((st.flags & lt::torrent_flags::paused)
					!= lt::torrent_flags_t{})
				{
					++stats.numPausedTorrents;
				}
				else
				{
					++stats.numRunningTorrents;
				}

				switch (st.state)
				{
					case lt::torrent_status::downloading_metadata:
						++stats.numMetadataTorrents;
						++stats.numDownloadingTorrents;
						break;
					case lt::torrent_status::downloading:
						++stats.numDownloadingTorrents;
						break;
					case lt::torrent_status::finished:
					case lt::torrent_status::seeding:
						++stats.numSeedingTorrents;
						break;
					case lt::torrent_status::checking_files:
					case lt::torrent_status::checking_resume_data:
						++stats.numCheckingTorrents;
						break;
					default:
						break;
				}
				if (st.errc)
					++stats.numErrorTorrents;
				if (st.state == lt::torrent_status::finished
					|| st.state == lt::torrent_status::seeding)
				{
					stats.longTermSeedingUploadRate += st.upload_rate;
				}
			}

			// DHT nodes — use the cached value from dht_stats_alert / session_stats_alert
			stats.dhtNodes = m_cachedDhtNodeCount.load();

			// Only expose the socket libtorrent actually opened. Reading this
			// through the lightweight status API keeps every UI consumer aligned.
			auto const listenStatus = GetListenStatus();
			stats.isListening = listenStatus.isListening;
			stats.listenPort = listenStatus.port;
			stats.listenError = listenStatus.error;

			// Session-level totals from session_stats_alert (more accurate than per-torrent sums)
			{
				std::lock_guard lkStats(m_sessionStatsMutex);
				if (m_sessionTotalDownload > 0)
					stats.totalDownloaded = m_sessionTotalDownload;
				if (m_sessionTotalUpload > 0)
					stats.totalUploaded = m_sessionTotalUpload;
				// Libtorrent's disk block metric counts 16 KiB blocks.
				stats.diskCacheBytes = m_sessionDiskBlocksInUse * 16 * 1024;
				stats.dhtBytesReceived = m_sessionDhtBytesReceived;
				stats.dhtBytesSent = m_sessionDhtBytesSent;
			}
		}
		catch (...)
		{
		}

		return stats;
	}

	std::unordered_map<std::string, std::int64_t>
		LibtorrentHandle::GetSessionMetrics() const
	{
		std::lock_guard lock(m_sessionStatsMutex);
		return m_sessionMetricValues;
	}

	// ---------------------------------------------------------------
	//  Per-torrent detail
	// ---------------------------------------------------------------
	std::vector<LibtorrentHandle::PeerConnectionEvent>
		LibtorrentHandle::GetRecentPeerEvents(
			std::string const& taskId,
			std::int64_t maxAgeSeconds) const
	{
		std::vector<PeerConnectionEvent> result;
		auto const cutoff = maxAgeSeconds > 0
			? std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()).count()
			- maxAgeSeconds
			: 0;

		std::lock_guard lock(m_peerEventMutex);
		auto const found = m_peerEvents.find(taskId);
		if (found == m_peerEvents.end())
			return result;
		result.reserve(found->second.size());
		for (auto const& event : found->second)
		{
			if (cutoff == 0 || event.timestamp >= cutoff)
				result.push_back(event);
		}
		return result;
	}

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
			info.numComplete = st.num_complete;
			info.numIncomplete = st.num_incomplete;
			info.state = static_cast<int>(st.state);
			info.addedTimestamp =
				static_cast<std::int64_t>(st.added_time);
			info.completedTimestamp =
				static_cast<std::int64_t>(st.completed_time);
			info.activeTimeSeconds = st.active_duration.count();
			info.seedingTimeSeconds = st.seeding_duration.count();
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
				info.creator = ti->creator();
				info.creationTimestamp = static_cast<std::int64_t>(ti->creation_date());
				info.isPrivate = ti->priv();
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
				ti.status = "not contacted";
				bool contacted = false;
				bool updating = false;
				bool failed = false;
				auto const now = lt::time_point_cast<lt::seconds32>(
					lt::clock_type::now());

				// A tracker can have several listen endpoints and both v1/v2
				// info-hashes. Aggregate every active state instead of reading
				// only the first slot (which is often an unused v1 entry).
				for (auto const& endpoint : t.endpoints)
				{
					for (auto const& ih : endpoint.info_hashes)
					{
						ti.retries = std::max(
							ti.retries, static_cast<int>(ih.fails));
						updating = updating || ih.updating;
						failed = failed || ih.fails > 0 || bool(ih.last_error);
						contacted = contacted || ih.start_sent || ih.complete_sent
							|| ih.updating || ih.fails > 0 || bool(ih.last_error)
							|| !ih.message.empty() || ih.scrape_complete >= 0
							|| ih.scrape_incomplete >= 0;

						if (ih.scrape_complete >= 0)
							ti.seeders = std::max(ti.seeders, ih.scrape_complete);
						if (ih.scrape_incomplete >= 0)
							ti.leechers = std::max(ti.leechers, ih.scrape_incomplete);
						if (ih.scrape_downloaded >= 0)
							ti.downloaded = std::max(
								ti.downloaded, ih.scrape_downloaded);

						if (ih.next_announce != (lt::time_point32::min)()
							&& ih.next_announce != (lt::time_point32::max)())
						{
							auto const remaining = static_cast<int>(std::max<
																	std::int64_t>(0, lt::total_seconds(
																		ih.next_announce - now)));
							if (ti.nextAnnounceSeconds < 0)
								ti.nextAnnounceSeconds = remaining;
							else
								ti.nextAnnounceSeconds = std::min(
									ti.nextAnnounceSeconds, remaining);
						}

						if (ti.message.empty())
						{
							if (ih.last_error)
								ti.message = ih.last_error.message();
							else if (!ih.message.empty())
								ti.message = ih.message;
						}
					}
				}

				if (ti.seeders >= 0 && ti.leechers >= 0)
					ti.numPeers = ti.seeders + ti.leechers;
				if (updating)
					ti.status = "updating";
				else if (failed)
					ti.status = "error";
				else if (contacted)
					ti.status = "working";

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
				info.isPieceAligned = info.pieceSize > 0;
				for (int i = 0; i < numFiles && info.isPieceAligned; ++i)
				{
					auto const fileIndex = lt::file_index_t{ i };
					if (!fs.pad_file_at(fileIndex)
						&& fs.file_offset(fileIndex) % info.pieceSize != 0)
					{
						info.isPieceAligned = false;
					}
				}

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
			const auto priorities = handle.get_piece_priorities();
			for (int index = 0;
				 index < pieceCount
				 && index < status.pieces.size();
				 ++index)
			{
				if (status.pieces[lt::piece_index_t{ index }])
					result.states[static_cast<std::size_t>(index)] = 2;
				else if (index < static_cast<int>(priorities.size())
						 && static_cast<std::uint8_t>(priorities[
							 static_cast<std::size_t>(index)]) == 0)
					result.states[static_cast<std::size_t>(index)] = 3;
				else if (status.state == lt::torrent_status::checking_files)
					result.states[static_cast<std::size_t>(index)] = 4;
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

	void LibtorrentHandle::ForceReannounceTracker(
		std::string const& taskId,
		std::string const& trackerUrl)
	{
		std::lock_guard lock(m_torrentMapMutex);
		auto const found = m_taskIdToHandle.find(taskId);
		if (found == m_taskIdToHandle.end() || !found->second.is_valid())
			return;

		auto const trackers = found->second.trackers();
		for (std::size_t index = 0; index < trackers.size(); ++index)
		{
			if (trackers[index].url != trackerUrl)
				continue;
			found->second.force_reannounce(
				0, static_cast<int>(index),
				lt::torrent_handle::ignore_min_interval);
			found->second.scrape_tracker(static_cast<int>(index));
			return;
		}
	}

	std::vector<LibtorrentHandle::TrackerLogEntry>
		LibtorrentHandle::GetTrackerLog(
			std::string const& taskId,
			std::string const& trackerUrl) const
	{
		std::lock_guard lock(m_trackerLogMutex);
		auto const task = m_trackerLogs.find(taskId);
		if (task == m_trackerLogs.end())
			return {};
		auto const tracker = task->second.find(trackerUrl);
		if (tracker == task->second.end())
			return {};
		return std::vector<TrackerLogEntry>(
			tracker->second.begin(), tracker->second.end());
	}

	std::optional<LibtorrentHandle::TrackerLogEntry>
		LibtorrentHandle::GetLatestTrackerLog(
			std::string const& taskId,
			std::string const& trackerUrl) const
	{
		std::lock_guard lock(m_trackerLogMutex);
		auto const task = m_trackerLogs.find(taskId);
		if (task == m_trackerLogs.end())
			return std::nullopt;
		auto const tracker = task->second.find(trackerUrl);
		if (tracker == task->second.end() || tracker->second.empty())
			return std::nullopt;
		return tracker->second.back();
	}

	void LibtorrentHandle::ClearTrackerLog(
		std::string const& taskId,
		std::string const& trackerUrl)
	{
		std::lock_guard lock(m_trackerLogMutex);
		auto const task = m_trackerLogs.find(taskId);
		if (task == m_trackerLogs.end())
			return;
		task->second.erase(trackerUrl);
		if (task->second.empty())
			m_trackerLogs.erase(task);
	}

	void LibtorrentHandle::ClearTrackerLogs(std::string const& taskId)
	{
		std::lock_guard lock(m_trackerLogMutex);
		m_trackerLogs.erase(taskId);
	}

	void LibtorrentHandle::ClearAllTrackerLogs()
	{
		std::lock_guard lock(m_trackerLogMutex);
		m_trackerLogs.clear();
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
		RequestResumeDataForTorrent(item->second);
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
		RequestResumeDataForTorrent(item->second);
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
		RequestResumeDataForTorrent(item->second);
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
