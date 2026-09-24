export module OpenNet.Core.Content.ContentHasher;

import std;
export import OpenNet.Core.Content.ContentCatalog;

export namespace OpenNet::Core::Content
{
    struct ContentHashResult
    {
        std::uint64_t size{};
        std::vector<ContentIdentity> identities;
        std::optional<CanonicalPieceLayer> canonicalPieceLayer;
    };

    class ContentHasher
    {
    public:
        static constexpr std::size_t Bep52BlockSize = 16 * 1024;
        static constexpr std::size_t CanonicalPieceLength =
            CanonicalPieceLayer::PieceLength;

        // Hashes a completed file. The BEP 52 identity is the SHA-256 Merkle
        // root over 16 KiB leaves. Empty files have no pieces-root identity.
        static ContentHashResult HashFile(std::filesystem::path const& path);
    };
}
