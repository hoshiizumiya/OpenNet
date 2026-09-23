/*
 * PROJECT:   OpenNet
 * FILE:      Core/Content/ContentCatalog.ixx
 * PURPOSE:   Domain model and storage boundary for long-term-seeding content.
 */

export module OpenNet.Core.Content.ContentCatalog;

import std;
import OpenNet.Core.Content.ContentIdentity;
import OpenNet.Core.Content.ResourceKey;

export namespace OpenNet::Core::Content
{
    enum class ContentAvailability : std::uint8_t
    {
        Unknown = 0,
        Available,
        Missing,
        Modified,
    };

    enum class ContentSourceKind : std::uint8_t
    {
        Unknown = 0,
        Torrent,
        Http,
        ImportedFile,
    };

    struct ContentFileFingerprint
    {
        std::uint64_t fileSize{};
        std::int64_t lastWriteTicks{};

        // Optional Windows file identity. These are cache-invalidation hints
        // only; cryptographic identities remain authoritative.
        std::optional<std::uint64_t> volumeSerial;
        std::optional<std::array<std::uint8_t, 16>> fileId;
    };

    struct ContentLocation
    {
        std::filesystem::path localPath;
        ContentAvailability availability{ ContentAvailability::Unknown };
        ContentFileFingerprint fingerprint;
        std::int64_t verifiedAt{};
    };

    struct ContentSourceReference
    {
        ContentSourceKind kind{ ContentSourceKind::Unknown };
        std::string sourceId;
        std::optional<std::int32_t> sourceFileIndex;

        bool operator==(ContentSourceReference const&) const = default;
    };

    struct CanonicalPieceLayer
    {
        static constexpr std::uint32_t CurrentVersion = 1;
        static constexpr std::uint32_t PieceLength = 1024 * 1024;

        std::uint32_t version{ CurrentVersion };
        std::uint32_t pieceLength{ PieceLength };
        std::vector<std::array<std::uint8_t, 32>> pieceRoots;

        [[nodiscard]] bool IsValidFor(std::uint64_t fileSize) const noexcept
        {
            if (fileSize == 0) return false;
            if (version != CurrentVersion || pieceLength != PieceLength)
                return false;
            auto const expected = static_cast<std::size_t>(
                1 + ((fileSize - 1) / pieceLength));
            return pieceRoots.size() == expected;
        }
    };

    struct ContentRecord
    {
        ContentKey key;
        std::uint64_t size{};
        std::vector<ContentIdentity> identities;
        std::vector<ContentLocation> locations;
        std::vector<ContentSourceReference> sources;
        std::vector<ResourceKey> resourceKeys;
        std::optional<CanonicalPieceLayer> canonicalPieceLayer;

        [[nodiscard]] bool IsAvailable() const noexcept
        {
            return std::ranges::any_of(locations, [](ContentLocation const& location)
            {
                return location.availability == ContentAvailability::Available;
            });
        }
    };

    class IContentCatalog
    {
    public:
        virtual ~IContentCatalog() = default;

        virtual void Upsert(ContentRecord record) = 0;
        virtual std::optional<ContentRecord> FindByKey(ContentKey const& key) const = 0;
        virtual std::optional<ContentRecord> FindByIdentity(ContentIdentity const& identity) const = 0;
        virtual std::optional<ContentRecord> FindByLocation(
            std::filesystem::path const& path) const = 0;
        virtual std::vector<ContentRecord> SnapshotAvailable() const = 0;
        virtual std::vector<ContentRecord> SnapshotAll() const = 0;
        virtual void SetLocationAvailability(
            std::filesystem::path const& path,
            ContentAvailability availability) = 0;
        virtual void RemoveLocation(std::filesystem::path const& path) = 0;
    };
}
