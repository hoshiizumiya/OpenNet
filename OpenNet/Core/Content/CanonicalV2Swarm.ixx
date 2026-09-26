export module OpenNet.Core.Content.CanonicalV2Swarm;

import std;
export import OpenNet.Core.Content.ContentCatalog;

export namespace OpenNet::Core::Content
{
    struct CanonicalV2Torrent
    {
        std::vector<std::uint8_t> metainfo;
        std::array<std::uint8_t, 32> infoHashV2{};
    };

    class CanonicalV2Swarm
    {
    public:
        static constexpr std::uint32_t ProtocolVersion = 1;
        static constexpr std::uint32_t PieceLength =
            CanonicalPieceLayer::PieceLength;

        // OpenNet.Content.v1 is deliberately independent of source URL,
        // original filename, source torrent piece size, trackers and timestamps.
        static CanonicalV2Torrent Build(ContentRecord const& content);

        // Downloaders validate the server-supplied metainfo against the
        // authoritative BEP 52 file root before libtorrent is allowed to write.
        static bool ValidateManifest(
            ContentIdentity const& expectedIdentity,
            std::uint64_t expectedSize,
            std::vector<std::uint8_t> const& metainfo,
            std::string_view expectedInfoHashV2);

        static std::string InfoHashHex(
            std::array<std::uint8_t, 32> const& hash);
    };
}
