#include "Core/NatMap/StunProtocol.h"
#include "Core/NatMap/StunHostList.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace Stun = OpenNet::Core::NatMap::Stun;
void Require(bool condition, char const* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try
    {
        Stun::TransactionId const transaction{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        auto const hosts = OpenNet::Core::NatMap::ParseStunHostList(L" first.example ;192.0.2.1; third.example\t");
        Require(hosts.size() == 3 && hosts[0] == L"first.example" && hosts[1] == L"192.0.2.1" &&
            hosts[2] == L"third.example", "STUN fallback hosts parsed and trimmed");
        Require(OpenNet::Core::NatMap::ParseStunHostList(L"first;;third").empty(), "empty STUN fallback rejected");
        Require(OpenNet::Core::NatMap::ParseStunHostList(L"first;").empty(), "trailing STUN fallback rejected");
        Require(OpenNet::Core::NatMap::ParseStunHostList(std::wstring(254, L'a')).empty(), "long STUN host rejected");
        auto request = Stun::BindingRequest(transaction);
        Require(request[1] == 1 && request[2] == 0 && request[4] == 0x21 && request[19] == 11, "binding request wire format");
        std::vector<std::uint8_t> response(request.begin(), request.end());
        response[0] = 1;
        response[1] = 1;
        response[3] = 12;
        // XOR-MAPPED 203.0.113.9:45678 (port 0xb26e XOR 0x2112 = 0x937c).
        response.insert(response.end(), { 0, 0x20, 0, 8, 0, 1, 0x93, 0x7c, 0xea, 0x12, 0xd5, 0x4b });
        auto endpoint = Stun::ParseBindingResponse(response, transaction);
        Require(endpoint && !endpoint->ipv6 && endpoint->port == 45678 && endpoint->address[0] == 203 &&
                endpoint->address[2] == 113 && endpoint->address[3] == 9, "IPv4 XOR mapped endpoint");
        auto const valid = response;
        for (std::size_t size = 0; size < valid.size(); ++size)
            Require(!Stun::ParseBindingResponse(std::span(valid).first(size), transaction), "truncated reply rejected");
        for (std::size_t offset : { 0u, 1u, 4u, 8u, 19u })
        {
            response = valid;
            response[offset] ^= 0xff;
            Require(!Stun::ParseBindingResponse(response, transaction), "wrong type/cookie/transaction rejected");
        }
        response = valid;
        response.push_back(0);
        Require(!Stun::ParseBindingResponse(response, transaction), "unaccounted trailing byte rejected");
        response = valid;
        response[23] = 9;
        Require(!Stun::ParseBindingResponse(response, transaction), "attribute overrun rejected");
        response = valid;
        response[3] = 16;
        response.insert(response.end(), { 0x80, 0x22, 0, 8 });
        Require(!Stun::ParseBindingResponse(response, transaction), "invalid trailing attribute invalidates whole reply");
        response = valid;
        response[3] = 20;
        response.insert(response.end(), { 0x80, 0x22, 0, 1, 'x', 0, 0, 0 });
        Require(Stun::ParseBindingResponse(response, transaction).has_value(), "padded unknown attribute accepted");
        response = valid;
        response[26] = 0x21;
        response[27] = 0x12;
        Require(!Stun::ParseBindingResponse(response, transaction), "zero mapped port rejected");
        response = valid;
        response[25] = 3;
        Require(!Stun::ParseBindingResponse(response, transaction), "unknown address family rejected");
        response = valid;
        response[3] = 11;
        response.pop_back();
        Require(!Stun::ParseBindingResponse(response, transaction), "unaligned body rejected");
        response.assign(request.begin(), request.end());
        response[0] = 1;
        response[1] = 1;
        response[3] = 12;
        response.insert(response.end(), { 0, 1, 0, 8, 0, 1, 0xb2, 0x6e, 203, 0, 113, 9 });
        Require(Stun::ParseBindingResponse(response, transaction)->port == 45678, "legacy mapped address accepted");
        response[3] = 24;
        response.insert(response.end(), valid.begin() + 20, valid.end());
        Require(Stun::ParseBindingResponse(response, transaction)->address[3] == 9, "XOR address preferred");
        response.assign(request.begin(), request.end());
        response[0] = 1;
        response[1] = 1;
        response[3] = 24;
        // Independently specified XOR encoding of 2001:db8::1, masked by cookie + transaction.
        response.insert(response.end(), { 0, 0x20, 0, 20, 0, 2, 0x93, 0x7c,
            0x01, 0x13, 0xa9, 0xfa, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10 });
        endpoint = Stun::ParseBindingResponse(response, transaction);
        Require(endpoint && endpoint->ipv6 && endpoint->port == 45678 && endpoint->address[0] == 0x20 &&
                endpoint->address[1] == 1 && endpoint->address[2] == 0x0d && endpoint->address[3] == 0xb8 &&
                endpoint->address[15] == 1, "IPv6 XOR uses transaction mask");
        // Exercise arbitrary attribute sizes and untrusted contents under sanitizers.
        std::uint32_t state = 0x12345678;
        for (unsigned sample = 0; sample < 20000; ++sample)
        {
            response.assign(request.begin(), request.end());
            response[0] = 1;
            response[1] = 1;
            unsigned const size = (sample % 64) * 4;
            response[3] = static_cast<std::uint8_t>(size);
            for (unsigned i = 0; i < size; ++i)
            {
                state = state * 1664525u + 1013904223u;
                response.push_back(static_cast<std::uint8_t>(state >> 24));
            }
            (void)Stun::ParseBindingResponse(response, transaction);
        }
        std::cout << "STUN wire format, IPv4/IPv6, malformed packets and bounds tests passed\n";
        return 0;
    }
    catch (std::exception const& error) { std::cerr << error.what() << '\n'; return 1; }
}
