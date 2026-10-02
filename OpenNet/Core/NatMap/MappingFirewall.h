#pragma once

#include <cstdint>
#include <string>

namespace OpenNet::Core::NatMap
{
    enum class MappingTransport
    {
        Udp,
        Tcp,
    };

    struct MappingFirewallRuleDescriptor
    {
        std::wstring name;
        std::wstring localPort;
        std::int32_t protocol{};
    };

    // Exposed for deterministic tests; building a descriptor never changes the
    // Windows Firewall policy.
    MappingFirewallRuleDescriptor BuildMappingFirewallRule(MappingTransport transport, std::uint16_t port);

    // A rule is installed only after an explicit command from the NAT tools UI.
    bool AllowInboundUdp(std::uint16_t port, std::wstring& error);
    bool RemoveInboundUdp(std::uint16_t port, std::wstring& error);
    bool AllowInboundTcp(std::uint16_t port, std::wstring& error);
    bool RemoveInboundTcp(std::uint16_t port, std::wstring& error);
}
