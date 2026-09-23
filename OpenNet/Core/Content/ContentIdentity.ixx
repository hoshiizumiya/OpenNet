/*
 * PROJECT:   OpenNet
 * FILE:      Core/Content/ContentIdentity.ixx
 * PURPOSE:   Protocol-neutral identities for files kept in the long-term
 *            content catalog.
 */

export module OpenNet.Core.Content.ContentIdentity;

import std;

export namespace OpenNet::Core::Content
{
    enum class ContentIdentityAlgorithm : std::uint8_t
    {
        Bep52FileRootSha256 = 1,
        WholeFileSha1 = 2,
        WholeFileSha256 = 3,

        // BitComet publicly documents this value only as a 160-bit LT-Seeding
        // file hash. Keep it opaque until the algorithm is independently
        // verified or documented by BitComet.
        BitCometLtSeed160Opaque = 128,
    };

    struct ContentIdentity
    {
        ContentIdentityAlgorithm algorithm{};
        std::vector<std::uint8_t> digest;

        [[nodiscard]] static constexpr std::size_t ExpectedDigestBytes(ContentIdentityAlgorithm value) noexcept
        {
            switch (value)
            {
            case ContentIdentityAlgorithm::Bep52FileRootSha256:
            case ContentIdentityAlgorithm::WholeFileSha256:
                return 32;
            case ContentIdentityAlgorithm::WholeFileSha1:
            case ContentIdentityAlgorithm::BitCometLtSeed160Opaque:
                return 20;
            default:
                return 0;
            }
        }

        [[nodiscard]] bool IsWellFormed() const noexcept
        {
            auto const expected = ExpectedDigestBytes(algorithm);
            return expected != 0 && digest.size() == expected;
        }

        [[nodiscard]] std::string ToHex() const
        {
            static constexpr char digits[] = "0123456789abcdef";
            std::string result;
            result.resize(digest.size() * 2);
            for (std::size_t index = 0; index < digest.size(); ++index)
            {
                result[index * 2] = digits[digest[index] >> 4];
                result[index * 2 + 1] = digits[digest[index] & 0x0f];
            }
            return result;
        }

        bool operator==(ContentIdentity const&) const = default;
    };

    // Stable local catalog key. It deliberately is not a cryptographic content
    // hash so one logical content object may acquire additional protocol
    // identities later without changing its local primary key.
    struct ContentKey
    {
        std::array<std::uint8_t, 16> value{};

        bool operator==(ContentKey const&) const = default;
    };
}
