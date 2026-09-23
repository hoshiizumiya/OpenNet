module;

#include <Windows.h>

module OpenNet.Core.Content.ContentCatalogService;

import OpenNet.Core.Content.ContentHasher;
import std;

namespace OpenNet::Core::Content
{
    ContentCatalogService& ContentCatalogService::Instance()
    {
        static ContentCatalogService instance;
        return instance;
    }

    ContentCatalogService::~ContentCatalogService()
    {
        Shutdown();
    }

    bool ContentCatalogService::Initialize()
    {
        std::lock_guard lock(m_mutex);
        if (m_initialized) return true;
        if (!m_catalog.Initialize()) return false;

        m_stopping = false;
        m_initialized = true;
        ValidateLocations();
        m_worker = std::thread([this] { WorkerLoop(); });
        return true;
    }

    void ContentCatalogService::Shutdown()
    {
        {
            std::lock_guard lock(m_mutex);
            if (!m_initialized) return;
            m_stopping = true;
        }
        m_condition.notify_all();
        if (m_worker.joinable()) m_worker.join();

        std::lock_guard lock(m_mutex);
        m_jobs.clear();
        m_changedCallback = {};
        m_catalog.Close();
        m_initialized = false;
    }

    std::int64_t ContentCatalogService::UnixNow()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    ContentKey ContentCatalogService::CreateContentKey()
    {
        GUID guid{};
        if (FAILED(CoCreateGuid(&guid)))
            throw std::runtime_error("Unable to allocate a content key.");

        ContentKey key;
        static_assert(sizeof(guid) == key.value.size());
        std::memcpy(key.value.data(), &guid, key.value.size());
        return key;
    }

    ContentFileFingerprint ContentCatalogService::GetFingerprint(
        std::filesystem::path const& path)
    {
        ContentFileFingerprint fingerprint;
        fingerprint.fileSize = std::filesystem::file_size(path);
        fingerprint.lastWriteTicks =
            std::filesystem::last_write_time(path).time_since_epoch().count();
        return fingerprint;
    }

    void ContentCatalogService::ValidateLocations()
    {
        // This is deliberately metadata-only. Startup must not reread every
        // historical download merely to prove the catalog still exists.
        for (auto const& record : m_catalog.SnapshotAll())
        {
            for (auto const& location : record.locations)
            {
                ContentAvailability state = ContentAvailability::Available;
                std::error_code error;
                if (!std::filesystem::is_regular_file(location.localPath, error) || error)
                {
                    state = ContentAvailability::Missing;
                }
                else
                {
                    auto const size = std::filesystem::file_size(location.localPath, error);
                    if (error || size != location.fingerprint.fileSize)
                    {
                        state = ContentAvailability::Modified;
                    }
                    else
                    {
                        auto const writeTime =
                            std::filesystem::last_write_time(location.localPath, error);
                        if (error
                            || writeTime.time_since_epoch().count()
                                != location.fingerprint.lastWriteTicks)
                            state = ContentAvailability::Modified;
                    }
                }

                if (state != location.availability)
                    m_catalog.SetLocationAvailability(location.localPath, state);
            }
        }
    }

    void ContentCatalogService::EnqueueFile(
        std::filesystem::path path,
        ContentSourceReference source,
        std::vector<ContentIdentity> knownIdentities,
        std::vector<ResourceKey> resourceKeys)
    {
        if (path.empty()) return;
        {
            std::lock_guard lock(m_mutex);
            if (!m_initialized)
            {
                if (!m_catalog.Initialize()) return;
                m_stopping = false;
                m_initialized = true;
                ValidateLocations();
                m_worker = std::thread([this] { WorkerLoop(); });
            }

            // Coalesce repeated completion/status events for the same path.
            auto const duplicate = std::ranges::find_if(
                m_jobs,
                [&](PendingJob const& job) { return job.path == path; });
            if (duplicate != m_jobs.end())
            {
                if (!source.sourceId.empty()) duplicate->source = std::move(source);
                for (auto& identity : knownIdentities)
                {
                    if (std::ranges::find(duplicate->knownIdentities, identity)
                        == duplicate->knownIdentities.end())
                        duplicate->knownIdentities.push_back(std::move(identity));
                }
                for (auto& resourceKey : resourceKeys)
                {
                    if (std::ranges::find(
                        duplicate->resourceKeys, resourceKey)
                        == duplicate->resourceKeys.end())
                        duplicate->resourceKeys.push_back(
                            std::move(resourceKey));
                }
                return;
            }

            m_jobs.push_back({
                std::move(path),
                std::move(source),
                std::move(knownIdentities),
                std::move(resourceKeys)
            });
        }
        m_condition.notify_one();
    }

    void ContentCatalogService::WorkerLoop()
    {
        for (;;)
        {
            PendingJob job;
            {
                std::unique_lock lock(m_mutex);
                m_condition.wait(lock, [this]
                {
                    return m_stopping || !m_jobs.empty();
                });
                if (m_stopping && m_jobs.empty()) return;
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
            }

            try
            {
                CatalogFile(job);
            }
            catch (std::exception const& exception)
            {
                OutputDebugStringA((
                    "ContentCatalogService: cataloging failed: "
                    + std::string(exception.what()) + "\n").c_str());
            }
            catch (...)
            {
                OutputDebugStringA("ContentCatalogService: cataloging failed\n");
            }
        }
    }

    void ContentCatalogService::CatalogFile(PendingJob const& job)
    {
        if (!std::filesystem::is_regular_file(job.path)) return;

        auto identities = job.knownIdentities;
        std::erase_if(identities, [](ContentIdentity const& identity)
        {
            return !identity.IsWellFormed();
        });

        bool const hasAuthoritativeTreeRoot = std::ranges::any_of(
            identities,
            [](ContentIdentity const& identity)
            {
                return identity.algorithm
                    == ContentIdentityAlgorithm::Bep52FileRootSha256;
            });

        std::uint64_t size = std::filesystem::file_size(job.path);
        std::optional<CanonicalPieceLayer> hashedPieceLayer;
        if (!hasAuthoritativeTreeRoot)
        {
            auto hashed = ContentHasher::HashFile(job.path);
            size = hashed.size;
            hashedPieceLayer = std::move(hashed.canonicalPieceLayer);
            for (auto& identity : hashed.identities)
            {
                if (std::ranges::find(identities, identity) == identities.end())
                    identities.push_back(std::move(identity));
            }
        }

        if (identities.empty()) return;

        std::optional<ContentRecord> existing;
        for (auto const& identity : identities)
        {
            if (auto candidate = m_catalog.FindByIdentity(identity))
            {
                if (existing && existing->key != candidate->key)
                    throw std::runtime_error(
                        "Content aliases resolve to different local content records.");
                existing = std::move(candidate);
            }
        }

        ContentRecord record = existing.value_or(ContentRecord{});
        if (!existing) record.key = CreateContentKey();
        if (record.size != 0 && record.size != size)
            throw std::runtime_error(
                "A content identity resolved to a different file size.");
        record.size = size;
        if (hashedPieceLayer)
            record.canonicalPieceLayer = std::move(hashedPieceLayer);

        for (auto const& identity : identities)
        {
            if (std::ranges::find(record.identities, identity)
                == record.identities.end())
                record.identities.push_back(identity);
        }

        for (auto const& resourceKey : job.resourceKeys)
        {
            if (std::ranges::find(
                record.resourceKeys, resourceKey)
                == record.resourceKeys.end())
                record.resourceKeys.push_back(resourceKey);
        }

        ContentLocation location;
        location.localPath = job.path;
        location.availability = ContentAvailability::Available;
        location.fingerprint = GetFingerprint(job.path);
        location.verifiedAt = UnixNow();

        auto locationIt = std::ranges::find_if(
            record.locations,
            [&](ContentLocation const& value)
            {
                return value.localPath == location.localPath;
            });
        if (locationIt == record.locations.end())
            record.locations.push_back(std::move(location));
        else
            *locationIt = std::move(location);

        if (!job.source.sourceId.empty()
            && std::ranges::find(record.sources, job.source)
                == record.sources.end())
            record.sources.push_back(job.source);

        m_catalog.Upsert(std::move(record));
        NotifyChanged();
    }

    void ContentCatalogService::RemoveLocation(std::filesystem::path const& path)
    {
        m_catalog.RemoveLocation(path);
        NotifyChanged();
    }

    std::vector<ContentRecord> ContentCatalogService::SnapshotAvailable() const
    {
        return m_catalog.SnapshotAvailable();
    }

    std::vector<ContentRecord> ContentCatalogService::SnapshotAll() const
    {
        return m_catalog.SnapshotAll();
    }

    std::optional<ContentRecord> ContentCatalogService::FindByIdentity(
        ContentIdentity const& identity) const
    {
        return m_catalog.FindByIdentity(identity);
    }

    std::optional<ContentRecord> ContentCatalogService::FindByLocation(
        std::filesystem::path const& path) const
    {
        return m_catalog.FindByLocation(path);
    }

    std::optional<ContentRecord> ContentCatalogService::EnsureCanonicalPieceLayer(
        ContentIdentity const& identity)
    {
        auto record = m_catalog.FindByIdentity(identity);
        if (!record || !record->IsAvailable())
            return std::nullopt;
        if (record->canonicalPieceLayer
            && record->canonicalPieceLayer->IsValidFor(record->size))
            return record;

        auto location = std::ranges::find_if(
            record->locations,
            [](ContentLocation const& value)
            {
                return value.availability == ContentAvailability::Available;
            });
        if (location == record->locations.end())
            return std::nullopt;

        try
        {
            auto hashed = ContentHasher::HashFile(location->localPath);
            if (hashed.size != record->size || !hashed.canonicalPieceLayer)
            {
                m_catalog.SetLocationAvailability(
                    location->localPath,
                    ContentAvailability::Modified);
                NotifyChanged();
                return std::nullopt;
            }

            auto expectedRoot = std::ranges::find_if(
                record->identities,
                [](ContentIdentity const& value)
                {
                    return value.algorithm
                        == ContentIdentityAlgorithm::Bep52FileRootSha256;
                });
            auto actualRoot = std::ranges::find_if(
                hashed.identities,
                [](ContentIdentity const& value)
                {
                    return value.algorithm
                        == ContentIdentityAlgorithm::Bep52FileRootSha256;
                });

            if (expectedRoot != record->identities.end()
                && (actualRoot == hashed.identities.end()
                    || *expectedRoot != *actualRoot))
            {
                m_catalog.SetLocationAvailability(
                    location->localPath,
                    ContentAvailability::Modified);
                NotifyChanged();
                return std::nullopt;
            }

            for (auto& hashIdentity : hashed.identities)
            {
                if (std::ranges::find(
                    record->identities, hashIdentity) == record->identities.end())
                    record->identities.push_back(std::move(hashIdentity));
            }
            record->canonicalPieceLayer =
                std::move(hashed.canonicalPieceLayer);
            location->fingerprint = GetFingerprint(location->localPath);
            location->verifiedAt = UnixNow();
            m_catalog.Upsert(*record);
            NotifyChanged();
            return record;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    void ContentCatalogService::SetChangedCallback(std::function<void()> callback)
    {
        std::lock_guard lock(m_mutex);
        m_changedCallback = std::move(callback);
    }

    void ContentCatalogService::NotifyChanged()
    {
        std::function<void()> callback;
        {
            std::lock_guard lock(m_mutex);
            callback = m_changedCallback;
        }
        if (callback) callback();
    }
}
