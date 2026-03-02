/*
 * PROJECT:   OpenNet
 * FILE:      Core/TorrentSettings.cpp
 * PURPOSE:   Persistent settings for libtorrent – JSON serialization + settings_pack builder
 *
 * LICENSE:   The MIT License
 */

#include "pch.h"
#include "Core/TorrentSettings.h"

#include <nlohmann/json.hpp>

#include <libtorrent/settings_pack.hpp>
#include <libtorrent/session.hpp> // for proxy_type_t

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.h>

#include <filesystem>
#include <fstream>

using json = nlohmann::json;
namespace lt = libtorrent;

// -------------------------------------------------------------------
//  JSON serialization helpers (nlohmann)
// -------------------------------------------------------------------
namespace OpenNet::Core
{

    static void to_json(json &j, TorrentSettings const &s)
    {
        j = json{
            // Connection
            {"listenInterfaces", s.listenInterfaces},
            {"connectionsLimit", s.connectionsLimit},
            {"enableIncomingTcp", s.enableIncomingTcp},
            {"enableOutgoingTcp", s.enableOutgoingTcp},
            {"enableIncomingUtp", s.enableIncomingUtp},
            {"enableOutgoingUtp", s.enableOutgoingUtp},
            {"allowMultipleConnectionsPerIp", s.allowMultipleConnectionsPerIp},
            {"anonymousMode", s.anonymousMode},
            // Discovery
            {"enableDht", s.enableDht},
            {"enableLsd", s.enableLsd},
            {"enableUpnp", s.enableUpnp},
            {"enableNatpmp", s.enableNatpmp},
            // Tracker
            {"announceToAllTiers", s.announceToAllTiers},
            {"announceToAllTrackers", s.announceToAllTrackers},
            // Limits
            {"activeDownloads", s.activeDownloads},
            {"activeSeeds", s.activeSeeds},
            {"activeLimit", s.activeLimit},
            // Speed
            {"downloadRateLimit", s.downloadRateLimit},
            {"uploadRateLimit", s.uploadRateLimit},
            // Seeding
            {"seedingRatioLimit", s.seedingRatioLimit},
            {"seedingTimeLimit", s.seedingTimeLimit},
            // Peer
            {"peerTimeout", s.peerTimeout},
            {"handshakeTimeout", s.handshakeTimeout},
            {"closeRedundantConnections", s.closeRedundantConnections},
            {"maxPeerListSize", s.maxPeerListSize},
            // Disk
            {"aioThreads", s.aioThreads},
            {"checkingMemUsage", s.checkingMemUsage},
            // Encryption
            {"encryptionPolicy", static_cast<int>(s.encryptionPolicy)},
            {"preferRc4", s.preferRc4},
            // Proxy
            {"proxyType", static_cast<int>(s.proxyType)},
            {"proxyHostname", s.proxyHostname},
            {"proxyPort", s.proxyPort},
            {"proxyUsername", s.proxyUsername},
            {"proxyPassword", s.proxyPassword},
            {"proxyPeerConnections", s.proxyPeerConnections},
            {"proxyTrackerConnections", s.proxyTrackerConnections},
            // Identity
            {"userAgent", s.userAgent},
            {"peerFingerprint", s.peerFingerprint},
            // Download defaults
            {"defaultSavePath", s.defaultSavePath},
            {"preallocateStorage", s.preallocateStorage},
            {"autoStartDownloads", s.autoStartDownloads},
            {"moveCompletedEnabled", s.moveCompletedEnabled},
            {"moveCompletedPath", s.moveCompletedPath},
        };
    }

    static void from_json(json const &j, TorrentSettings &s)
    {
        auto get = [&](auto const &key, auto &target)
        {
            if (j.contains(key))
            {
                try
                {
                    j.at(key).get_to(target);
                }
                catch (...)
                {
                }
            }
        };

        get("listenInterfaces", s.listenInterfaces);
        get("connectionsLimit", s.connectionsLimit);
        get("enableIncomingTcp", s.enableIncomingTcp);
        get("enableOutgoingTcp", s.enableOutgoingTcp);
        get("enableIncomingUtp", s.enableIncomingUtp);
        get("enableOutgoingUtp", s.enableOutgoingUtp);
        get("allowMultipleConnectionsPerIp", s.allowMultipleConnectionsPerIp);
        get("anonymousMode", s.anonymousMode);
        get("enableDht", s.enableDht);
        get("enableLsd", s.enableLsd);
        get("enableUpnp", s.enableUpnp);
        get("enableNatpmp", s.enableNatpmp);
        get("announceToAllTiers", s.announceToAllTiers);
        get("announceToAllTrackers", s.announceToAllTrackers);
        get("activeDownloads", s.activeDownloads);
        get("activeSeeds", s.activeSeeds);
        get("activeLimit", s.activeLimit);
        get("downloadRateLimit", s.downloadRateLimit);
        get("uploadRateLimit", s.uploadRateLimit);
        get("seedingRatioLimit", s.seedingRatioLimit);
        get("seedingTimeLimit", s.seedingTimeLimit);
        get("peerTimeout", s.peerTimeout);
        get("handshakeTimeout", s.handshakeTimeout);
        get("closeRedundantConnections", s.closeRedundantConnections);
        get("maxPeerListSize", s.maxPeerListSize);
        get("aioThreads", s.aioThreads);
        get("checkingMemUsage", s.checkingMemUsage);
        get("userAgent", s.userAgent);
        get("peerFingerprint", s.peerFingerprint);
        get("defaultSavePath", s.defaultSavePath);
        get("preallocateStorage", s.preallocateStorage);
        get("autoStartDownloads", s.autoStartDownloads);
        get("moveCompletedEnabled", s.moveCompletedEnabled);
        get("moveCompletedPath", s.moveCompletedPath);

        // enums stored as int
        if (j.contains("encryptionPolicy"))
        {
            int v = 1;
            try
            {
                j.at("encryptionPolicy").get_to(v);
            }
            catch (...)
            {
            }
            s.encryptionPolicy = static_cast<EncryptionPolicy>(v);
        }
        if (j.contains("proxyType"))
        {
            int v = 0;
            try
            {
                j.at("proxyType").get_to(v);
            }
            catch (...)
            {
            }
            s.proxyType = static_cast<ProxyType>(v);
        }
        get("preferRc4", s.preferRc4);
        get("proxyHostname", s.proxyHostname);
        get("proxyPort", s.proxyPort);
        get("proxyUsername", s.proxyUsername);
        get("proxyPassword", s.proxyPassword);
        get("proxyPeerConnections", s.proxyPeerConnections);
        get("proxyTrackerConnections", s.proxyTrackerConnections);
    }

    // -------------------------------------------------------------------
    //  TorrentSettingsManager
    // -------------------------------------------------------------------

    TorrentSettingsManager &TorrentSettingsManager::Instance()
    {
        static TorrentSettingsManager s_instance;
        return s_instance;
    }

    std::wstring TorrentSettingsManager::FilePath() const
    {
        return m_filePath;
    }

    TorrentSettings TorrentSettingsManager::Get() const
    {
        std::lock_guard lk(m_mutex);
        return m_settings;
    }

    void TorrentSettingsManager::Set(TorrentSettings const &settings)
    {
        {
            std::lock_guard lk(m_mutex);
            m_settings = settings;
        }
        Save();
    }

    void TorrentSettingsManager::Load()
    {
        std::lock_guard lk(m_mutex);
        if (m_loaded)
            return;

        try
        {
            // Determine file path: <LocalFolder>/torrent_settings.json
            auto localFolder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            m_filePath = std::wstring(localFolder.Path().c_str()) + L"\\torrent_settings.json";

            if (std::filesystem::exists(m_filePath))
            {
                std::ifstream ifs(m_filePath);
                if (ifs.good())
                {
                    json j = json::parse(ifs, nullptr, /*allow_exceptions=*/false);
                    if (!j.is_discarded())
                    {
                        m_settings = j.get<TorrentSettings>();
                    }
                }
            }

            // Resolve default save path if empty
            if (m_settings.defaultSavePath.empty())
            {
                try
                {
                    auto downloads = winrt::Windows::Storage::KnownFolders::GetFolderAsync(
                                         winrt::Windows::Storage::KnownFolderId::DownloadsFolder)
                                         .get();
                    m_settings.defaultSavePath = winrt::to_string(downloads.Path());
                }
                catch (...)
                {
                    // Fallback: use LocalFolder\Downloads
                    auto path = std::wstring(localFolder.Path().c_str()) + L"\\Downloads";
                    std::filesystem::create_directories(path);
                    m_settings.defaultSavePath = winrt::to_string(winrt::hstring(path));
                }
            }
        }
        catch (std::exception const &ex)
        {
            OutputDebugStringA(("TorrentSettingsManager::Load error: " + std::string(ex.what()) + "\n").c_str());
        }

        m_loaded = true;
    }

    void TorrentSettingsManager::Save()
    {
        std::lock_guard lk(m_mutex);

        try
        {
            if (m_filePath.empty())
            {
                auto localFolder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
                m_filePath = std::wstring(localFolder.Path().c_str()) + L"\\torrent_settings.json";
            }

            json j = m_settings;
            std::ofstream ofs(m_filePath);
            if (ofs.good())
            {
                ofs << j.dump(2);
            }
        }
        catch (std::exception const &ex)
        {
            OutputDebugStringA(("TorrentSettingsManager::Save error: " + std::string(ex.what()) + "\n").c_str());
        }
    }

} // namespace OpenNet::Core

// -------------------------------------------------------------------
//  Free function: build a libtorrent settings_pack from TorrentSettings
//  This lives here so libtorrent headers stay out of TorrentSettings.h
// -------------------------------------------------------------------
namespace OpenNet::Core
{

    void ApplyTorrentSettingsToSettingsPack(
        TorrentSettings const &s,
        lt::settings_pack &pack)
    {
        // Connection
        pack.set_str(lt::settings_pack::listen_interfaces, s.listenInterfaces);
        pack.set_int(lt::settings_pack::connections_limit, s.connectionsLimit);
        pack.set_bool(lt::settings_pack::enable_incoming_tcp, s.enableIncomingTcp);
        pack.set_bool(lt::settings_pack::enable_outgoing_tcp, s.enableOutgoingTcp);
        pack.set_bool(lt::settings_pack::enable_incoming_utp, s.enableIncomingUtp);
        pack.set_bool(lt::settings_pack::enable_outgoing_utp, s.enableOutgoingUtp);
        pack.set_bool(lt::settings_pack::allow_multiple_connections_per_ip, s.allowMultipleConnectionsPerIp);
        pack.set_bool(lt::settings_pack::anonymous_mode, s.anonymousMode);

        // Discovery
        pack.set_bool(lt::settings_pack::enable_dht, s.enableDht);
        pack.set_bool(lt::settings_pack::enable_lsd, s.enableLsd);
        pack.set_bool(lt::settings_pack::enable_upnp, s.enableUpnp);
        pack.set_bool(lt::settings_pack::enable_natpmp, s.enableNatpmp);

        // Tracker
        pack.set_bool(lt::settings_pack::announce_to_all_tiers, s.announceToAllTiers);
        pack.set_bool(lt::settings_pack::announce_to_all_trackers, s.announceToAllTrackers);

        // Limits
        pack.set_int(lt::settings_pack::active_downloads, s.activeDownloads);
        pack.set_int(lt::settings_pack::active_seeds, s.activeSeeds);
        pack.set_int(lt::settings_pack::active_limit, s.activeLimit);

        // Speed limits
        pack.set_int(lt::settings_pack::download_rate_limit, s.downloadRateLimit);
        pack.set_int(lt::settings_pack::upload_rate_limit, s.uploadRateLimit);

        // Peer
        pack.set_int(lt::settings_pack::peer_timeout, s.peerTimeout);
        pack.set_int(lt::settings_pack::handshake_timeout, s.handshakeTimeout);
        pack.set_bool(lt::settings_pack::close_redundant_connections, s.closeRedundantConnections);
        pack.set_int(lt::settings_pack::max_peerlist_size, s.maxPeerListSize);

        // Disk I/O
        pack.set_int(lt::settings_pack::aio_threads, s.aioThreads);
        pack.set_int(lt::settings_pack::checking_mem_usage, s.checkingMemUsage);

        // Encryption (pe_settings mapped to settings_pack ints)
        // In libtorrent 2.x: out_enc_policy, in_enc_policy, allowed_enc_level
        int encPolicy = 1; // enabled
        switch (s.encryptionPolicy)
        {
        case EncryptionPolicy::Forced:
            encPolicy = 0;
            break; // pe_forced
        case EncryptionPolicy::Enabled:
            encPolicy = 1;
            break; // pe_enabled
        case EncryptionPolicy::Disabled:
            encPolicy = 2;
            break; // pe_disabled
        }
        pack.set_int(lt::settings_pack::out_enc_policy, encPolicy);
        pack.set_int(lt::settings_pack::in_enc_policy, encPolicy);

        int encLevel = s.preferRc4 ? 1 /*pe_rc4*/ : 3 /*pe_both*/;
        pack.set_int(lt::settings_pack::allowed_enc_level, encLevel);
        pack.set_bool(lt::settings_pack::prefer_rc4, s.preferRc4);

        // Proxy
        pack.set_int(lt::settings_pack::proxy_type, static_cast<int>(s.proxyType));
        pack.set_str(lt::settings_pack::proxy_hostname, s.proxyHostname);
        pack.set_int(lt::settings_pack::proxy_port, s.proxyPort);
        pack.set_str(lt::settings_pack::proxy_username, s.proxyUsername);
        pack.set_str(lt::settings_pack::proxy_password, s.proxyPassword);
        pack.set_bool(lt::settings_pack::proxy_peer_connections, s.proxyPeerConnections);
        pack.set_bool(lt::settings_pack::proxy_tracker_connections, s.proxyTrackerConnections);

        // Identity
        pack.set_str(lt::settings_pack::user_agent, s.userAgent);
        pack.set_str(lt::settings_pack::peer_fingerprint, s.peerFingerprint);

        // Alert mask – always include these categories
        pack.set_int(lt::settings_pack::alert_mask,
                     lt::alert_category::status |
                         lt::alert_category::error |
                         lt::alert_category::storage |
                         lt::alert_category::peer |
                         lt::alert_category::tracker |
                         lt::alert_category::stats);

        // Seeding limits are per-torrent in libtorrent; share_ratio_limit / seed_time_limit
        // are applied via session settings:
        pack.set_int(lt::settings_pack::share_ratio_limit,
                     s.seedingRatioLimit > 0 ? static_cast<int>(s.seedingRatioLimit * 100) : 0);
        pack.set_int(lt::settings_pack::seed_time_limit,
                     s.seedingTimeLimit > 0 ? s.seedingTimeLimit * 60 : 0); // minutes => seconds
    }

    TorrentSettings BuildTorrentSettingsFromPack(lt::settings_pack const &pack)
    {
        TorrentSettings s;

        s.listenInterfaces = pack.get_str(lt::settings_pack::listen_interfaces);
        s.connectionsLimit = pack.get_int(lt::settings_pack::connections_limit);
        s.enableIncomingTcp = pack.get_bool(lt::settings_pack::enable_incoming_tcp);
        s.enableOutgoingTcp = pack.get_bool(lt::settings_pack::enable_outgoing_tcp);
        s.enableIncomingUtp = pack.get_bool(lt::settings_pack::enable_incoming_utp);
        s.enableOutgoingUtp = pack.get_bool(lt::settings_pack::enable_outgoing_utp);
        s.allowMultipleConnectionsPerIp = pack.get_bool(lt::settings_pack::allow_multiple_connections_per_ip);
        s.anonymousMode = pack.get_bool(lt::settings_pack::anonymous_mode);

        s.enableDht = pack.get_bool(lt::settings_pack::enable_dht);
        s.enableLsd = pack.get_bool(lt::settings_pack::enable_lsd);
        s.enableUpnp = pack.get_bool(lt::settings_pack::enable_upnp);
        s.enableNatpmp = pack.get_bool(lt::settings_pack::enable_natpmp);

        s.announceToAllTiers = pack.get_bool(lt::settings_pack::announce_to_all_tiers);
        s.announceToAllTrackers = pack.get_bool(lt::settings_pack::announce_to_all_trackers);

        s.activeDownloads = pack.get_int(lt::settings_pack::active_downloads);
        s.activeSeeds = pack.get_int(lt::settings_pack::active_seeds);
        s.activeLimit = pack.get_int(lt::settings_pack::active_limit);

        s.downloadRateLimit = pack.get_int(lt::settings_pack::download_rate_limit);
        s.uploadRateLimit = pack.get_int(lt::settings_pack::upload_rate_limit);

        s.peerTimeout = pack.get_int(lt::settings_pack::peer_timeout);
        s.handshakeTimeout = pack.get_int(lt::settings_pack::handshake_timeout);
        s.closeRedundantConnections = pack.get_bool(lt::settings_pack::close_redundant_connections);
        s.maxPeerListSize = pack.get_int(lt::settings_pack::max_peerlist_size);

        s.aioThreads = pack.get_int(lt::settings_pack::aio_threads);
        s.checkingMemUsage = pack.get_int(lt::settings_pack::checking_mem_usage);

        s.userAgent = pack.get_str(lt::settings_pack::user_agent);
        s.peerFingerprint = pack.get_str(lt::settings_pack::peer_fingerprint);

        // Encryption
        int encPol = pack.get_int(lt::settings_pack::out_enc_policy);
        switch (encPol)
        {
        case 0:
            s.encryptionPolicy = EncryptionPolicy::Forced;
            break;
        case 2:
            s.encryptionPolicy = EncryptionPolicy::Disabled;
            break;
        default:
            s.encryptionPolicy = EncryptionPolicy::Enabled;
            break;
        }
        s.preferRc4 = pack.get_bool(lt::settings_pack::prefer_rc4);

        // Proxy
        s.proxyType = static_cast<ProxyType>(pack.get_int(lt::settings_pack::proxy_type));
        s.proxyHostname = pack.get_str(lt::settings_pack::proxy_hostname);
        s.proxyPort = pack.get_int(lt::settings_pack::proxy_port);
        s.proxyUsername = pack.get_str(lt::settings_pack::proxy_username);
        s.proxyPassword = pack.get_str(lt::settings_pack::proxy_password);
        s.proxyPeerConnections = pack.get_bool(lt::settings_pack::proxy_peer_connections);
        s.proxyTrackerConnections = pack.get_bool(lt::settings_pack::proxy_tracker_connections);

        // Seeding
        int ratioRaw = pack.get_int(lt::settings_pack::share_ratio_limit);
        s.seedingRatioLimit = ratioRaw > 0 ? ratioRaw / 100.0 : 0.0;
        int seedTimeSec = pack.get_int(lt::settings_pack::seed_time_limit);
        s.seedingTimeLimit = seedTimeSec > 0 ? seedTimeSec / 60 : 0;

        return s;
    }

} // namespace OpenNet::Core
