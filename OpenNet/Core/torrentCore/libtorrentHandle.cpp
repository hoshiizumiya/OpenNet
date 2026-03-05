#include "pch.h"
#include "libtorrentHandle.h"
#include "TorrentStateManager.h"
#include "Core/TorrentSettings.h"

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/session_stats.hpp>
#include <libtorrent/ip_filter.hpp>

#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace lt = libtorrent;
using namespace std::chrono_literals;

namespace OpenNet::Core::Torrent
{
    LibtorrentHandle::LibtorrentHandle() {}
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
            auto pack = std::make_unique<lt::settings_pack>();
            ConfigureDefaultSettings(*pack);
            m_session = std::make_unique<lt::session>(*pack);

            // Load saved session state if state manager is available
            if (m_stateManager)
            {
                m_stateManager->LoadSessionState(*m_session);
            }
        }
        catch (std::exception const &ex)
        {
            std::lock_guard lk(m_cbMutex);
            if (m_errorCb)
                m_errorCb(std::string("Session init error: ") + ex.what());
            return false;
        }
        return true;
    }

    void LibtorrentHandle::ConfigureDefaultSettings(lt::settings_pack &pack)
    {
        // Load persistent settings and apply to pack
        auto &settingsMgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
        settingsMgr.Load();
        auto settings = settingsMgr.Get();

        // Use the shared builder that maps TorrentSettings -> settings_pack
        ::OpenNet::Core::ApplyTorrentSettingsToSettingsPack(settings, pack);
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
        if (m_session)
        {
            try
            {
                m_session->post_torrent_updates();
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
            catch (const std::exception &ex)
            {
                OutputDebugStringW((L"LibtorrentHandle: Error saving session: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
            }
        }

        // Clear session
        if (m_session)
        {
            try
            {
                OutputDebugStringA("LibtorrentHandle: Clearing session\n");
                m_session.reset();
            }
            catch (const std::exception &ex)
            {
                OutputDebugStringW((L"LibtorrentHandle: Error clearing session: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
            }
            catch (...)
            {
                OutputDebugStringA("LibtorrentHandle: Unknown error clearing session\n");
            }
        }

        OutputDebugStringA("LibtorrentHandle: Stop completed\n");
    }

    bool LibtorrentHandle::AddMagnet(std::string const &magnetUri, std::string const &savePath)
    {
        if (!Initialize())
            return false;
        try
        {
            lt::add_torrent_params atp = lt::parse_magnet_uri(magnetUri);
            atp.save_path = savePath; // 目标目录
            // Remove seed_mode flag for downloads
            atp.flags &= ~lt::torrent_flags::seed_mode;

            // Generate task ID and save metadata
            std::string taskId = TorrentStateManager::GenerateTaskId();

            if (m_stateManager)
            {
                TaskMetadata metadata;
                metadata.taskId = taskId;
                metadata.magnetUri = magnetUri;
                metadata.savePath = savePath;
                metadata.name = ""; // Will be updated when metadata is received
                metadata.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count();
                metadata.status = 1; // Downloading
                m_stateManager->SaveTaskMetadata(metadata);
            }

            lt::torrent_handle handle = m_session->add_torrent(atp);

            // Store mapping
            {
                std::lock_guard lk(m_torrentMapMutex);
                m_taskIdToHandle[taskId] = handle;
                m_handleToTaskId[handle] = taskId;
            }

            return true;
        }
        catch (std::exception const &ex)
        {
            std::lock_guard lk(m_cbMutex);
            if (m_errorCb)
                m_errorCb(std::string("AddMagnet error: ") + ex.what());
            return false;
        }
    }

    bool LibtorrentHandle::AddTorrentFile(std::string const &torrentFilePath, std::string const &savePath)
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

            // Generate task ID and save metadata
            std::string taskId = TorrentStateManager::GenerateTaskId();

            if (m_stateManager)
            {
                TaskMetadata metadata;
                metadata.taskId = taskId;
                metadata.magnetUri = ""; // Not a magnet, store file path reference if needed
                metadata.savePath = savePath;
                metadata.name = ti.name();
                metadata.totalSize = ti.total_size();
                metadata.addedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count();
                metadata.status = 1; // Downloading
                m_stateManager->SaveTaskMetadata(metadata);
            }

            lt::torrent_handle handle = m_session->add_torrent(atp);

            // Store mapping
            {
                std::lock_guard lk(m_torrentMapMutex);
                m_taskIdToHandle[taskId] = handle;
                m_handleToTaskId[handle] = taskId;
            }

            return true;
        }
        catch (std::exception const &ex)
        {
            std::lock_guard lk(m_cbMutex);
            if (m_errorCb)
                m_errorCb(std::string("AddTorrentFile error: ") + ex.what());
            return false;
        }
    }

    std::string LibtorrentHandle::AddTorrentFromResumeData(std::string const &taskId)
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
        catch (std::exception const &ex)
        {
            std::lock_guard lk(m_cbMutex);
            if (m_errorCb)
                m_errorCb(std::string("AddTorrentFromResumeData error: ") + ex.what());
            return "";
        }
    }

    void LibtorrentHandle::PauseTorrent(std::string const &taskId)
    {
        std::lock_guard lk(m_torrentMapMutex);
        auto it = m_taskIdToHandle.find(taskId);
        if (it != m_taskIdToHandle.end() && it->second.is_valid())
        {
            it->second.pause();
            if (m_stateManager)
            {
                m_stateManager->UpdateTaskStatus(taskId, 2); // Paused
            }
        }
    }

    void LibtorrentHandle::ResumeTorrent(std::string const &taskId)
    {
        std::lock_guard lk(m_torrentMapMutex);
        auto it = m_taskIdToHandle.find(taskId);
        if (it != m_taskIdToHandle.end() && it->second.is_valid())
        {
            it->second.resume();
            if (m_stateManager)
            {
                m_stateManager->UpdateTaskStatus(taskId, 1); // Downloading
            }
        }
    }

    void LibtorrentHandle::RemoveTorrent(std::string const &taskId, bool deleteFiles)
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
        for (auto const &[taskId, handle] : m_taskIdToHandle)
        {
            if (handle.is_valid())
            {
                RequestResumeDataForTorrent(handle);
            }
        }
    }

    void LibtorrentHandle::SetStateManager(TorrentStateManager *stateManager)
    {
        m_stateManager = stateManager;
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

    std::string LibtorrentHandle::GetTaskIdByName(std::string const &name) const
    {
        std::lock_guard lk(m_torrentMapMutex);
        for (auto const &[taskId, handle] : m_taskIdToHandle)
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

        while (!m_stopRequested.load())
        {
            if (!m_session)
            {
                OutputDebugStringA("LibtorrentHandle: No session in AlertLoop, breaking\n");
                break;
            }

            try
            {
                std::vector<lt::alert *> alerts;
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
                    std::lock_guard lk(m_torrentMapMutex);
                    if (!m_taskIdToHandle.empty())
                    {
                        m_session->post_torrent_updates();
                    }
                    // Time-gated stats requests: only every 2 seconds to avoid
                    // self-excitation (these calls generate alerts that reset
                    // emptyAlertCount, preventing adaptive backoff from engaging)
                    auto now = std::chrono::steady_clock::now();
                    if (now - m_lastStatsRequest >= std::chrono::seconds(2))
                    {
                        m_session->post_session_stats();
                        m_session->post_dht_stats();
                        m_lastStatsRequest = now;
                    }
                }
            }
            catch (const std::exception &ex)
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

    void LibtorrentHandle::DispatchAlerts(std::vector<lt::alert *> const &alerts)
    {
        for (lt::alert *a : alerts)
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
                    for (auto const &s : st->status)
                    {
                        ProgressEvent evt;
                        evt.progressPercent = static_cast<int>(s.progress_ppm / 10000); // 1e6 -> %
                        evt.downloadRateKB = static_cast<int>(s.download_rate / 1000);
                        evt.uploadRateKB = static_cast<int>(s.upload_rate / 1000);
                        evt.name = s.name;
                        progressCbCopy(evt);

                        // Update progress in database
                        if (m_stateManager)
                        {
                            std::lock_guard mapLk(m_torrentMapMutex);
                            auto it = m_handleToTaskId.find(s.handle);
                            if (it != m_handleToTaskId.end())
                            {
                                m_stateManager->UpdateTaskProgress(it->second, s.total_done);
                            }
                        }
                    }
                }
            }
            else if (auto tf = lt::alert_cast<lt::torrent_finished_alert>(a))
            {
                // Request resume data when torrent finishes
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
                        finishedCbCopy(status.name);

                        // Update status in database
                        if (m_stateManager)
                        {
                            std::lock_guard mapLk(m_torrentMapMutex);
                            auto it = m_handleToTaskId.find(tf->handle);
                            if (it != m_handleToTaskId.end())
                            {
                                m_stateManager->UpdateTaskStatus(it->second, 3); // Completed
                            }
                        }
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

    void LibtorrentHandle::HandleSaveResumeDataAlert(lt::save_resume_data_alert const *alert)
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
        catch (std::exception const &ex)
        {
            OutputDebugStringA(("HandleSaveResumeDataAlert error: " + std::string(ex.what()) + "\n").c_str());
        }
        m_pendingResumeDataCount.fetch_sub(1);
    }

    void LibtorrentHandle::HandleSaveResumeDataFailedAlert(lt::save_resume_data_failed_alert const *alert)
    {
        m_pendingResumeDataCount.fetch_sub(1);
        if (alert)
        {
            OutputDebugStringA(("Save resume data failed: " + alert->message() + "\n").c_str());
        }
    }

    void LibtorrentHandle::RequestResumeDataForTorrent(lt::torrent_handle const &handle)
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

    void LibtorrentHandle::ApplySettings(lt::settings_pack const &pack)
    {
        if (m_session)
        {
            m_session->apply_settings(pack);
        }
    }

    void LibtorrentHandle::SetIpFilter(lt::ip_filter const &filter)
    {
        if (m_session)
        {
            m_session->set_ip_filter(filter);
        }
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

            for (auto const &h : torrents)
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

            // Listen port
            try { stats.listenPort = static_cast<int>(m_session->listen_port()); }
            catch (...) { stats.listenPort = 0; }

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
        std::string const &taskId) const
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

            if (st.total_done > 0)
                info.shareRatio = static_cast<double>(st.total_upload) / st.total_done;

            // Info hash
            if (auto ti = handle.torrent_file())
            {
                std::ostringstream oss;
                oss << ti->info_hashes();
                info.infoHash = oss.str();
                info.comment = ti->comment();
            }

            // Peers
            std::vector<lt::peer_info> ltPeers;
            handle.get_peer_info(ltPeers);
            info.peers.reserve(ltPeers.size());
            for (auto const &p : ltPeers)
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
                info.peers.push_back(std::move(pi));
            }

            // Trackers
            auto ltTrackers = handle.trackers();
            info.trackers.reserve(ltTrackers.size());
            for (auto const &t : ltTrackers)
            {
                TorrentTrackerInfo ti;
                ti.url = t.url;
                ti.tier = t.tier;

                // Determine tracker status from endpoints
                if (!t.endpoints.empty())
                {
                    auto const &ep = t.endpoints.front();
                    if (!ep.info_hashes.empty())
                    {
                        auto const &ih = ep.info_hashes.front();
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
                auto const& fs = ti->files();
                auto fileProgress = handle.file_progress(lt::torrent_handle::piece_granularity);
                auto filePriorities = handle.get_file_priorities();
                int numFiles = fs.num_files();

                info.files.reserve(numFiles);
                for (int i = 0; i < numFiles; ++i)
                {
                    TorrentFileEntry fe;
                    fe.path = fs.file_path(lt::file_index_t{i});
                    fe.size = fs.file_size(lt::file_index_t{i});
                    fe.fileIndex = i;
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

    // ---------------------------------------------------------------
    //  Force recheck
    // ---------------------------------------------------------------
    void LibtorrentHandle::ForceRecheck(std::string const &taskId)
    {
        std::lock_guard lk(m_torrentMapMutex);
        auto it = m_taskIdToHandle.find(taskId);
        if (it != m_taskIdToHandle.end() && it->second.is_valid())
        {
            it->second.force_recheck();
        }
    }

    // ---------------------------------------------------------------
    //  Set file priorities
    // ---------------------------------------------------------------
    void LibtorrentHandle::SetFilePriorities(
        std::string const &taskId,
        std::vector<int> const &priorities)
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