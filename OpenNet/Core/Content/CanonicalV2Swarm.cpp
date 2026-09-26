module;

#include <libtorrent/create_torrent.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/sha1_hash.hpp>

module OpenNet.Core.Content.CanonicalV2Swarm;

import std;

namespace OpenNet::Core::Content
{
    namespace lt = libtorrent;

    namespace
    {
        lt::sha256_hash ToLibtorrent(
            std::array<std::uint8_t, 32> const& value)
        {
            lt::sha256_hash hash;
            std::memcpy(hash.data(), value.data(), value.size());
            return hash;
        }
    }

    CanonicalV2Torrent CanonicalV2Swarm::Build(ContentRecord const& content)
    {
        if (content.size == 0)
            throw std::invalid_argument(
                "OpenNet.Content.v1 does not create canonical torrents for empty files.");
        if (content.size
            > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
            throw std::overflow_error(
                "Content is too large for libtorrent file offsets.");
        if (!content.canonicalPieceLayer
            || !content.canonicalPieceLayer->IsValidFor(content.size))
            throw std::invalid_argument(
                "Canonical piece layer is missing or inconsistent.");

        std::vector<lt::create_file_entry> files;
        files.push_back({
            "OpenNet.Content.v1/content",
            static_cast<std::int64_t>(content.size),
            {},
            0,
            {}
        });

        lt::create_torrent creator(
            std::move(files),
            static_cast<int>(PieceLength),
            lt::create_torrent::v2_only);
        creator.set_creation_date(0);

        auto const& roots = content.canonicalPieceLayer->pieceRoots;
        for (std::size_t i = 0; i < roots.size(); ++i)
        {
            creator.set_hash2(
                lt::file_index_t{0},
                lt::piece_index_t::diff_type{
                    static_cast<int>(i) },
                ToLibtorrent(roots[i]));
        }

        auto buffer = creator.generate_buf();
        auto params = lt::load_torrent_buffer(
            lt::span<char const>(buffer.data(), buffer.size()));
        if (!params.ti || !params.ti->info_hashes().has_v2())
            throw std::runtime_error(
                "Generated canonical torrent has no v2 info-hash.");

        CanonicalV2Torrent result;
        result.metainfo.assign(buffer.begin(), buffer.end());
        auto const hash = params.ti->info_hashes().v2;
        std::memcpy(
            result.infoHashV2.data(),
            hash.data(),
            result.infoHashV2.size());
        return result;
    }

    bool CanonicalV2Swarm::ValidateManifest(
        ContentIdentity const& expectedIdentity,
        std::uint64_t expectedSize,
        std::vector<std::uint8_t> const& metainfo,
        std::string_view expectedInfoHashV2)
    {
        if (expectedIdentity.algorithm
            != ContentIdentityAlgorithm::Bep52FileRootSha256
            || !expectedIdentity.IsWellFormed()
            || expectedSize == 0
            || expectedSize
                > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())
            || metainfo.empty())
            return false;

        try
        {
            auto params = lt::load_torrent_buffer(
                lt::span<char const>(
                    reinterpret_cast<char const*>(metainfo.data()),
                    metainfo.size()));
            if (!params.ti
                || !params.ti->info_hashes().has_v2()
                || params.ti->num_files() != 1
                || params.ti->total_size()
                    != static_cast<std::int64_t>(expectedSize)
                || params.ti->piece_length()
                    != static_cast<int>(PieceLength))
                return false;

            auto const path = params.ti->layout().file_path(
                lt::file_index_t{0});
            if (path != "OpenNet.Content.v1/content")
                return false;

            auto const root = params.ti->layout().root(
                lt::file_index_t{0});
            if (root.is_all_zeros()
                || expectedIdentity.digest.size()
                    != static_cast<std::size_t>(root.size())
                || std::memcmp(
                    root.data(),
                    expectedIdentity.digest.data(),
                    expectedIdentity.digest.size()) != 0)
                return false;

            std::array<std::uint8_t, 32> infoHash{};
            auto const hash = params.ti->info_hashes().v2;
            std::memcpy(infoHash.data(), hash.data(), infoHash.size());
            return InfoHashHex(infoHash) == expectedInfoHashV2;
        }
        catch (...)
        {
            return false;
        }
    }

    std::string CanonicalV2Swarm::InfoHashHex(
        std::array<std::uint8_t, 32> const& hash)
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::string result(hash.size() * 2, '\0');
        for (std::size_t i = 0; i < hash.size(); ++i)
        {
            result[i * 2] = digits[hash[i] >> 4];
            result[i * 2 + 1] = digits[hash[i] & 0x0f];
        }
        return result;
    }
}
