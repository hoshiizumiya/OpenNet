export module OpenNet.Core.Content.ContentCatalogService;

import std;
export import OpenNet.Core.Content.ContentCatalog;
import OpenNet.Core.Content.SqliteContentCatalog;

export namespace OpenNet::Core::Content
{
    class ContentCatalogService
    {
    public:
        static ContentCatalogService& Instance();

        bool Initialize();
        void Shutdown();

        void EnqueueFile(
            std::filesystem::path path,
            ContentSourceReference source,
            std::vector<ContentIdentity> knownIdentities = {},
            std::vector<ResourceKey> resourceKeys = {});

        void RemoveLocation(std::filesystem::path const& path);

        [[nodiscard]] std::vector<ContentRecord> SnapshotAvailable() const;
        [[nodiscard]] std::vector<ContentRecord> SnapshotAll() const;
        [[nodiscard]] std::optional<ContentRecord> FindByIdentity(
            ContentIdentity const& identity) const;
        [[nodiscard]] std::optional<ContentRecord> FindByLocation(
            std::filesystem::path const& path) const;

        // Ensures the deterministic OpenNet.Content.v1 canonical piece layer
        // exists for one available content object. Existing v2 metadata may
        // provide only a file root, so the first canonical seed can require one
        // local scan to derive 1 MiB piece roots.
        [[nodiscard]] std::optional<ContentRecord> EnsureCanonicalPieceLayer(
            ContentIdentity const& identity);

        // Called after a durable catalog mutation. The callback must be cheap;
        // network synchronization should debounce/coalesce changes itself.
        void SetChangedCallback(std::function<void()> callback);

    private:
        struct PendingJob
        {
            std::filesystem::path path;
            ContentSourceReference source;
            std::vector<ContentIdentity> knownIdentities;
            std::vector<ResourceKey> resourceKeys;
        };

        ContentCatalogService() = default;
        ~ContentCatalogService();

        ContentCatalogService(ContentCatalogService const&) = delete;
        ContentCatalogService& operator=(ContentCatalogService const&) = delete;

        void WorkerLoop();
        void CatalogFile(PendingJob const& job);
        void ValidateLocations();
        void NotifyChanged();
        static ContentKey CreateContentKey();
        static ContentFileFingerprint GetFingerprint(std::filesystem::path const& path);
        static std::int64_t UnixNow();

        mutable std::mutex m_mutex;
        SqliteContentCatalog m_catalog;
        std::deque<PendingJob> m_jobs;
        std::condition_variable m_condition;
        std::thread m_worker;
        std::function<void()> m_changedCallback;
        bool m_initialized{};
        bool m_stopping{};
    };
}
