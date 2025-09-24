#include "pch.h"
#include "libtorrentHandle.h"

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert.hpp>

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
        // 可根据需要增加更多设置
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
            atp.flags |= lt::torrent_flags::seed_mode; // 可根据需要调整
            m_session->async_add_torrent(atp);
            return true;
        }
        catch (std::exception const& ex)
        {
            std::lock_guard lk(m_cbMutex);
            if (m_errorCb) m_errorCb(std::string("AddMagnet error: ") + ex.what());
            return false;
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
                }
            }
            else if (auto tf = lt::alert_cast<lt::torrent_finished_alert>(a))
            {
                std::lock_guard lk(m_cbMutex);
                if (m_finishedCb)
                {
                    try
                    {
                        auto status = tf->handle.status();
                        m_finishedCb(status.name);
                    }
                    catch (...) {}
                }
            }
            else if (auto se = lt::alert_cast<lt::session_error_alert>(a))
            {
                std::lock_guard lk(m_cbMutex);
                if (m_errorCb) m_errorCb(se->message());
            }
            else if (auto te = lt::alert_cast<lt::torrent_error_alert>(a))
            {
                std::lock_guard lk(m_cbMutex);
                if (m_errorCb) m_errorCb(te->message());
            }
            else if (auto fe = lt::alert_cast<lt::file_error_alert>(a))
            {
                std::lock_guard lk(m_cbMutex);
                if (m_errorCb) m_errorCb(fe->message());
            }
        }
    }
}