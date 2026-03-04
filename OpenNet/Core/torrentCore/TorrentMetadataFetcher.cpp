#include "pch.h"
#include "TorrentMetadataFetcher.h"

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_status.hpp>

#include <fstream>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace lt = libtorrent;
using namespace std::chrono_literals;

namespace OpenNet::Core::Torrent
{
    // Helper function to convert info_hash to hex string
    static std::string InfoHashToHex(lt::info_hash_t const& ih)
    {
        std::ostringstream oss;
        oss << ih;
        return oss.str();
    }

    TorrentMetadataFetcher::TorrentMetadataFetcher()
    {
    }

    TorrentMetadataFetcher::~TorrentMetadataFetcher()
    {
        Cancel();
        if (m_session)
        {
            try
            {
                if (m_handle.is_valid())
                {
                    m_session->remove_torrent(m_handle, lt::session::delete_files);
                }
                m_session.reset();
            }
            catch (...) {}
        }
    }

    bool TorrentMetadataFetcher::InitializeSession()
    {
        if (m_session) return true;

        try
        {
            lt::settings_pack pack;

            // Minimal configuration for metadata fetching
            pack.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:0,[::]:0");
            pack.set_bool(lt::settings_pack::enable_dht, true);
            pack.set_bool(lt::settings_pack::enable_lsd, true);
            pack.set_bool(lt::settings_pack::enable_upnp, false);  // Not needed for metadata
            pack.set_bool(lt::settings_pack::enable_natpmp, false);

            // Optimize for metadata fetching
            pack.set_int(lt::settings_pack::alert_mask, 
                lt::alert_category::status | 
                lt::alert_category::error);

            // Limit connections since we only need metadata
            pack.set_int(lt::settings_pack::connections_limit, 50);

            // Add DHT bootstrap nodes — without these, the DHT table is empty
            // and magnet links that rely on DHT can never find peers.
            pack.set_str(lt::settings_pack::dht_bootstrap_nodes,
                "router.bittorrent.com:6881,"
                "router.utorrent.com:6881,"
                "dht.transmissionbt.com:6881,"
                "dht.libtorrent.org:25401");

            m_session = std::make_unique<lt::session>(pack);
            return true;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("TorrentMetadataFetcher: Session init error: " + std::string(ex.what()) + "\n").c_str());
            return false;
        }
    }

    winrt::Windows::Foundation::IAsyncAction TorrentMetadataFetcher::FetchMetadataAsync(
        std::string const& torrentSource,
        std::function<void(TorrentMetadataInfo const&)> onSuccess,
        std::function<void(std::string const&)> onError,
        int timeoutSeconds)
    {
        // Reset state
        m_isFetching.store(true);
        m_cancelled.store(false);
        m_metadataReceived.store(false);
        m_metadataFailed.store(false);
        m_result = std::nullopt;
        m_errorMessage.clear();

        // Check if it's a torrent file (can parse directly)
        if (IsTorrentFile(torrentSource))
        {
            co_await winrt::resume_background();

            auto result = ParseTorrentFile(torrentSource);
            m_isFetching.store(false);

            if (result.has_value())
            {
                m_result = result;
                if (onSuccess) onSuccess(result.value());
            }
            else
            {
                if (onError) onError("Failed to parse torrent file");
            }
            co_return;
        }

        // It's a magnet link - need to download metadata
        if (!IsMagnetLink(torrentSource))
        {
            m_isFetching.store(false);
            if (onError) onError("Invalid torrent source");
            co_return;
        }

        co_await winrt::resume_background();

        // Initialize session
        if (!InitializeSession())
        {
            m_isFetching.store(false);
            if (onError) onError("Failed to initialize torrent session");
            co_return;
        }

        try
        {
            // Parse magnet URI
            lt::add_torrent_params atp = lt::parse_magnet_uri(torrentSource);

            // Use temp directory for metadata
            wchar_t tempPath[MAX_PATH];
            GetTempPathW(MAX_PATH, tempPath);
            std::wstring tempDir = std::wstring(tempPath) + L"OpenNet\\MetadataTemp";
            std::filesystem::create_directories(tempDir);

            atp.save_path = std::filesystem::path(tempDir).string();

            // Set flags for metadata-only mode
            // NOTE: Do NOT set upload_mode here — it prevents metadata download
            // for magnet links via the ut_metadata extension protocol (BEP 9).
            atp.flags &= ~lt::torrent_flags::auto_managed;
            atp.flags &= ~lt::torrent_flags::paused;

            // Add torrent
            m_handle = m_session->add_torrent(atp);

            if (m_progressCallback)
            {
                m_progressCallback("Connecting to peers...", 10);
            }

            // Wait for metadata with timeout
            auto startTime = std::chrono::steady_clock::now();
            auto timeout = std::chrono::seconds(timeoutSeconds);

            while (!m_cancelled.load() && !m_metadataReceived.load() && !m_metadataFailed.load())
            {
                auto elapsed = std::chrono::steady_clock::now() - startTime;
                if (elapsed >= timeout)
                {
                    // Clean up before returning
                    if (m_handle.is_valid())
                    {
                        m_session->remove_torrent(m_handle, lt::session::delete_files);
                        m_handle = lt::torrent_handle{};
                    }
                    m_isFetching.store(false);
                    if (onError) onError("Metadata download timeout");
                    co_return;
                }

                // Process alerts
                ProcessAlerts();

                // Check if metadata is available
                if (m_handle.is_valid())
                {
                    auto status = m_handle.status();

                    if (status.has_metadata)
                    {
                        m_metadataReceived.store(true);

                        if (m_progressCallback)
                        {
                            m_progressCallback("Metadata received!", 100);
                        }

                        m_result = ExtractMetadata(m_handle);
                        break;
                    }

                    // Update progress
                    if (m_progressCallback)
                    {
                        auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                        auto timeoutSec = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
                        int progressPercent = (timeoutSec > 0)
                            ? 10 + static_cast<int>((elapsedSec * 80) / timeoutSec)
                            : 10;
                        progressPercent = std::min(progressPercent, 90);

                        std::string statusMsg = "Downloading metadata... (" + 
                            std::to_string(status.num_peers) + " peers)";
                        m_progressCallback(statusMsg, progressPercent);
                    }
                }

                // Wait before next check
                co_await winrt::resume_after(500ms);
            }

            if (m_cancelled.load())
            {
                if (m_handle.is_valid())
                {
                    m_session->remove_torrent(m_handle, lt::session::delete_files);
                    m_handle = lt::torrent_handle{};
                }
                m_isFetching.store(false);
                if (onError) onError("Operation cancelled");
                co_return;
            }

            if (m_metadataFailed.load())
            {
                if (m_handle.is_valid())
                {
                    m_session->remove_torrent(m_handle, lt::session::delete_files);
                    m_handle = lt::torrent_handle{};
                }
                m_isFetching.store(false);
                if (onError) onError(m_errorMessage.empty() ? "Metadata download failed" : m_errorMessage);
                co_return;
            }

            // Clean up
            if (m_handle.is_valid())
            {
                m_session->remove_torrent(m_handle, lt::session::delete_files);
                m_handle = lt::torrent_handle{};
            }

            m_isFetching.store(false);

            if (m_result.has_value() && onSuccess)
            {
                onSuccess(m_result.value());
            }
            else if (!m_result.has_value() && onError)
            {
                onError("Failed to extract metadata");
            }
        }
        catch (std::exception const& ex)
        {
            m_isFetching.store(false);
            if (onError) onError(ex.what());
        }
    }

    std::optional<TorrentMetadataInfo> TorrentMetadataFetcher::GetResult() const
    {
        return m_result;
    }

    void TorrentMetadataFetcher::ProcessAlerts()
    {
        if (!m_session) return;

        std::vector<lt::alert*> alerts;
        m_session->pop_alerts(&alerts);

        for (lt::alert* a : alerts)
        {
            if (auto* mra = lt::alert_cast<lt::metadata_received_alert>(a))
            {
                OutputDebugStringA("TorrentMetadataFetcher: Metadata received alert\n");
                m_metadataReceived.store(true);
            }
            else if (auto* mfa = lt::alert_cast<lt::metadata_failed_alert>(a))
            {
                OutputDebugStringA(("TorrentMetadataFetcher: Metadata failed: " + mfa->message() + "\n").c_str());
                m_errorMessage = mfa->message();
                m_metadataFailed.store(true);
            }
            else if (auto* tea = lt::alert_cast<lt::torrent_error_alert>(a))
            {
                OutputDebugStringA(("TorrentMetadataFetcher: Torrent error: " + tea->message() + "\n").c_str());
                m_errorMessage = tea->message();
            }
        }
    }

    TorrentMetadataInfo TorrentMetadataFetcher::ExtractMetadata(lt::torrent_handle const& handle)
    {
        TorrentMetadataInfo info;

        if (!handle.is_valid())
            return info;

        try
        {
            auto status = handle.status();
            auto ti = handle.torrent_file();

            if (!ti)
                return info;

            // Basic info
            info.name = ti->name();
            info.totalSize = ti->total_size();
            info.pieceLength = static_cast<int>(ti->piece_length());
            info.numPieces = ti->num_pieces();
            info.isPrivate = ti->priv();
            info.comment = ti->comment();
            info.creator = ti->creator();
            info.creationDate = static_cast<int64_t>(ti->creation_date());

            // Info hash
            info.infoHash = InfoHashToHex(ti->info_hashes());

            // Files
            auto const& fs = ti->files();
            for (lt::file_index_t i{ 0 }; i < fs.end_file(); ++i)
            {
                TorrentFileInfo fileInfo;
                fileInfo.path = fs.file_path(i);
                fileInfo.size = fs.file_size(i);
                fileInfo.fileIndex = static_cast<int>(i);
                fileInfo.priority = 4; // Normal
                fileInfo.selected = true;
                info.files.push_back(fileInfo);
            }

            // Trackers
            auto trackers = handle.trackers();
            for (auto const& tracker : trackers)
            {
                info.trackers.push_back(tracker.url);
            }

            // Web seeds
            auto seeds = handle.url_seeds();
            for (auto const& seed : seeds)
            {
                info.webSeeds.push_back(seed);
            }
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("TorrentMetadataFetcher: ExtractMetadata error: " + std::string(ex.what()) + "\n").c_str());
        }

        return info;
    }

    void TorrentMetadataFetcher::SetProgressCallback(MetadataProgressCallback callback)
    {
        std::lock_guard lk(m_mutex);
        m_progressCallback = std::move(callback);
    }

    void TorrentMetadataFetcher::Cancel()
    {
        m_cancelled.store(true);
    }

    std::optional<TorrentMetadataInfo> TorrentMetadataFetcher::ParseTorrentFile(std::string const& filePath)
    {
        try
        {
            lt::torrent_info ti(filePath);
            TorrentMetadataInfo info;

            // Basic info
            info.name = ti.name();
            info.totalSize = ti.total_size();
            info.pieceLength = static_cast<int>(ti.piece_length());
            info.numPieces = ti.num_pieces();
            info.isPrivate = ti.priv();
            info.comment = ti.comment();
            info.creator = ti.creator();
            info.creationDate = static_cast<int64_t>(ti.creation_date());

            // Info hash
            // Info hash
            info.infoHash = InfoHashToHex(ti.info_hashes());

            // Files
            auto const& fs = ti.files();
            for (lt::file_index_t i{ 0 }; i < fs.end_file(); ++i)
            {
                TorrentFileInfo fileInfo;
                fileInfo.path = fs.file_path(i);
                fileInfo.size = fs.file_size(i);
                fileInfo.fileIndex = static_cast<int>(i);
                fileInfo.priority = 4;
                fileInfo.selected = true;
                info.files.push_back(fileInfo);
            }

            // Trackers
            auto const& trackerTiers = ti.trackers();
            for (auto const& tracker : trackerTiers)
            {
                info.trackers.push_back(tracker.url);
            }

            // Web seeds
            auto const& seeds = ti.web_seeds();
            for (auto const& seed : seeds)
            {
                info.webSeeds.push_back(seed.url);
            }

            return info;
        }
        catch (std::exception const& ex)
        {
            OutputDebugStringA(("TorrentMetadataFetcher: ParseTorrentFile error: " + std::string(ex.what()) + "\n").c_str());
            return std::nullopt;
        }
    }

    bool TorrentMetadataFetcher::IsValidTorrentSource(std::string const& source)
    {
        return IsMagnetLink(source) || IsTorrentFile(source);
    }

    bool TorrentMetadataFetcher::IsMagnetLink(std::string const& source)
    {
        // Check if starts with "magnet:?"
        if (source.length() < 8)
            return false;

        std::string prefix = source.substr(0, 8);
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
        return prefix == "magnet:?";
    }

    bool TorrentMetadataFetcher::IsTorrentFile(std::string const& source)
    {
        // Check if file exists and has .torrent extension
        if (source.length() < 8)
            return false;

        // Check for .torrent extension
        std::string lowerSource = source;
        std::transform(lowerSource.begin(), lowerSource.end(), lowerSource.begin(), ::tolower);

        if (lowerSource.length() >= 8 && lowerSource.substr(lowerSource.length() - 8) == ".torrent")
        {
            return std::filesystem::exists(source);
        }

        return false;
    }

} // namespace OpenNet::Core::Torrent
