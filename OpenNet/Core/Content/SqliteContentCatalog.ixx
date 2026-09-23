module;

struct sqlite3;

export module OpenNet.Core.Content.SqliteContentCatalog;

import std;
import OpenNet.Core.Content.ContentCatalog;

export namespace OpenNet::Core::Content
{
    class SqliteContentCatalog final : public IContentCatalog
    {
    public:
        SqliteContentCatalog() = default;
        ~SqliteContentCatalog();

        SqliteContentCatalog(SqliteContentCatalog const&) = delete;
        SqliteContentCatalog& operator=(SqliteContentCatalog const&) = delete;

        bool Initialize();
        void Close();

        void Upsert(ContentRecord record) override;
        std::optional<ContentRecord> FindByKey(ContentKey const& key) const override;
        std::optional<ContentRecord> FindByIdentity(ContentIdentity const& identity) const override;
        std::optional<ContentRecord> FindByLocation(
            std::filesystem::path const& path) const override;
        std::vector<ContentRecord> SnapshotAvailable() const override;
        std::vector<ContentRecord> SnapshotAll() const override;
        void SetLocationAvailability(
            std::filesystem::path const& path,
            ContentAvailability availability) override;
        void RemoveLocation(std::filesystem::path const& path) override;

    private:
        bool EnsureInitialized();
        void CreateTables();
        std::optional<ContentRecord> LoadRecord(ContentKey const& key) const;
        static std::string PathToUtf8(std::filesystem::path const& path);
        static std::filesystem::path PathFromUtf8(char const* path);

        mutable std::recursive_mutex m_mutex;
        sqlite3* m_db{};
        bool m_initialized{};
    };
}
