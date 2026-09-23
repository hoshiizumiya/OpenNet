module;

#include <Windows.h>
#include <winrt/base.h>

module OpenNet.Core.Content.ContentDirectorySyncService;

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Content.CanonicalV2Swarm;
import OpenNet.Core.Content.ContentCatalogService;
import OpenNet.Core.Content.ContentDirectoryClient;
import OpenNet.Core.P2PManager;
import OpenNet.Core.TorrentSettings;
import std;

namespace OpenNet::Core::Content
{
    namespace
    {
        constexpr auto SyncCategory = "content_directory";
        constexpr auto NodeIdKey = "node_id";
        constexpr auto GenerationKey = "generation";
        constexpr auto LeaseIdKey = "lease_id";
        constexpr auto PendingGenerationKey = "pending_generation";
        constexpr auto PendingRegistrationIdKey = "pending_registration_id";

        std::string GuidString()
        {
            GUID guid{};
            if (FAILED(CoCreateGuid(&guid)))
                throw std::runtime_error("Unable to generate content directory node ID.");
            wchar_t buffer[39]{};
            if (StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer))) <= 0)
                throw std::runtime_error("Unable to format content directory node ID.");
            std::string value = winrt::to_string(buffer);
            if (value.size() >= 2 && value.front() == '{' && value.back() == '}')
                value = value.substr(1, value.size() - 2);
            return value;
        }
    }

    ContentDirectorySyncService& ContentDirectorySyncService::Instance()
    {
        static ContentDirectorySyncService instance;
        return instance;
    }

    ContentDirectorySyncService::~ContentDirectorySyncService()
    {
        Stop();
    }

    std::string ContentDirectorySyncService::GetOrCreateNodeId()
    {
        auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
        database.Initialize();
        if (auto existing = database.GetString(SyncCategory, NodeIdKey);
            existing && !existing->empty())
            return *existing;

        auto id = GuidString();
        database.SetString(SyncCategory, NodeIdKey, id);
        return id;
    }

    void ContentDirectorySyncService::Start()
    {
        std::lock_guard lock(m_mutex);
        if (m_started) return;

        m_stopping = false;
        ++m_revision;
        m_syncedRevision = 0;
        m_announcedResourceRevision = 0;
        m_started = true;
        ::OpenNet::Core::Content::ContentCatalogService::Instance()
            .SetChangedCallback([this] { MarkDirty(); });
        m_worker = std::thread([this] { WorkerLoop(); });
    }

    void ContentDirectorySyncService::Stop()
    {
        {
            std::lock_guard lock(m_mutex);
            if (!m_started) return;
            m_stopping = true;
        }

        ::OpenNet::Core::Content::ContentCatalogService::Instance()
            .SetChangedCallback({});
        m_condition.notify_all();
        if (m_worker.joinable()) m_worker.join();

        std::lock_guard lock(m_mutex);
        m_started = false;
        m_stopping = false;
    }

    void ContentDirectorySyncService::MarkDirty()
    {
        {
            std::lock_guard lock(m_mutex);
            if (!m_started) return;
            ++m_revision;
        }
        m_condition.notify_all();
    }

    void ContentDirectorySyncService::WorkerLoop()
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        std::unordered_map<std::string, std::chrono::steady_clock::time_point>
            activeSeedSessions;

        try
        {
            auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
            database.Initialize();

            auto const nodeId = GetOrCreateNodeId();
            std::uint64_t generation = static_cast<std::uint64_t>(
                (std::max<std::int64_t>)(
                    0,
                    database.GetInt(SyncCategory, GenerationKey, 0)));
            std::optional<std::string> leaseId =
                database.GetString(SyncCategory, LeaseIdKey);
            std::uint64_t pendingGeneration = static_cast<std::uint64_t>(
                (std::max<std::int64_t>)(
                    0,
                    database.GetInt(SyncCategory, PendingGenerationKey, 0)));
            std::optional<std::string> pendingRegistrationId =
                database.GetString(SyncCategory, PendingRegistrationIdKey);
            ContentDirectoryClient client;
            auto lastHeartbeat =
                std::chrono::steady_clock::now() - std::chrono::minutes(5);

            for (;;)
            {
                {
                    std::lock_guard lock(m_mutex);
                    if (m_stopping) break;
                }

                bool directoryReachable = false;
                std::uint64_t attemptRevision{};
                bool dirty{};
                {
                    std::lock_guard lock(m_mutex);
                    attemptRevision = m_revision;
                    dirty = m_syncedRevision != m_revision
                        || !leaseId
                        || leaseId->empty();
                }

                if (::OpenNet::Core::P2PManager::Instance()
                    .IsTorrentCoreInitialized())
                {
                    auto listen =
                        ::OpenNet::Core::P2PManager::Instance().GetListenStatus();
                    auto torrentSettings =
                        ::OpenNet::Core::TorrentSettingsManager::Instance().Get();

                    std::vector<ContentDirectoryEndpoint> endpoints;
                    auto addEndpoint = [&](ContentPeerTransport transport,
                                           ContentAddressFamily family,
                                           int port)
                    {
                        if (port <= 0 || port > 65535) return;
                        endpoints.push_back({
                            transport,
                            family,
                            static_cast<std::uint16_t>(port)
                        });
                    };

                    if (torrentSettings.enableIncomingTcp)
                    {
                        addEndpoint(ContentPeerTransport::Tcp,
                            ContentAddressFamily::Ipv4, listen.ipv4TcpPort);
                        addEndpoint(ContentPeerTransport::Tcp,
                            ContentAddressFamily::Ipv6, listen.ipv6TcpPort);
                    }
                    if (torrentSettings.enableIncomingUtp)
                    {
                        addEndpoint(ContentPeerTransport::Utp,
                            ContentAddressFamily::Ipv4, listen.ipv4UtpPort);
                        addEndpoint(ContentPeerTransport::Utp,
                            ContentAddressFamily::Ipv6, listen.ipv6UtpPort);
                    }

                    if (!endpoints.empty())
                    {
                        auto const now = std::chrono::steady_clock::now();
                        if (dirty)
                        {
                            auto records =
                                ::OpenNet::Core::Content::ContentCatalogService::Instance()
                                    .SnapshotAvailable();

                            std::uint64_t const targetGeneration = generation + 1;
                            if (pendingGeneration != targetGeneration
                                || !pendingRegistrationId
                                || pendingRegistrationId->empty())
                            {
                                pendingGeneration = targetGeneration;
                                pendingRegistrationId = GuidString();
                                database.SetInt(
                                    SyncCategory,
                                    PendingGenerationKey,
                                    static_cast<std::int64_t>(pendingGeneration));
                                database.SetString(
                                    SyncCategory,
                                    PendingRegistrationIdKey,
                                    *pendingRegistrationId);
                            }

                            auto result = client.Register(
                                nodeId,
                                pendingGeneration,
                                *pendingRegistrationId,
                                leaseId,
                                records,
                                endpoints);
                            if (result)
                            {
                                generation = result->acceptedGeneration;
                                leaseId = result->leaseId;
                                database.SetInt(
                                    SyncCategory,
                                    GenerationKey,
                                    static_cast<std::int64_t>(generation));
                                database.SetString(
                                    SyncCategory,
                                    LeaseIdKey,
                                    *leaseId);
                                database.Delete(
                                    SyncCategory, PendingGenerationKey);
                                database.Delete(
                                    SyncCategory, PendingRegistrationIdKey);
                                pendingGeneration = 0;
                                pendingRegistrationId.reset();
                                {
                                    std::lock_guard lock(m_mutex);
                                    m_syncedRevision = attemptRevision;
                                }
                                lastHeartbeat = now;
                                directoryReachable = true;
                            }
                        }
                        else if (leaseId
                            && now - lastHeartbeat >= std::chrono::seconds(90))
                        {
                            if (client.Heartbeat(nodeId, *leaseId))
                            {
                                lastHeartbeat = now;
                                directoryReachable = true;
                            }
                            else
                            {
                                leaseId.reset();
                                database.Delete(SyncCategory, LeaseIdKey);
                                std::lock_guard lock(m_mutex);
                                ++m_revision;
                            }
                        }
                        else
                        {
                            directoryReachable = leaseId.has_value();
                        }

                        if (leaseId)
                        {
                            std::uint64_t resourceAttemptRevision{};
                            bool announceResources{};
                            {
                                std::lock_guard lock(m_mutex);
                                resourceAttemptRevision = m_revision;
                                announceResources =
                                    m_announcedResourceRevision
                                        != m_revision;
                            }

                            if (announceResources)
                            {
                                bool allAnnounced = true;
                                auto records =
                                    ::OpenNet::Core::Content::ContentCatalogService::Instance()
                                        .SnapshotAvailable();

                                for (auto const& record : records)
                                {
                                    auto identity = std::ranges::find_if(
                                        record.identities,
                                        [](ContentIdentity const& value)
                                        {
                                            return value.algorithm
                                                == ContentIdentityAlgorithm::Bep52FileRootSha256;
                                        });
                                    if (identity == record.identities.end())
                                        continue;

                                    for (auto const& resourceKey
                                        : record.resourceKeys)
                                    {
                                        if (!client.AnnounceResource(
                                            nodeId,
                                            *leaseId,
                                            resourceKey,
                                            *identity))
                                        {
                                            allAnnounced = false;
                                        }
                                    }
                                }

                                if (allAnnounced)
                                {
                                    std::lock_guard lock(m_mutex);
                                    m_announcedResourceRevision =
                                        resourceAttemptRevision;
                                }
                            }

                            auto wakeups = client.PollWakeups(
                                nodeId, *leaseId, 32);
                            if (wakeups)
                            {
                                directoryReachable = true;
                                for (auto const& wakeup : *wakeups)
                                {
                                    bool completed = false;
                                    std::string failure =
                                        "No matching available local content.";

                                    for (auto const& identity : wakeup.identities)
                                    {
                                        auto record =
                                            ::OpenNet::Core::Content::ContentCatalogService::Instance()
                                                .EnsureCanonicalPieceLayer(identity);
                                        if (!record
                                            || !record->IsAvailable()
                                            || !record->canonicalPieceLayer)
                                            continue;

                                        auto location = std::ranges::find_if(
                                            record->locations,
                                            [](ContentLocation const& value)
                                            {
                                                return value.availability
                                                    == ContentAvailability::Available;
                                            });
                                        if (location == record->locations.end())
                                            continue;

                                        try
                                        {
                                            auto canonical =
                                                CanonicalV2Swarm::Build(*record);
                                            auto const expectedInfoHash =
                                                CanonicalV2Swarm::InfoHashHex(
                                                    canonical.infoHashV2);
                                            std::string const sessionId =
                                                "seed:" + expectedInfoHash;

                                            auto opened =
                                                ::OpenNet::Core::P2PManager::Instance()
                                                    .OpenLongSeedSession(
                                                        sessionId,
                                                        canonical.metainfo,
                                                        location->localPath);
                                            if (!opened.succeeded
                                                || opened.infoHashV2
                                                    != expectedInfoHash)
                                            {
                                                failure = opened.error.empty()
                                                    ? "Unable to open canonical seed session."
                                                    : opened.error;
                                                continue;
                                            }

                                            if (client.CompleteWakeup(
                                                nodeId,
                                                *leaseId,
                                                wakeup.wakeRequestId,
                                                true,
                                                opened.infoHashV2,
                                                canonical.metainfo))
                                            {
                                                activeSeedSessions.insert_or_assign(
                                                    sessionId,
                                                    std::chrono::steady_clock::now());
                                                completed = true;
                                                break;
                                            }

                                            failure =
                                                "Server rejected wakeup completion.";
                                        }
                                        catch (std::exception const& exception)
                                        {
                                            failure = exception.what();
                                        }
                                    }

                                    if (!completed)
                                    {
                                        (void)client.CompleteWakeup(
                                            nodeId,
                                            *leaseId,
                                            wakeup.wakeRequestId,
                                            false,
                                            {},
                                            {},
                                            failure);
                                    }
                                }
                            }
                        }
                    }
                }

                auto const now = std::chrono::steady_clock::now();
                std::vector<std::string> expired;
                for (auto const& [sessionId, lastUsed] : activeSeedSessions)
                {
                    if (now - lastUsed >= std::chrono::minutes(5))
                        expired.push_back(sessionId);
                }
                for (auto const& sessionId : expired)
                {
                    ::OpenNet::Core::P2PManager::Instance()
                        .CloseLongSeedSession(sessionId);
                    activeSeedSessions.erase(sessionId);
                }

                std::unique_lock lock(m_mutex);
                if (m_stopping) break;
                auto const delay = directoryReachable
                    ? std::chrono::seconds(3)
                    : std::chrono::seconds(15);
                m_condition.wait_for(
                    lock,
                    delay,
                    [this]
                    {
                        return m_stopping
                            || m_syncedRevision != m_revision;
                    });
            }
        }
        catch (std::exception const& exception)
        {
            OutputDebugStringA((
                "ContentDirectorySyncService stopped after error: "
                + std::string(exception.what()) + "\n").c_str());
        }
        catch (...)
        {
            OutputDebugStringA(
                "ContentDirectorySyncService stopped after unknown error\n");
        }

        for (auto const& [sessionId, lastUsed] : activeSeedSessions)
        {
            (void)lastUsed;
            ::OpenNet::Core::P2PManager::Instance()
                .CloseLongSeedSession(sessionId);
        }

        winrt::uninit_apartment();
    }
}
