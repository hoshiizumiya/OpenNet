export module OpenNet.Core.Content.ResourceKey;

import std;

export namespace OpenNet::Core::Content
{
    enum class ResourceKeyAlgorithm : std::uint8_t
    {
        ExactUrlSha256V1 = 1,
    };

    struct ResourceKey
    {
        ResourceKeyAlgorithm algorithm{ ResourceKeyAlgorithm::ExactUrlSha256V1 };
        std::array<std::uint8_t, 32> digest{};

        [[nodiscard]] std::string ToHex() const;
        bool operator==(ResourceKey const&) const = default;
    };

    class ResourceKeyFactory
    {
    public:
        // Produces a cross-client discovery key without sending the URL itself
        // to OpenNet.Server. The exact query remains part of the hash.
        //
        // URLs with embedded credentials or query names that strongly suggest
        // signed/authentication material are deliberately excluded from the
        // shared directory rather than trying to strip secrets heuristically.
        static std::optional<ResourceKey> FromHttpUrl(
            std::string_view url);
    };
}
