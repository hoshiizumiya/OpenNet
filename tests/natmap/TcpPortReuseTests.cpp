#include "Core/NatMap/StunProtocol.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

void Require(bool condition, char const* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Socket
{
    SOCKET value{ INVALID_SOCKET };

    Socket() = default;
    explicit Socket(SOCKET socket) : value(socket) {}
    ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
    Socket(Socket const&) = delete;
    Socket& operator=(Socket const&) = delete;
    Socket(Socket&& other) noexcept : value(std::exchange(other.value, INVALID_SOCKET)) {}
    Socket& operator=(Socket&& other) noexcept
    {
        if (this != &other)
        {
            if (value != INVALID_SOCKET) closesocket(value);
            value = std::exchange(other.value, INVALID_SOCKET);
        }
        return *this;
    }
};

sockaddr_in Loopback(std::uint16_t port)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    return address;
}

void ConfigureTimeouts(SOCKET socket)
{
    DWORD const timeout = 3000;
    Require(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<char const*>(&timeout), sizeof(timeout)) == 0, "set receive timeout");
    Require(setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
        reinterpret_cast<char const*>(&timeout), sizeof(timeout)) == 0, "set send timeout");
}

void ConfigureReuse(SOCKET socket)
{
    BOOL const enabled = TRUE;
    Require(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<char const*>(&enabled), sizeof(enabled)) == 0, "enable address reuse");
}

std::uint16_t BoundPort(SOCKET socket)
{
    sockaddr_in address{};
    int size = sizeof(address);
    Require(getsockname(socket, reinterpret_cast<sockaddr*>(&address), &size) == 0, "query bound port");
    return ntohs(address.sin_port);
}

Socket MakeListener(std::uint16_t port = 0, bool reuse = false)
{
    Socket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    Require(socket.value != INVALID_SOCKET, "create listener");
    ConfigureTimeouts(socket.value);
    if (reuse) ConfigureReuse(socket.value);
    sockaddr_in address = Loopback(port);
    Require(bind(socket.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind listener");
    Require(listen(socket.value, SOMAXCONN) == 0, "listen");
    return socket;
}

Socket ConnectFrom(std::uint16_t sourcePort, sockaddr_in const& destination, bool reuse)
{
    Socket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    Require(socket.value != INVALID_SOCKET, "create client");
    ConfigureTimeouts(socket.value);
    if (reuse) ConfigureReuse(socket.value);
    sockaddr_in source = Loopback(sourcePort);
    Require(bind(socket.value, reinterpret_cast<sockaddr*>(&source), sizeof(source)) == 0, "bind client source port");
    Require(connect(socket.value, reinterpret_cast<sockaddr const*>(&destination), sizeof(destination)) == 0,
        "connect client");
    return socket;
}

Socket Accept(SOCKET listener, sockaddr_in& source)
{
    int size = sizeof(source);
    Socket socket(accept(listener, reinterpret_cast<sockaddr*>(&source), &size));
    Require(socket.value != INVALID_SOCKET, "accept connection");
    ConfigureTimeouts(socket.value);
    return socket;
}

void SendAll(SOCKET socket, std::string const& bytes)
{
    std::size_t offset{};
    while (offset < bytes.size())
    {
        int const count = send(socket, bytes.data() + offset,
            static_cast<int>(bytes.size() - offset), 0);
        Require(count > 0, "send stream bytes");
        offset += static_cast<std::size_t>(count);
    }
}

std::string ReceiveExact(SOCKET socket, std::size_t size)
{
    std::string bytes(size, '\0');
    std::size_t offset{};
    while (offset < size)
    {
        int const count = recv(socket, bytes.data() + offset,
            static_cast<int>(size - offset), 0);
        Require(count > 0, "receive stream bytes");
        offset += static_cast<std::size_t>(count);
    }
    return bytes;
}

std::string StunReply(std::string request)
{
    Require(request.size() == 20 && request[0] == 0 && request[1] == 1,
        "TCP STUN binding request");
    request[0] = 1;
    request[1] = 1;
    request[3] = 12;
    std::array<unsigned char, 12> const attribute{
        0, 0x20, 0, 8, 0, 1, 0x93, 0x7c, 0xea, 0x12, 0xd5, 0x4b
    };
    request.append(reinterpret_cast<char const*>(attribute.data()), attribute.size());
    return request;
}

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
    struct Cleanup { ~Cleanup() { WSACleanup(); } } cleanup;

    try
    {
        Socket keeperServer = MakeListener();
        Socket stunServer = MakeListener();
        Socket targetServer = MakeListener();
        sockaddr_in keeperAddress = Loopback(BoundPort(keeperServer.value));
        sockaddr_in stunAddress = Loopback(BoundPort(stunServer.value));
        sockaddr_in targetAddress = Loopback(BoundPort(targetServer.value));

        // Match upstream NATMap's order: the HTTP keeper selects the source
        // port, TCP STUN reuses it, then the public listener binds it.
        Socket keeperClient = ConnectFrom(0, keeperAddress, true);
        std::uint16_t const mappingPort = BoundPort(keeperClient.value);
        Socket stunClient = ConnectFrom(mappingPort, stunAddress, true);
        Socket mappingListener = MakeListener(mappingPort, true);

        sockaddr_in keeperSource{};
        Socket keeperPeer = Accept(keeperServer.value, keeperSource);
        sockaddr_in stunSource{};
        Socket stunPeer = Accept(stunServer.value, stunSource);
        Require(ntohs(keeperSource.sin_port) == mappingPort && ntohs(stunSource.sin_port) == mappingPort,
            "keeper and STUN use the public source port");

        std::string const head = "HEAD / HTTP/1.1\r\nHost: fixture\r\nConnection: keep-alive\r\n\r\n";
        SendAll(keeperClient.value, head);
        Require(ReceiveExact(keeperPeer.value, head.size()) == head, "HTTP keepalive request");
        std::string const headReply = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        SendAll(keeperPeer.value, headReply);
        Require(ReceiveExact(keeperClient.value, headReply.size()) == headReply, "HTTP keepalive response");

        OpenNet::Core::NatMap::Stun::TransactionId const transaction{
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
        };
        std::array<std::uint8_t, 20> const request =
            OpenNet::Core::NatMap::Stun::BindingRequest(transaction);
        std::string const requestBytes(reinterpret_cast<char const*>(request.data()), request.size());
        SendAll(stunClient.value, requestBytes);
        std::string const response = StunReply(ReceiveExact(stunPeer.value, requestBytes.size()));
        SendAll(stunPeer.value, response);
        std::string const responseBytes = ReceiveExact(stunClient.value, response.size());
        std::span<std::uint8_t const> const packet(
            reinterpret_cast<std::uint8_t const*>(responseBytes.data()), responseBytes.size());
        auto const mapped = OpenNet::Core::NatMap::Stun::ParseBindingResponse(packet, transaction);
        Require(mapped && !mapped->ipv6 && mapped->port == 45678, "TCP STUN mapped endpoint");

        Socket externalClient = ConnectFrom(0, Loopback(mappingPort), false);
        sockaddr_in externalSource{};
        Socket publicPeer = Accept(mappingListener.value, externalSource);
        Socket targetClient = ConnectFrom(0, targetAddress, false);
        sockaddr_in targetSource{};
        Socket targetPeer = Accept(targetServer.value, targetSource);
        Require(externalSource.sin_addr.s_addr == htonl(INADDR_LOOPBACK) &&
            targetSource.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "relay fixture endpoints");

        std::string const requestPayload = "GET /api/v2/app/version HTTP/1.1\r\nHost: public.example\r\n\r\n";
        SendAll(externalClient.value, requestPayload);
        std::string const inbound = ReceiveExact(publicPeer.value, requestPayload.size());
        SendAll(targetClient.value, inbound);
        Require(ReceiveExact(targetPeer.value, requestPayload.size()) == requestPayload,
            "public-to-WebUI relay path");

        std::string const responsePayload = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
        SendAll(targetPeer.value, responsePayload);
        std::string const outbound = ReceiveExact(targetClient.value, responsePayload.size());
        SendAll(publicPeer.value, outbound);
        Require(ReceiveExact(externalClient.value, responsePayload.size()) == responsePayload,
            "WebUI-to-public relay path");

        Socket exclusive(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        Require(exclusive.value != INVALID_SOCKET, "create exclusive probe");
        BOOL const enabled = TRUE;
        Require(setsockopt(exclusive.value, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<char const*>(&enabled), sizeof(enabled)) == 0, "enable exclusive probe");
        sockaddr_in exclusiveAddress = Loopback(mappingPort);
        Require(bind(exclusive.value, reinterpret_cast<sockaddr*>(&exclusiveAddress), sizeof(exclusiveAddress)) == SOCKET_ERROR,
            "shared TCP mapping port cannot also be exclusive");
        int const exclusiveError = WSAGetLastError();
        Require(exclusiveError == WSAEADDRINUSE || exclusiveError == WSAEACCES,
            "exclusive probe returns address conflict");

        std::cout << "TCP same-source-port HTTP keepalive, STUN, listener, bidirectional relay and exclusivity boundary passed\n";
        return 0;
    }
    catch (std::exception const& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
