module;

#include <openssl/evp.h>

module OpenNet.Core.Content.ContentHasher;

import std;

namespace OpenNet::Core::Content
{
    namespace
    {
        using Digest = std::array<std::uint8_t, 32>;

        struct EvpContextDeleter
        {
            void operator()(EVP_MD_CTX* value) const noexcept
            {
                EVP_MD_CTX_free(value);
            }
        };

        using EvpContext = std::unique_ptr<EVP_MD_CTX, EvpContextDeleter>;

        EvpContext CreateSha256Context()
        {
            EvpContext context{ EVP_MD_CTX_new() };
            if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
                throw std::runtime_error("Unable to initialize SHA-256.");
            return context;
        }

        Digest Finalize(EVP_MD_CTX* context)
        {
            Digest digest{};
            unsigned int size{};
            if (EVP_DigestFinal_ex(context, digest.data(), &size) != 1
                || size != digest.size())
                throw std::runtime_error("Unable to finalize SHA-256.");
            return digest;
        }

        Digest Hash(std::span<std::uint8_t const> input)
        {
            auto context = CreateSha256Context();
            if (!input.empty()
                && EVP_DigestUpdate(context.get(), input.data(), input.size()) != 1)
                throw std::runtime_error("Unable to update SHA-256.");
            return Finalize(context.get());
        }

        Digest ParentHash(Digest const& left, Digest const& right)
        {
            std::array<std::uint8_t, 64> joined{};
            std::copy(left.begin(), left.end(), joined.begin());
            std::copy(right.begin(), right.end(), joined.begin() + left.size());
            return Hash(joined);
        }

        ContentIdentity Identity(
            ContentIdentityAlgorithm algorithm,
            Digest const& digest)
        {
            return {
                algorithm,
                std::vector<std::uint8_t>(digest.begin(), digest.end())
            };
        }

        class MerkleAccumulator
        {
        public:
            void AddLeaf(Digest digest)
            {
                AddNode(std::move(digest), 0);
                ++m_leafCount;
            }

            [[nodiscard]] std::size_t LeafCount() const noexcept
            {
                return m_leafCount;
            }

            Digest FinalizeRoot()
            {
                if (m_leafCount == 0)
                    throw std::invalid_argument("Empty file has no BEP 52 pieces root.");

                auto const target = std::bit_ceil(m_leafCount);
                std::size_t padded = m_leafCount;
                while (padded < target)
                {
                    auto const level = static_cast<std::size_t>(
                        std::countr_zero(padded));
                    AddNode(ZeroSubtree(level), level);
                    padded += std::size_t{1} << level;
                }

                auto const finalLevel = static_cast<std::size_t>(
                    std::bit_width(target) - 1);
                if (!m_levels[finalLevel])
                    throw std::runtime_error("Unable to finalize BEP 52 Merkle root.");
                return *m_levels[finalLevel];
            }

        private:
            static Digest ZeroSubtree(std::size_t level)
            {
                Digest value{};
                while (level-- > 0)
                    value = ParentHash(value, value);
                return value;
            }

            void AddNode(Digest node, std::size_t level)
            {
                if (level >= m_levels.size())
                    throw std::overflow_error("BEP 52 Merkle tree is too deep.");

                while (m_levels[level])
                {
                    node = ParentHash(*m_levels[level], node);
                    m_levels[level].reset();
                    ++level;
                    if (level >= m_levels.size())
                        throw std::overflow_error("BEP 52 Merkle tree is too deep.");
                }
                m_levels[level] = std::move(node);
            }

            std::array<std::optional<Digest>, 64> m_levels{};
            std::size_t m_leafCount{};
        };

        Digest FixedPieceRoot(std::span<Digest const> leaves)
        {
            constexpr std::size_t blocksPerPiece =
                ContentHasher::CanonicalPieceLength / ContentHasher::Bep52BlockSize;
            static_assert(std::has_single_bit(blocksPerPiece));

            if (leaves.empty() || leaves.size() > blocksPerPiece)
                throw std::invalid_argument("Invalid canonical piece leaf count.");

            std::array<Digest, blocksPerPiece> work{};
            std::copy(leaves.begin(), leaves.end(), work.begin());

            std::size_t width = work.size();
            while (width > 1)
            {
                for (std::size_t i = 0; i < width; i += 2)
                    work[i / 2] = ParentHash(work[i], work[i + 1]);
                width /= 2;
            }
            return work.front();
        }
    }

    ContentHashResult ContentHasher::HashFile(
        std::filesystem::path const& path,
        std::stop_token stopToken)
    {
        if (stopToken.stop_requested())
            throw std::runtime_error("Content hashing cancelled.");

        std::error_code sizeError;
        auto const expectedSize = std::filesystem::file_size(path, sizeError);
        if (sizeError)
            throw std::runtime_error("Unable to determine file size before hashing.");

        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            throw std::runtime_error("Unable to open file for content hashing.");

        auto wholeFile = CreateSha256Context();
        MerkleAccumulator fileTree;
        std::vector<Digest> pieceLeaves;
        pieceLeaves.reserve(CanonicalPieceLength / Bep52BlockSize);
        std::vector<Digest> canonicalPieceRoots;
        canonicalPieceRoots.reserve(static_cast<std::size_t>(
            (expectedSize + CanonicalPieceLength - 1) / CanonicalPieceLength));

        std::array<std::uint8_t, Bep52BlockSize> buffer{};
        std::uint64_t total{};

        while (stream)
        {
            if (stopToken.stop_requested())
                throw std::runtime_error("Content hashing cancelled.");

            stream.read(
                reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
            auto const read = stream.gcount();
            if (read <= 0) break;

            auto const bytes = std::span<std::uint8_t const>(
                buffer.data(), static_cast<std::size_t>(read));
            if (EVP_DigestUpdate(wholeFile.get(), bytes.data(), bytes.size()) != 1)
                throw std::runtime_error("Unable to update whole-file SHA-256.");

            auto leaf = Hash(bytes);
            fileTree.AddLeaf(leaf);
            pieceLeaves.push_back(leaf);
            total += static_cast<std::uint64_t>(read);

            if (expectedSize > CanonicalPieceLength
                && pieceLeaves.size()
                    == CanonicalPieceLength / Bep52BlockSize)
            {
                canonicalPieceRoots.push_back(FixedPieceRoot(pieceLeaves));
                pieceLeaves.clear();
            }
        }

        if (stopToken.stop_requested())
            throw std::runtime_error("Content hashing cancelled.");
        if (!stream.eof() && stream.fail())
            throw std::runtime_error("Unable to read file while hashing content.");
        if (total != expectedSize)
            throw std::runtime_error("File size changed while hashing content.");

        ContentHashResult result;
        result.size = total;
        result.identities.push_back(Identity(
            ContentIdentityAlgorithm::WholeFileSha256,
            Finalize(wholeFile.get())));

        if (total != 0)
        {
            auto const fileRoot = fileTree.FinalizeRoot();
            result.identities.insert(
                result.identities.begin(),
                Identity(
                    ContentIdentityAlgorithm::Bep52FileRootSha256,
                    fileRoot));

            if (total <= CanonicalPieceLength)
            {
                canonicalPieceRoots.clear();
                canonicalPieceRoots.push_back(fileRoot);
            }
            else if (!pieceLeaves.empty())
            {
                canonicalPieceRoots.push_back(FixedPieceRoot(pieceLeaves));
            }

            CanonicalPieceLayer layer;
            layer.pieceRoots = std::move(canonicalPieceRoots);
            if (!layer.IsValidFor(total))
                throw std::runtime_error("Generated canonical piece layer is inconsistent.");
            result.canonicalPieceLayer = std::move(layer);
        }

        return result;
    }
}
