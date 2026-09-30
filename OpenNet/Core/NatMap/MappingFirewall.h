#pragma once

#include <cstdint>
#include <string>

namespace OpenNet::Core::NatMap
{
    // A rule is installed only after an explicit command from the NAT tools UI.
    bool AllowInboundUdp(std::uint16_t port, std::wstring& error);
    bool RemoveInboundUdp(std::uint16_t port, std::wstring& error);
}
