#include "Core/NatMap/UdpMappingService.h"
#include "Core/NatMap/MappingFirewall.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::chrono_literals;
void Require(bool condition, char const* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct DatagramSocket
{
    SOCKET value{ INVALID_SOCKET };
    sockaddr_in address{};
    explicit DatagramSocket(std::uint16_t port = 0)
    {
        value = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        Require(value != INVALID_SOCKET, "create fixture socket");
        BOOL exclusive = TRUE;
        Require(setsockopt(value, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char*>(&exclusive), sizeof(exclusive)) == 0, "exclusive fixture port");
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        Require(bind(value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind fixture port");
        int size = sizeof(address);
        Require(getsockname(value, reinterpret_cast<sockaddr*>(&address), &size) == 0, "fixture bound address");
    }
    ~DatagramSocket() { if (value != INVALID_SOCKET) closesocket(value); }
    DatagramSocket(DatagramSocket const&) = delete;
    DatagramSocket& operator=(DatagramSocket const&) = delete;
    std::uint16_t Port() const { return ntohs(address.sin_port); }
    void Send(std::string const& bytes, sockaddr_in const& target) const
    {
        Require(sendto(value, bytes.data(), static_cast<int>(bytes.size()), 0,
            reinterpret_cast<sockaddr const*>(&target), sizeof(target)) == static_cast<int>(bytes.size()), "send fixture datagram");
    }
    std::string Receive(sockaddr_in& source) const
    {
        fd_set readers;
        FD_ZERO(&readers);
        FD_SET(value, &readers);
        timeval timeout{ 3, 0 };
        Require(select(0, &readers, nullptr, nullptr, &timeout) == 1, "receive fixture timeout");
        std::array<char, 65536> buffer{};
        int size = sizeof(source);
        int const count = recvfrom(value, buffer.data(), static_cast<int>(buffer.size()), 0,
            reinterpret_cast<sockaddr*>(&source), &size);
        Require(count >= 0, "receive fixture datagram");
        return std::string(buffer.data(), static_cast<std::size_t>(count));
    }
    bool HasDatagram(std::chrono::milliseconds wait) const
    {
        fd_set readers;
        FD_ZERO(&readers);
        FD_SET(value, &readers);
        timeval timeout{
            static_cast<long>(wait.count() / 1000),
            static_cast<long>((wait.count() % 1000) * 1000)
        };
        int const ready = select(0, &readers, nullptr, nullptr, &timeout);
        Require(ready != SOCKET_ERROR, "poll fixture socket");
        return ready == 1;
    }
};

template<typename Predicate>
void WaitFor(Predicate predicate, char const* message)
{
    auto const until = std::chrono::steady_clock::now() + 4s;
    while (!predicate())
    {
        Require(std::chrono::steady_clock::now() < until, message);
        std::this_thread::sleep_for(10ms);
    }
}

std::string Reply(std::string request)
{
    Require(request.size() == 20 && request[1] == 1, "production STUN binding request");
    request[0] = 1;
    request[1] = 1;
    request[3] = 12;
    std::array<unsigned char, 12> const attribute{ 0, 0x20, 0, 8, 0, 1, 0x93, 0x7c, 0xea, 0x12, 0xd5, 0x4b };
    request.append(reinterpret_cast<char const*>(attribute.data()), attribute.size());
    return request;
}

std::string UtpSyn(std::uint16_t connectionId)
{
    std::string packet(20, '\0');
    packet[0] = 0x41; // ST_SYN (4), uTP version 1.
    packet[2] = static_cast<char>(connectionId >> 8);
    packet[3] = static_cast<char>(connectionId & 0xff);
    return packet;
}

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
    struct Cleanup { ~Cleanup() { WSACleanup(); } } cleanup;
    try
    {
        using OpenNet::Core::NatMap::BuildMappingFirewallRule;
        using OpenNet::Core::NatMap::MappingTransport;
        auto const udpRule = BuildMappingFirewallRule(MappingTransport::Udp, 45678);
        auto const tcpRule = BuildMappingFirewallRule(MappingTransport::Tcp, 45678);
        Require(udpRule.name == L"OpenNet NAT mapping UDP 45678" && udpRule.localPort == L"45678" &&
            udpRule.protocol == IPPROTO_UDP, "UDP firewall rule descriptor");
        Require(tcpRule.name == L"OpenNet NAT mapping TCP 45678" && tcpRule.localPort == L"45678" &&
            tcpRule.protocol == IPPROTO_TCP, "TCP firewall rule descriptor");
        Require(udpRule.name != tcpRule.name && udpRule.protocol != tcpRule.protocol,
            "firewall protocols use separate rule identities");

        DatagramSocket stun;
        DatagramSocket target;
        DatagramSocket replacementTarget;
        DatagramSocket clientA;
        DatagramSocket clientB;
        OpenNet::Core::NatMap::UdpMappingService service({ 500ms, 1500ms, 5000ms });
        Require(!service.Start(target.Port(), target.Port(), L"127.0.0.1", stun.Port()), "reject relay to same port");
        Require(!service.Start(0, 0, L"127.0.0.1", stun.Port()), "reject missing target");
        Require(service.Start(0, target.Port(), L"127.0.0.1", stun.Port(), true), "start automatic-target mapping");
        WaitFor([&] { return service.Snapshot().running; }, "mapping starts");
        Require(service.Snapshot().followsTorrentTarget && service.Snapshot().targetAvailable,
            "automatic target starts available");
        sockaddr_in mapping{};
        auto response = Reply(stun.Receive(mapping));
        auto malformed = response;
        malformed[8] ^= 0xff;
        stun.Send(malformed, mapping);
        std::this_thread::sleep_for(30ms);
        Require(service.Snapshot().publicPort == 0, "wrong transaction not published");
        stun.Send(response, mapping);
        WaitFor([&] { return service.Snapshot().publicPort == 45678; }, "valid mapping published");
        Require(service.Snapshot().publicAddress == L"203.0.113.9", "mapped address parsed");
        Require(service.Snapshot().localPort == ntohs(mapping.sin_port), "one socket owns mapping and relay port");
        mapping.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        clientA.Send("d1:ad2:id20:abcdefghijklmnopqrstee", mapping);
        Require(!target.HasDatagram(150ms), "automatic target rejects DHT traffic with loopback source identity");
        clientA.Send("", mapping);
        Require(!target.HasDatagram(150ms), "automatic target rejects non-uTP empty datagram");
        std::string const peerA = UtpSyn(0x1001);
        std::string const peerB = UtpSyn(0x1002);
        clientA.Send(peerA, mapping);
        sockaddr_in insideA{};
        Require(target.Receive(insideA) == peerA, "external-to-target uTP relay A");
        clientB.Send(peerB, mapping);
        sockaddr_in insideB{};
        Require(target.Receive(insideB) == peerB, "external-to-target uTP relay B");
        Require(insideA.sin_port != insideB.sin_port, "per-peer local sockets isolate reply routing");
        target.Send("reply-B", insideB);
        target.Send("reply-A", insideA);
        sockaddr_in origin{};
        Require(clientA.Receive(origin) == "reply-A" && origin.sin_port == mapping.sin_port, "reply A uses mapping socket");
        Require(clientB.Receive(origin) == "reply-B" && origin.sin_port == mapping.sin_port, "reply B uses mapping socket");
        target.Send("", insideA);
        Require(clientA.Receive(origin).empty(), "zero-length outbound datagram");
        // Continuous invalid packets from the STUN endpoint must not starve peer
        // responses or bypass the mapping expiration check.
        std::jthread flood([&](std::stop_token stop)
        {
            while (!stop.stop_requested())
            {
                char const packet[] = "invalid STUN";
                sendto(stun.value, packet, static_cast<int>(sizeof(packet) - 1), 0, reinterpret_cast<sockaddr*>(&mapping), sizeof(mapping));
                std::this_thread::sleep_for(1ms);
            }
        });
        target.Send("during-STUN", insideA);
        Require(clientA.Receive(origin) == "during-STUN", "STUN traffic does not starve target replies");
        WaitFor([&] { return service.Snapshot().publicPort == 0 && !service.Snapshot().error.empty(); }, "stale mapping expires during STUN traffic");
        flood.request_stop();
        flood.join();
        service.RequestNetworkRecovery();
        WaitFor([&] { return service.Snapshot().publicPort == 0 && service.Snapshot().error.empty(); }, "network recovery clears stale observation");
        sockaddr_in recoveredMapping{};
        // Keepalive requests queued before recovery carry an obsolete
        // transaction ID. Reply to the bounded backlog as a real STUN server
        // would; the production parser must ignore those replies and accept
        // only the request generated after recovery.
        for (unsigned attempt = 0; attempt < 8 && service.Snapshot().publicPort == 0; ++attempt)
        {
            std::string const recoveryResponse = Reply(stun.Receive(recoveredMapping));
            Require(recoveredMapping.sin_port == mapping.sin_port, "network recovery preserves mapping port");
            stun.Send(recoveryResponse, recoveredMapping);
            std::this_thread::sleep_for(20ms);
        }
        WaitFor([&] { return service.Snapshot().publicPort == 45678; }, "network recovery refreshes STUN observation");
        recoveredMapping.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        std::string const recoveredPeer = UtpSyn(0x2001);
        clientA.Send(recoveredPeer, recoveredMapping);
        sockaddr_in recoveredInside{};
        Require(target.Receive(recoveredInside) == recoveredPeer, "uTP relay resumes after network recovery");
        target.Send("recovered-reply", recoveredInside);
        Require(clientA.Receive(origin) == "recovered-reply", "return relay resumes after network recovery");

        service.RequestAutomaticTargetPort(0);
        WaitFor([&] { return !service.Snapshot().targetAvailable && service.Snapshot().targetPort == 0; },
            "stopped torrent listener pauses relay target");
        service.RequestAutomaticTargetPort(replacementTarget.Port());
        WaitFor([&]
        {
            auto const snapshot = service.Snapshot();
            return snapshot.targetAvailable && snapshot.targetPort == replacementTarget.Port();
        }, "restarted torrent listener updates relay target");
        std::string const replacementPeer = UtpSyn(0x3001);
        clientA.Send(replacementPeer, recoveredMapping);
        sockaddr_in replacementInside{};
        Require(replacementTarget.Receive(replacementInside) == replacementPeer,
            "listener restart relays to the new target");
        replacementTarget.Send("replacement-reply", replacementInside);
        Require(clientA.Receive(origin) == "replacement-reply",
            "new target replies through the preserved mapping port");

        auto const boundPort = service.Snapshot().localPort;
        service.Stop();
        Require(!service.Snapshot().running && !service.Snapshot().starting && service.Snapshot().publicPort == 0, "stop clears state");
        {
            DatagramSocket occupied(boundPort);
            Require(service.Start(boundPort, target.Port(), L"127.0.0.1", stun.Port()), "launch occupied-port attempt");
            WaitFor([&] { return !service.Snapshot().error.empty(); }, "occupied port reports bind error");
            Require(!service.Snapshot().running, "occupied port cannot start");
            service.Stop();
        }
        Require(service.Start(boundPort, target.Port(), L"127.0.0.1", stun.Port()), "restart on released port");
        WaitFor([&] { return service.Snapshot().running; }, "released port can restart");
        mapping.sin_port = htons(boundPort);
        std::string const manualPacket = "d1:ad2:id20:abcdefghijklmnopqrstee";
        clientA.Send(manualPacket, mapping);
        Require(target.Receive(insideA) == manualPacket, "manual target retains generic UDP relay");
        clientA.Send("", mapping);
        Require(target.Receive(insideA).empty(), "manual target relays zero-length datagram");
        service.Stop();
        for (unsigned i = 0; i < 10; ++i)
        {
            Require(service.Start(0, target.Port(), L"127.0.0.1", stun.Port()), "start then immediately stop");
            service.Stop();
            Require(!service.Snapshot().running && !service.Snapshot().starting, "immediate stop joins worker");
        }
        auto const cancelStart = std::chrono::steady_clock::now();
        Require(service.Start(0, target.Port(), L"opennet-cancel-test.invalid", stun.Port()), "launch DNS startup");
        std::this_thread::sleep_for(20ms);
        service.Stop();
        Require(std::chrono::steady_clock::now() - cancelStart < 3s, "DNS cancellation does not wait for timeout");
        std::cout << "UDP mapping, separate UDP/TCP firewall identities, uTP-only automatic relay, generic manual relay, two-peer routing, STUN starvation/expiry, network recovery, automatic target lifecycle, bind conflict, restart and cancellation passed\n";
        return 0;
    }
    catch (std::exception const& error) { std::cerr << error.what() << '\n'; return 1; }
}
