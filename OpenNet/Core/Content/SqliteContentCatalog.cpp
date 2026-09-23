module;

#include <Windows.h>
#include <sqlite3.h>

module OpenNet.Core.Content.SqliteContentCatalog;

import OpenNet.Core.IO.FileSystem;
import std;

namespace OpenNet::Core::Content
{
    namespace
    {
        void BindKey(sqlite3_stmt* statement, int index, ContentKey const& key)
        {
            sqlite3_bind_blob(
                statement,
                index,
                key.value.data(),
                static_cast<int>(key.value.size()),
                SQLITE_TRANSIENT);
        }

        ContentKey ReadKey(sqlite3_stmt* statement, int column)
        {
            ContentKey key;
            auto const* bytes = static_cast<std::uint8_t const*>(
                sqlite3_column_blob(statement, column));
            int const size = sqlite3_column_bytes(statement, column);
            if (!bytes || size != static_cast<int>(key.value.size()))
                throw std::runtime_error("Invalid content key in content catalog.");
            std::copy_n(bytes, key.value.size(), key.value.begin());
            return key;
        }

        std::vector<std::uint8_t> ReadBlob(sqlite3_stmt* statement, int column)
        {
            auto const* bytes = static_cast<std::uint8_t const*>(
                sqlite3_column_blob(statement, column));
            int const size = sqlite3_column_bytes(statement, column);
            if (!bytes || size <= 0) return {};
            return { bytes, bytes + size };
        }

        std::int64_t UnixNow()
        {
            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }

        void ThrowSqlite(sqlite3* database, char const* operation)
        {
            throw std::runtime_error(
                std::string(operation) + ": " + sqlite3_errmsg(database));
        }
    }

    SqliteContentCatalog::~SqliteContentCatalog()
    {
        Close();
    }

    bool SqliteContentCatalog::Initialize()
    {
        std::lock_guard lock(m_mutex);
        if (m_initialized && m_db) return true;

        auto const databasePath =
            std::filesystem::path(winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW())
            / L"content_catalog.db";

        int const result = sqlite3_open16(databasePath.c_str(), &m_db);
        if (result != SQLITE_OK)
        {
            OutputDebugStringA("SqliteContentCatalog: failed to open content_catalog.db\n");
            Close();
            return false;
        }

        sqlite3_busy_timeout(m_db, 5000);
        sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
        CreateTables();
        m_initialized = true;
        return true;
    }

    void SqliteContentCatalog::Close()
    {
        std::lock_guard lock(m_mutex);
        if (m_db)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        m_initialized = false;
    }

    bool SqliteContentCatalog::EnsureInitialized()
    {
        return (m_initialized && m_db) || Initialize();
    }

    void SqliteContentCatalog::CreateTables()
    {
        char const* sql = R"(
            CREATE TABLE IF NOT EXISTS content (
                content_key BLOB PRIMARY KEY,
                file_size INTEGER NOT NULL,
                created_at INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS content_identity (
                content_key BLOB NOT NULL,
                algorithm INTEGER NOT NULL,
                digest BLOB NOT NULL,
                PRIMARY KEY (content_key, algorithm, digest),
                UNIQUE (algorithm, digest),
                FOREIGN KEY (content_key) REFERENCES content(content_key)
                    ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS content_location (
                local_path TEXT PRIMARY KEY,
                content_key BLOB NOT NULL,
                file_size INTEGER NOT NULL,
                last_write_ticks INTEGER NOT NULL,
                volume_serial INTEGER,
                file_id BLOB,
                availability INTEGER NOT NULL,
                verified_at INTEGER NOT NULL,
                FOREIGN KEY (content_key) REFERENCES content(content_key)
                    ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_content_location_key
                ON content_location(content_key);
            CREATE INDEX IF NOT EXISTS idx_content_location_availability
                ON content_location(availability);

            CREATE TABLE IF NOT EXISTS content_source (
                content_key BLOB NOT NULL,
                source_kind INTEGER NOT NULL,
                source_id TEXT NOT NULL,
                source_file_index INTEGER,
                UNIQUE (content_key, source_kind, source_id, source_file_index),
                FOREIGN KEY (content_key) REFERENCES content(content_key)
                    ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS content_piece_layer (
                content_key BLOB PRIMARY KEY,
                version INTEGER NOT NULL,
                piece_length INTEGER NOT NULL,
                piece_roots BLOB NOT NULL,
                FOREIGN KEY (content_key) REFERENCES content(content_key)
                    ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS content_resource_key (
                algorithm INTEGER NOT NULL,
                digest BLOB NOT NULL,
                content_key BLOB NOT NULL,
                observed_at INTEGER NOT NULL,
                PRIMARY KEY (algorithm, digest),
                FOREIGN KEY (content_key) REFERENCES content(content_key)
                    ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_content_resource_key_content
                ON content_resource_key(content_key);
        )";

        char* message{};
        if (sqlite3_exec(m_db, sql, nullptr, nullptr, &message) != SQLITE_OK)
        {
            std::string error = message ? message : "unknown SQLite error";
            sqlite3_free(message);
            throw std::runtime_error("Unable to create content catalog schema: " + error);
        }
    }

    std::string SqliteContentCatalog::PathToUtf8(std::filesystem::path const& path)
    {
        return winrt::to_string(winrt::hstring{ path.wstring() });
    }

    std::filesystem::path SqliteContentCatalog::PathFromUtf8(char const* path)
    {
        if (!path) return {};
        return std::filesystem::path{ winrt::to_hstring(path).c_str() };
    }

    void SqliteContentCatalog::Upsert(ContentRecord record)
    {
        std::lock_guard lock(m_mutex);
        if (!EnsureInitialized()) throw std::runtime_error("Content catalog is unavailable.");
        if (record.size > static_cast<std::uint64_t>((std::numeric_limits<sqlite3_int64>::max)()))
            throw std::overflow_error("Content size cannot be represented by SQLite.");

        for (auto const& identity : record.identities)
        {
            if (!identity.IsWellFormed())
                throw std::invalid_argument("Malformed content identity.");
        }

        if (sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK)
            ThrowSqlite(m_db, "BEGIN content catalog transaction");

        try
        {
            {
                sqlite3_stmt* statement{};
                char const* sql =
                    "INSERT INTO content(content_key, file_size, created_at) VALUES(?, ?, ?) "
                    "ON CONFLICT(content_key) DO UPDATE SET file_size=excluded.file_size;";
                if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
                    ThrowSqlite(m_db, "Prepare content upsert");
                BindKey(statement, 1, record.key);
                sqlite3_bind_int64(statement, 2, static_cast<sqlite3_int64>(record.size));
                sqlite3_bind_int64(statement, 3, UnixNow());
                int const result = sqlite3_step(statement);
                sqlite3_finalize(statement);
                if (result != SQLITE_DONE) ThrowSqlite(m_db, "Upsert content");
            }

            for (auto const& identity : record.identities)
            {
                sqlite3_stmt* statement{};
                char const* sql =
                    "INSERT OR IGNORE INTO content_identity(content_key, algorithm, digest) "
                    "VALUES(?, ?, ?);";
                if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
                    ThrowSqlite(m_db, "Prepare identity upsert");
                BindKey(statement, 1, record.key);
                sqlite3_bind_int(statement, 2, static_cast<int>(identity.algorithm));
                sqlite3_bind_blob(
                    statement, 3, identity.digest.data(),
                    static_cast<int>(identity.digest.size()), SQLITE_TRANSIENT);
                int const result = sqlite3_step(statement);
                sqlite3_finalize(statement);
                if (result != SQLITE_DONE) ThrowSqlite(m_db, "Upsert content identity");
            }

            for (auto const& location : record.locations)
            {
                sqlite3_stmt* statement{};
                char const* sql =
                    "INSERT INTO content_location("
                    "local_path, content_key, file_size, last_write_ticks, volume_serial, "
                    "file_id, availability, verified_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
                    "ON CONFLICT(local_path) DO UPDATE SET "
                    "content_key=excluded.content_key, file_size=excluded.file_size, "
                    "last_write_ticks=excluded.last_write_ticks, "
                    "volume_serial=excluded.volume_serial, file_id=excluded.file_id, "
                    "availability=excluded.availability, verified_at=excluded.verified_at;";
                if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
                    ThrowSqlite(m_db, "Prepare content location upsert");
                auto const path = PathToUtf8(location.localPath);
                sqlite3_bind_text(statement, 1, path.c_str(), -1, SQLITE_TRANSIENT);
                BindKey(statement, 2, record.key);
                sqlite3_bind_int64(statement, 3, static_cast<sqlite3_int64>(location.fingerprint.fileSize));
                sqlite3_bind_int64(statement, 4, location.fingerprint.lastWriteTicks);
                if (location.fingerprint.volumeSerial)
                    sqlite3_bind_int64(statement, 5, static_cast<sqlite3_int64>(*location.fingerprint.volumeSerial));
                else
                    sqlite3_bind_null(statement, 5);
                if (location.fingerprint.fileId)
                    sqlite3_bind_blob(statement, 6, location.fingerprint.fileId->data(),
                        static_cast<int>(location.fingerprint.fileId->size()), SQLITE_TRANSIENT);
                else
                    sqlite3_bind_null(statement, 6);
                sqlite3_bind_int(statement, 7, static_cast<int>(location.availability));
                sqlite3_bind_int64(statement, 8, location.verifiedAt);
                int const result = sqlite3_step(statement);
                sqlite3_finalize(statement);
                if (result != SQLITE_DONE) ThrowSqlite(m_db, "Upsert content location");
            }

            if (record.canonicalPieceLayer
                && record.canonicalPieceLayer->IsValidFor(record.size))
            {
                std::vector<std::uint8_t> roots;
                roots.reserve(record.canonicalPieceLayer->pieceRoots.size() * 32);
                for (auto const& root : record.canonicalPieceLayer->pieceRoots)
                    roots.insert(roots.end(), root.begin(), root.end());

                sqlite3_stmt* statement{};
                char const* sql =
                    "INSERT INTO content_piece_layer("
                    "content_key, version, piece_length, piece_roots) "
                    "VALUES(?, ?, ?, ?) "
                    "ON CONFLICT(content_key) DO UPDATE SET "
                    "version=excluded.version, piece_length=excluded.piece_length, "
                    "piece_roots=excluded.piece_roots;";
                if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
                    ThrowSqlite(m_db, "Prepare piece-layer upsert");
                BindKey(statement, 1, record.key);
                sqlite3_bind_int(statement, 2,
                    static_cast<int>(record.canonicalPieceLayer->version));
                sqlite3_bind_int(statement, 3,
                    static_cast<int>(record.canonicalPieceLayer->pieceLength));
                sqlite3_bind_blob(statement, 4, roots.data(),
                    static_cast<int>(roots.size()), SQLITE_TRANSIENT);
                int const result = sqlite3_step(statement);
                sqlite3_finalize(statement);
                if (result != SQLITE_DONE) ThrowSqlite(m_db, "Upsert piece layer");
            }

            for (auto const& resourceKey : record.resourceKeys)
            {
                sqlite3_stmt* statement{};
                char const* sql =
                    "INSERT INTO content_resource_key("
                    "algorithm, digest, content_key, observed_at) "
                    "VALUES(?, ?, ?, ?) "
                    "ON CONFLICT(algorithm, digest) DO UPDATE SET "
                    "content_key=excluded.content_key, "
                    "observed_at=excluded.observed_at;";
                if (sqlite3_prepare_v2(
                    m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
                    ThrowSqlite(m_db, "Prepare resource-key upsert");
                sqlite3_bind_int(
                    statement, 1, static_cast<int>(resourceKey.algorithm));
                sqlite3_bind_blob(
                    statement, 2,
                    resourceKey.digest.data(),
                    static_cast<int>(resourceKey.digest.size()),
                    SQLITE_TRANSIENT);
                BindKey(statement, 3, record.key);
                sqlite3_bind_int64(statement, 4, UnixNow());
                int const result = sqlite3_step(statement);
                sqlite3_finalize(statement);
                if (result != SQLITE_DONE)
                    ThrowSqlite(m_db, "Upsert content resource key");
            }

            for (auto const& source : record.sources)
            {
                sqlite3_stmt* statement{};
                char const* sql =
                    "INSERT OR IGNORE INTO content_source("
                    "content_key, source_kind, source_id, source_file_index) VALUES(?, ?, ?, ?);";
                if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
                    ThrowSqlite(m_db, "Prepare content source upsert");
                BindKey(statement, 1, record.key);
                sqlite3_bind_int(statement, 2, static_cast<int>(source.kind));
                sqlite3_bind_text(statement, 3, source.sourceId.c_str(), -1, SQLITE_TRANSIENT);
                if (source.sourceFileIndex)
                    sqlite3_bind_int(statement, 4, *source.sourceFileIndex);
                else
                    sqlite3_bind_null(statement, 4);
                int const result = sqlite3_step(statement);
                sqlite3_finalize(statement);
                if (result != SQLITE_DONE) ThrowSqlite(m_db, "Upsert content source");
            }

            if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
                ThrowSqlite(m_db, "COMMIT content catalog transaction");
        }
        catch (...)
        {
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    }

    std::optional<ContentRecord> SqliteContentCatalog::LoadRecord(ContentKey const& key) const
    {
        ContentRecord record;
        record.key = key;

        {
            sqlite3_stmt* statement{};
            if (sqlite3_prepare_v2(
                m_db, "SELECT file_size FROM content WHERE content_key=?;",
                -1, &statement, nullptr) != SQLITE_OK)
                ThrowSqlite(m_db, "Prepare content read");
            BindKey(statement, 1, key);
            int const result = sqlite3_step(statement);
            if (result != SQLITE_ROW)
            {
                sqlite3_finalize(statement);
                return std::nullopt;
            }
            record.size = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 0));
            sqlite3_finalize(statement);
        }

        {
            sqlite3_stmt* statement{};
            if (sqlite3_prepare_v2(
                m_db,
                "SELECT algorithm, digest FROM content_identity WHERE content_key=?;",
                -1, &statement, nullptr) != SQLITE_OK)
                ThrowSqlite(m_db, "Prepare identity read");
            BindKey(statement, 1, key);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                ContentIdentity identity;
                identity.algorithm = static_cast<ContentIdentityAlgorithm>(
                    sqlite3_column_int(statement, 0));
                identity.digest = ReadBlob(statement, 1);
                record.identities.push_back(std::move(identity));
            }
            sqlite3_finalize(statement);
        }

        {
            sqlite3_stmt* statement{};
            if (sqlite3_prepare_v2(
                m_db,
                "SELECT local_path, file_size, last_write_ticks, volume_serial, "
                "file_id, availability, verified_at FROM content_location WHERE content_key=?;",
                -1, &statement, nullptr) != SQLITE_OK)
                ThrowSqlite(m_db, "Prepare location read");
            BindKey(statement, 1, key);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                ContentLocation location;
                location.localPath = PathFromUtf8(
                    reinterpret_cast<char const*>(sqlite3_column_text(statement, 0)));
                location.fingerprint.fileSize =
                    static_cast<std::uint64_t>(sqlite3_column_int64(statement, 1));
                location.fingerprint.lastWriteTicks = sqlite3_column_int64(statement, 2);
                if (sqlite3_column_type(statement, 3) != SQLITE_NULL)
                    location.fingerprint.volumeSerial =
                        static_cast<std::uint64_t>(sqlite3_column_int64(statement, 3));
                if (sqlite3_column_type(statement, 4) != SQLITE_NULL)
                {
                    auto const fileId = ReadBlob(statement, 4);
                    if (fileId.size() == 16)
                    {
                        std::array<std::uint8_t, 16> value{};
                        std::copy(fileId.begin(), fileId.end(), value.begin());
                        location.fingerprint.fileId = value;
                    }
                }
                location.availability = static_cast<ContentAvailability>(
                    sqlite3_column_int(statement, 5));
                location.verifiedAt = sqlite3_column_int64(statement, 6);
                record.locations.push_back(std::move(location));
            }
            sqlite3_finalize(statement);
        }

        {
            sqlite3_stmt* statement{};
            if (sqlite3_prepare_v2(
                m_db,
                "SELECT version, piece_length, piece_roots "
                "FROM content_piece_layer WHERE content_key=?;",
                -1, &statement, nullptr) != SQLITE_OK)
                ThrowSqlite(m_db, "Prepare piece-layer read");
            BindKey(statement, 1, key);
            if (sqlite3_step(statement) == SQLITE_ROW)
            {
                CanonicalPieceLayer layer;
                layer.version = static_cast<std::uint32_t>(
                    sqlite3_column_int(statement, 0));
                layer.pieceLength = static_cast<std::uint32_t>(
                    sqlite3_column_int(statement, 1));
                auto const roots = ReadBlob(statement, 2);
                if (roots.size() % 32 == 0)
                {
                    layer.pieceRoots.resize(roots.size() / 32);
                    for (std::size_t i = 0; i < layer.pieceRoots.size(); ++i)
                    {
                        std::copy_n(
                            roots.data() + i * 32,
                            32,
                            layer.pieceRoots[i].begin());
                    }
                    if (layer.IsValidFor(record.size))
                        record.canonicalPieceLayer = std::move(layer);
                }
            }
            sqlite3_finalize(statement);
        }

        {
            sqlite3_stmt* statement{};
            if (sqlite3_prepare_v2(
                m_db,
                "SELECT algorithm, digest FROM content_resource_key "
                "WHERE content_key=?;",
                -1, &statement, nullptr) != SQLITE_OK)
                ThrowSqlite(m_db, "Prepare resource-key read");
            BindKey(statement, 1, key);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                auto const digest = ReadBlob(statement, 1);
                if (digest.size() != 32) continue;

                ResourceKey resourceKey;
                resourceKey.algorithm = static_cast<ResourceKeyAlgorithm>(
                    sqlite3_column_int(statement, 0));
                std::copy_n(
                    digest.data(),
                    resourceKey.digest.size(),
                    resourceKey.digest.begin());
                record.resourceKeys.push_back(std::move(resourceKey));
            }
            sqlite3_finalize(statement);
        }

        {
            sqlite3_stmt* statement{};
            if (sqlite3_prepare_v2(
                m_db,
                "SELECT source_kind, source_id, source_file_index "
                "FROM content_source WHERE content_key=?;",
                -1, &statement, nullptr) != SQLITE_OK)
                ThrowSqlite(m_db, "Prepare source read");
            BindKey(statement, 1, key);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                ContentSourceReference source;
                source.kind = static_cast<ContentSourceKind>(
                    sqlite3_column_int(statement, 0));
                if (auto const* value = reinterpret_cast<char const*>(
                    sqlite3_column_text(statement, 1)))
                    source.sourceId = value;
                if (sqlite3_column_type(statement, 2) != SQLITE_NULL)
                    source.sourceFileIndex = sqlite3_column_int(statement, 2);
                record.sources.push_back(std::move(source));
            }
            sqlite3_finalize(statement);
        }

        return record;
    }

    std::optional<ContentRecord> SqliteContentCatalog::FindByKey(
        ContentKey const& key) const
    {
        std::lock_guard lock(m_mutex);
        if (!const_cast<SqliteContentCatalog*>(this)->EnsureInitialized())
            return std::nullopt;
        return LoadRecord(key);
    }

    std::optional<ContentRecord> SqliteContentCatalog::FindByIdentity(
        ContentIdentity const& identity) const
    {
        if (!identity.IsWellFormed()) return std::nullopt;
        std::lock_guard lock(m_mutex);
        if (!const_cast<SqliteContentCatalog*>(this)->EnsureInitialized())
            return std::nullopt;

        sqlite3_stmt* statement{};
        char const* sql =
            "SELECT content_key FROM content_identity WHERE algorithm=? AND digest=?;";
        if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK)
            ThrowSqlite(m_db, "Prepare identity lookup");
        sqlite3_bind_int(statement, 1, static_cast<int>(identity.algorithm));
        sqlite3_bind_blob(statement, 2, identity.digest.data(),
            static_cast<int>(identity.digest.size()), SQLITE_TRANSIENT);

        std::optional<ContentKey> key;
        if (sqlite3_step(statement) == SQLITE_ROW)
            key = ReadKey(statement, 0);
        sqlite3_finalize(statement);
        return key ? LoadRecord(*key) : std::nullopt;
    }

    std::optional<ContentRecord> SqliteContentCatalog::FindByLocation(
        std::filesystem::path const& path) const
    {
        std::lock_guard lock(m_mutex);
        if (!const_cast<SqliteContentCatalog*>(this)->EnsureInitialized())
            return std::nullopt;

        sqlite3_stmt* statement{};
        if (sqlite3_prepare_v2(
            m_db,
            "SELECT content_key FROM content_location WHERE local_path=?;",
            -1, &statement, nullptr) != SQLITE_OK)
            ThrowSqlite(m_db, "Prepare location lookup");

        auto const utf8 = PathToUtf8(path);
        sqlite3_bind_text(
            statement, 1, utf8.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<ContentKey> key;
        if (sqlite3_step(statement) == SQLITE_ROW)
            key = ReadKey(statement, 0);
        sqlite3_finalize(statement);
        return key ? LoadRecord(*key) : std::nullopt;
    }

    std::vector<ContentRecord> SqliteContentCatalog::SnapshotAll() const
    {
        std::lock_guard lock(m_mutex);
        std::vector<ContentKey> keys;
        std::vector<ContentRecord> records;
        if (!const_cast<SqliteContentCatalog*>(this)->EnsureInitialized())
            return records;

        sqlite3_stmt* statement{};
        if (sqlite3_prepare_v2(
            m_db, "SELECT content_key FROM content ORDER BY created_at;",
            -1, &statement, nullptr) != SQLITE_OK)
            ThrowSqlite(m_db, "Prepare catalog snapshot");
        while (sqlite3_step(statement) == SQLITE_ROW)
            keys.push_back(ReadKey(statement, 0));
        sqlite3_finalize(statement);

        records.reserve(keys.size());
        for (auto const& key : keys)
        {
            if (auto record = LoadRecord(key))
                records.push_back(std::move(*record));
        }
        return records;
    }

    std::vector<ContentRecord> SqliteContentCatalog::SnapshotAvailable() const
    {
        auto records = SnapshotAll();
        std::erase_if(records, [](ContentRecord const& record)
        {
            return !record.IsAvailable() || record.identities.empty();
        });
        return records;
    }

    void SqliteContentCatalog::SetLocationAvailability(
        std::filesystem::path const& path,
        ContentAvailability availability)
    {
        std::lock_guard lock(m_mutex);
        if (!EnsureInitialized()) return;

        sqlite3_stmt* statement{};
        if (sqlite3_prepare_v2(
            m_db,
            "UPDATE content_location SET availability=? WHERE local_path=?;",
            -1, &statement, nullptr) != SQLITE_OK)
            ThrowSqlite(m_db, "Prepare availability update");
        auto const utf8 = PathToUtf8(path);
        sqlite3_bind_int(statement, 1, static_cast<int>(availability));
        sqlite3_bind_text(statement, 2, utf8.c_str(), -1, SQLITE_TRANSIENT);
        int const result = sqlite3_step(statement);
        sqlite3_finalize(statement);
        if (result != SQLITE_DONE) ThrowSqlite(m_db, "Update content availability");
    }

    void SqliteContentCatalog::RemoveLocation(std::filesystem::path const& path)
    {
        std::lock_guard lock(m_mutex);
        if (!EnsureInitialized()) return;

        sqlite3_stmt* statement{};
        if (sqlite3_prepare_v2(
            m_db, "DELETE FROM content_location WHERE local_path=?;",
            -1, &statement, nullptr) != SQLITE_OK)
            ThrowSqlite(m_db, "Prepare location delete");
        auto const utf8 = PathToUtf8(path);
        sqlite3_bind_text(statement, 1, utf8.c_str(), -1, SQLITE_TRANSIENT);
        int const result = sqlite3_step(statement);
        sqlite3_finalize(statement);
        if (result != SQLITE_DONE) ThrowSqlite(m_db, "Delete content location");

        sqlite3_exec(
            m_db,
            "DELETE FROM content WHERE content_key NOT IN "
            "(SELECT DISTINCT content_key FROM content_location);",
            nullptr, nullptr, nullptr);
    }
}
