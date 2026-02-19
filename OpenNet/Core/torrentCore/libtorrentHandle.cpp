#include "pch.h"
#include "libtorrentHandle.h"
#include "TorrentStateManager.h"

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/read_resume_data.hpp>

#include <chrono>
#include <thread>
#include <iostream>

namespace lt = libtorrent;
using namespace std::chrono_literals;

namespace OpenNet::Core::Torrent
{
    LibtorrentHandle::LibtorrentHandle() {}
    LibtorrentHandle::~LibtorrentHandle()
    {
        // Save all resume data before stopping
        SaveAllResumeData();
        Stop();
    }

    bool LibtorrentHandle::Initialize()
    {
        if (m_session) return true; // 已初始化
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
        catch (std::exception const& ex)
        {
            std::lock_guard lk(m_cbMutex);
            if (m_errorCb) m_errorCb(std::string("Session init error: ") + ex.what());
            return false;
        }
        return true;
    }

    void LibtorrentHandle::ConfigureDefaultSettings(lt::settings_pack& pack)
    {
        pack.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:6881,[::]:6881");
        pack.set_bool(lt::settings_pack::enable_dht, true);
        pack.set_bool(lt::settings_pack::enable_lsd, true);
        pack.set_bool(lt::settings_pack::enable_upnp, true);
        pack.set_bool(lt::settings_pack::enable_natpmp, true);
        pack.set_bool(lt::settings_pack::enable_incoming_tcp, true);
        pack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
        pack.set_bool(lt::settings_pack::enable_incoming_utp, true);
        pack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
        // Enable alerts for resume data
        pack.set_int(lt::settings_pack::alert_mask, 
            lt::alert_category::status | 
            lt::alert_category::error |
            lt::alert_category::storage);
    }

    void LibtorrentHandle::Start()
    {
        if (!Initialize()) return;
        if (m_running.load()) return;
        m_stopRequested = false;
        m_running = true;
        m_thread = std::thread(&LibtorrentHandle::AlertLoop, this);
    }

    void LibtorrentHandle::Stop()
    {
        // Save session state before stopping
        if (m_session && m_stateManager)
        {
            m_stateManager->SaveSessionState(*m_session);
        }

        m_stopRequested = true;
        if (m_running.exchange(false))
        {
            if (m_thread.joinable()) m_thread.join();
        }
        if (m_session)
        {
            try { m_session.reset(); }
            catch (...) {}
        }
    }

    bool LibtorrentHandle::AddMagnet(std::string const& magnetUri, std::string const& savePath)
    {
        if (!Initialize()) return false;
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
                    std::chrono::system_clock::now().time_since_epoch()).count();
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
        catch (std::exception const& ex)
        {
            std::lock_guard lk(m_cbMutex);
            if (m_errorCb) m_errorCb(std::string("AddMagnet error: ") + ex.what());
            return false;
        }
    }

    std::string LibtorrentHandle::AddTorrentFromResumeData(std::string const& taskId)
    {
        if (!Initialize()) return "";
        if (!m_stateManager) return "";

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
            if (m_errorCb) m_errorCb(std::string("AddTorrentFromResumeData error: ") + ex.what());
            return "";
        }
    }

    void LibtorrentHandle::PauseTorrent(std::string const& taskId)
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

    void LibtorrentHandle::ResumeTorrent(std::string const& taskId)
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

    void LibtorrentHandle::RemoveTorrent(std::string const& taskId, bool deleteFiles)
    {
        lt::torrent_handle handle;
        {
            std::lock_guard lk(m_torrentMapMutex);
            auto it = m_taskIdToHandle.find(taskId);
            if (it == m_taskIdToHandle.end()) return;
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
        if (!m_session) return;

        std::lock_guard lk(m_torrentMapMutex);
        for (auto const& [taskId, handle] : m_taskIdToHandle)
        {
            if (handle.is_valid())
            {
                RequestResumeDataForTorrent(handle);
            }
        }
    }

    void LibtorrentHandle::SetStateManager(TorrentStateManager* stateManager)
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
                catch (...) {}
            }
        }
        return "";
    }

    void LibtorrentHandle::AlertLoop()
    {
        while (!m_stopRequested.load())
        {
            if (!m_session) break;
            std::vector<lt::alert*> alerts;
            m_session->pop_alerts(&alerts);
            if (!alerts.empty())
            {
                DispatchAlerts(alerts);
            }
            if (m_session)
            {
                m_session->post_torrent_updates();
            }
            std::this_thread::sleep_for(500ms);
        }
    }

    void LibtorrentHandle::DispatchAlerts(std::vector<lt::alert*> const& alerts)
    {
        for (lt::alert* a : alerts)
        {
            if (auto st = lt::alert_cast<lt::state_update_alert>(a))
            {
                std::lock_guard lk(m_cbMutex);
                if (!m_progressCb) continue;
                for (auto const& s : st->status)
                {
                    ProgressEvent evt;
                    evt.progressPercent = static_cast<int>(s.progress_ppm / 10000); // 1e6 -> %
                    evt.downloadRateKB = static_cast<int>(s.download_rate / 1000);
                    evt.uploadRateKB = static_cast<int>(s.upload_rate / 1000);
                    evt.name = s.name;
                    m_progressCb(evt);

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
            else if (auto tf = lt::alert_cast<lt::torrent_finished_alert>(a))
            {
                // Request resume data when torrent finishes
                RequestResumeDataForTorrent(tf->handle);
                
                std::lock_guard lk(m_cbMutex);
                if (m_finishedCb)
                {
                    try
                    {
                        auto status = tf->handle.status();
                        m_finishedCb(status.name);

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
                    catch (...) {}
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
                std::lock_guard lk(m_cbMutex);
                if (m_errorCb) m_errorCb(se->message());
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
                
                std::lock_guard lk(m_cbMutex);
                if (m_errorCb) m_errorCb(te->message());
            }
            else if (auto fe = lt::alert_cast<lt::file_error_alert>(a))
            {
                std::lock_guard lk(m_cbMutex);
                if (m_errorCb) m_errorCb(fe->message());
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
                    catch (...) {}
                }
            }
        }
    }

    void LibtorrentHandle::HandleSaveResumeDataAlert(lt::save_resume_data_alert const* alert)
    {
        if (!m_stateManager) return;
        if (!alert || !alert->handle.is_valid()) return;

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
    }

    void LibtorrentHandle::HandleSaveResumeDataFailedAlert(lt::save_resume_data_failed_alert const* alert)
    {
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
            }
            catch (...) {}
        }
    }
}