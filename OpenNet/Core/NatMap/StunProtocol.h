#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace OpenNet::Core::NatMap::Stun
{
    using TransactionId = std::array<std::uint8_t, 12>;

    struct MappedEndpoint
    {
        bool ipv6{};
        std::uint16_t port{};
        std::array<std::uint8_t, 16> address{};
    };

    inline std::uint16_t Read16(std::uint8_t const* bytes) noexcept
    {
        return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
    }

    inline std::array<std::uint8_t, 20> BindingRequest(TransactionId const& transaction) noexcept
    {
        std::array<std::uint8_t, 20> request{ 0, 1, 0, 0, 0x21, 0x12, 0xa4, 0x42 };
        std::copy(transaction.begin(), transaction.end(), request.begin() + 8);
        return request;
    }

    // Datagram length, transaction, attribute padding and the whole message are
    // validated before returning an endpoint. Never publish a partially parsed reply.
    inline std::optional<MappedEndpoint> ParseBindingResponse(std::span<std::uint8_t const> data, TransactionId const& transaction) noexcept
    {
        if (data.size() < 20 || Read16(data.data()) != 0x0101 ||
            data[4] != 0x21 || data[5] != 0x12 || data[6] != 0xa4 || data[7] != 0x42 ||
            !std::equal(transaction.begin(), transaction.end(), data.begin() + 8)) return std::nullopt;
        auto const bodySize = Read16(data.data() + 2);
        if (bodySize % 4 != 0 || data.size() != 20u + bodySize) return std::nullopt;

        std::optional<MappedEndpoint> mapped;
        std::optional<MappedEndpoint> xorMapped;
        for (std::size_t offset = 20; offset < data.size();)
        {
            if (offset + 4 > data.size()) return std::nullopt;
            auto const type = Read16(data.data() + offset);
            auto const size = Read16(data.data() + offset + 2);
            auto const next = offset + 4 + ((static_cast<std::size_t>(size) + 3) & ~std::size_t{ 3 });
            if (next > data.size()) return std::nullopt;
            if (type == 0x0020 || type == 0x0001)
            {
                auto const* value = data.data() + offset + 4;
                if (size < 2) return std::nullopt;
                auto const family = value[1];
                if ((family == 1 && size != 8) || (family == 2 && size != 20)) return std::nullopt;
                if (family != 1 && family != 2) return std::nullopt;
                MappedEndpoint endpoint;
                endpoint.ipv6 = family == 2;
                endpoint.port = static_cast<std::uint16_t>(Read16(value + 2) ^ (type == 0x0020 ? 0x2112 : 0));
                if (!endpoint.port) return std::nullopt;
                auto const addressSize = endpoint.ipv6 ? 16u : 4u;
                for (std::size_t i = 0; i < addressSize; ++i)
                    endpoint.address[i] = static_cast<std::uint8_t>(value[4 + i] ^ (type == 0x0020 ? data[4 + i] : 0));
                if (type == 0x0020) xorMapped = endpoint;
                else mapped = endpoint;
            }
            offset = next;
        }
        return xorMapped ? xorMapped : mapped;
    }
}
