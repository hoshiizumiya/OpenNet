#include "Core/NatMap/TcpMappingService.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <stop_token>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

namespace
{
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

    sockaddr_in Loopback(std::uint16_t port, std::uint32_t ipv4 = INADDR_LOOPBACK)
    {
        sockaddr_in endpoint{};
        endpoint.sin_family = AF_INET;
        endpoint.sin_addr.s_addr = htonl(ipv4);
        endpoint.sin_port = htons(port);
        return endpoint;
    }

    void ConfigureTimeouts(SOCKET socket)
    {
        DWORD const timeout = 3000;
        Require(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<char const*>(&timeout), sizeof(timeout)) == 0, "set receive timeout");
        Require(setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
            reinterpret_cast<char const*>(&timeout), sizeof(timeout)) == 0, "set send timeout");
    }

    Socket MakeListener(std::uint32_t ipv4 = INADDR_LOOPBACK, std::uint16_t port = 0)
    {
        Socket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        Require(socket.value != INVALID_SOCKET, "create fixture listener");
        BOOL const exclusive = TRUE;
        Require(setsockopt(socket.value, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<char const*>(&exclusive), sizeof(exclusive)) == 0,
            "protect fixture listener");
        sockaddr_in address = Loopback(port, ipv4);
        Require(bind(socket.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
            "bind fixture listener");
        Require(listen(socket.value, SOMAXCONN) == 0, "listen fixture");
        return socket;
    }

    std::uint16_t BoundPort(SOCKET socket)
    {
        sockaddr_in address{};
        int size = sizeof(address);
        Require(getsockname(socket, reinterpret_cast<sockaddr*>(&address), &size) == 0,
            "query fixture port");
        return ntohs(address.sin_port);
    }

    bool WaitReadable(SOCKET socket, std::stop_token stop)
    {
        while (!stop.stop_requested())
        {
            fd_set readers;
            FD_ZERO(&readers);
            FD_SET(socket, &readers);
            timeval timeout{ 0, 100000 };
            int const ready = select(0, &readers, nullptr, nullptr, &timeout);
            if (ready == SOCKET_ERROR) return false;
            if (ready == 1) return true;
        }
        return false;
    }

    bool SendAll(SOCKET socket, std::span<char const> bytes)
    {
        std::size_t offset{};
        while (offset < bytes.size())
        {
            int const count = send(socket, bytes.data() + offset,
                static_cast<int>(bytes.size() - offset), 0);
            if (count <= 0) return false;
            offset += static_cast<std::size_t>(count);
        }
        return true;
    }

    bool ReceiveExact(SOCKET socket, std::span<char> bytes, std::stop_token stop)
    {
        std::size_t offset{};
        while (offset < bytes.size() && WaitReadable(socket, stop))
        {
            int const count = recv(socket, bytes.data() + offset,
                static_cast<int>(bytes.size() - offset), 0);
            if (count <= 0) return false;
            offset += static_cast<std::size_t>(count);
        }
        return offset == bytes.size();
    }

    Socket Accept(SOCKET listener, std::stop_token stop, sockaddr_in* source = nullptr)
    {
        if (!WaitReadable(listener, stop)) return {};
        int sourceSize = sizeof(sockaddr_in);
        Socket socket(accept(listener, source ? reinterpret_cast<sockaddr*>(source) : nullptr,
            source ? &sourceSize : nullptr));
        if (socket.value != INVALID_SOCKET) ConfigureTimeouts(socket.value);
        return socket;
    }

    class FixtureServers final
    {
    public:
        FixtureServers()
            : m_keeperListener(MakeListener()), m_silentStunListener(MakeListener(0x7f000002)),
              m_stunListener(MakeListener(INADDR_LOOPBACK, BoundPort(m_silentStunListener.value))),
              m_targetListener(MakeListener()),
              m_keeperRequests{}, m_silentStunConnections{}, m_stunRequests{}, m_failed{},
              m_keeper([this](std::stop_token stop) { RunKeeper(stop); }),
              m_silentStun([this](std::stop_token stop) { RunSilentStun(stop); }),
              m_stun([this](std::stop_token stop) { RunStun(stop); }),
              m_target([this](std::stop_token stop) { RunTarget(stop); })
        {
        }

        [[nodiscard]] std::uint16_t KeeperPort() const { return BoundPort(m_keeperListener.value); }
        [[nodiscard]] std::uint16_t StunPort() const { return BoundPort(m_stunListener.value); }
        [[nodiscard]] std::uint16_t TargetPort() const { return BoundPort(m_targetListener.value); }
        [[nodiscard]] std::size_t KeeperRequests() const { return m_keeperRequests.load(); }
        [[nodiscard]] std::size_t SilentStunConnections() const { return m_silentStunConnections.load(); }
        [[nodiscard]] std::size_t StunRequests() const { return m_stunRequests.load(); }
        [[nodiscard]] std::uint16_t SilentStunSourcePort() const { return m_silentStunSourcePort.load(); }
        [[nodiscard]] std::uint16_t ResponsiveStunSourcePort() const { return m_responsiveStunSourcePort.load(); }
        [[nodiscard]] bool Failed() const { return m_failed.load(); }

    private:
        void RunKeeper(std::stop_token stop)
        {
            std::string const reply =
                "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
            while (!stop.stop_requested())
            {
                Socket peer = Accept(m_keeperListener.value, stop);
                if (peer.value == INVALID_SOCKET) continue;
                std::string pending;
                std::array<char, 1024> bytes{};
                while (!stop.stop_requested() && WaitReadable(peer.value, stop))
                {
                    int const count = recv(peer.value, bytes.data(), static_cast<int>(bytes.size()), 0);
                    if (count <= 0) break;
                    pending.append(bytes.data(), static_cast<std::size_t>(count));
                    std::size_t end{};
                    while ((end = pending.find("\r\n\r\n")) != std::string::npos)
                    {
                        pending.erase(0, end + 4);
                        ++m_keeperRequests;
                        if (!SendAll(peer.value, std::span<char const>(reply.data(), reply.size())))
                        {
                            m_failed.store(true);
                            break;
                        }
                    }
                }
            }
        }

        void RunStun(std::stop_token stop)
        {
            while (!stop.stop_requested())
            {
                sockaddr_in source{};
                Socket peer = Accept(m_stunListener.value, stop, &source);
                if (peer.value == INVALID_SOCKET) continue;
                m_responsiveStunSourcePort.store(ntohs(source.sin_port));
                std::array<char, 20> request{};
                if (!ReceiveExact(peer.value, std::span<char>(request), stop)) continue;
                if (request[0] != 0 || request[1] != 1)
                {
                    m_failed.store(true);
                    continue;
                }
                std::array<char, 32> response{};
                response[0] = 1;
                response[1] = 1;
                response[3] = 12;
                std::copy(request.begin() + 4, request.end(), response.begin() + 4);
                std::array<unsigned char, 12> const attribute{
                    0, 0x20, 0, 8, 0, 1, 0x93, 0x7c, 0xea, 0x12, 0xd5, 0x4b
                };
                std::copy(attribute.begin(), attribute.end(),
                    reinterpret_cast<unsigned char*>(response.data() + 20));
                ++m_stunRequests;
                if (!SendAll(peer.value, std::span<char const>(response.data(), response.size())))
                    m_failed.store(true);
            }
        }

        void RunSilentStun(std::stop_token stop)
        {
            while (!stop.stop_requested())
            {
                sockaddr_in source{};
                Socket peer = Accept(m_silentStunListener.value, stop, &source);
                if (peer.value == INVALID_SOCKET) continue;
                ++m_silentStunConnections;
                m_silentStunSourcePort.store(ntohs(source.sin_port));
                // Keep the TCP connection open without replying. Production
                // must time out this candidate and retry the next host while
                // continuing to use the keeper-selected source port.
                while (!stop.stop_requested()) std::this_thread::sleep_for(50ms);
            }
        }

        void RunTarget(std::stop_token stop)
        {
            while (!stop.stop_requested())
            {
                Socket peer = Accept(m_targetListener.value, stop);
                if (peer.value == INVALID_SOCKET) continue;
                std::array<char, 2048> bytes{};
                while (!stop.stop_requested() && WaitReadable(peer.value, stop))
                {
                    int const count = recv(peer.value, bytes.data(), static_cast<int>(bytes.size()), 0);
                    if (count <= 0) break;
                    if (!SendAll(peer.value, std::span<char const>(bytes.data(), static_cast<std::size_t>(count))))
                    {
                        m_failed.store(true);
                        break;
                    }
                }
            }
        }

        Socket m_keeperListener;
        Socket m_silentStunListener;
        Socket m_stunListener;
        Socket m_targetListener;
        std::atomic_size_t m_keeperRequests{};
        std::atomic_size_t m_silentStunConnections{};
        std::atomic_size_t m_stunRequests{};
        std::atomic_uint16_t m_silentStunSourcePort{};
        std::atomic_uint16_t m_responsiveStunSourcePort{};
        std::atomic_bool m_failed{};
        std::jthread m_keeper;
        std::jthread m_silentStun;
        std::jthread m_stun;
        std::jthread m_target;
    };

    template<typename Predicate>
    bool WaitUntil(Predicate predicate, std::chrono::milliseconds timeout = 8s)
    {
        std::chrono::steady_clock::time_point const until = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < until)
        {
            if (predicate()) return true;
            std::this_thread::sleep_for(20ms);
        }
        return predicate();
    }

    Socket Connect(std::uint16_t port)
    {
        Socket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        Require(socket.value != INVALID_SOCKET, "create external client");
        ConfigureTimeouts(socket.value);
        sockaddr_in address = Loopback(port);
        Require(connect(socket.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
            "connect external client");
        return socket;
    }

    void ExerciseRelay(OpenNet::Core::NatMap::TcpMappingService& service)
    {
        auto const snapshot = service.Snapshot();
        Socket client = Connect(snapshot.localPort);
        std::string const payload = "GET /api/v2/app/version HTTP/1.1\r\nHost: public.example\r\n\r\n";
        Require(SendAll(client.value, std::span<char const>(payload.data(), payload.size())),
            "send public request");
        std::string echoed(payload.size(), '\0');
        std::stop_source stop;
        Require(ReceiveExact(client.value, std::span<char>(echoed.data(), echoed.size()), stop.get_token()),
            "receive relayed response");
        Require(echoed == payload, "bidirectional TCP relay payload");
        Require(WaitUntil([&] { return service.Snapshot().activeRelays == 1; }),
            "publish active relay count");
    }
}

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
    struct CleanupWinsock { ~CleanupWinsock() { WSACleanup(); } } cleanup;

    try
    {
        FixtureServers fixture;
        OpenNet::Core::NatMap::TcpMappingService service({ 100ms, 100ms, 5s });
        OpenNet::Core::NatMap::TcpMappingOptions options;
        options.localAddress = L"127.0.0.1";
        options.targetPort = fixture.TargetPort();
        options.keepaliveHost = L"127.0.0.1";
        options.keepalivePort = fixture.KeeperPort();
        options.stunHost = L"127.0.0.2; 127.0.0.1";
        options.stunPort = fixture.StunPort();
        options.maximumRelays = 2;
        auto invalidOptions = options;
        invalidOptions.stunHost = L"127.0.0.2;;127.0.0.1";
        Require(!service.Start(invalidOptions), "reject malformed TCP STUN fallback list");
        Require(service.Start(options), "start production TCP mapping service");
        Require(WaitUntil([&]
        {
            auto const snapshot = service.Snapshot();
            return snapshot.running && snapshot.generation == 1 && snapshot.localPort != 0 &&
                snapshot.publicAddress == L"203.0.113.9" && snapshot.publicPort == 45678;
        }), "publish first TCP mapping observation");
        Require(fixture.SilentStunSourcePort() != 0 &&
            fixture.SilentStunSourcePort() == fixture.ResponsiveStunSourcePort() &&
            fixture.ResponsiveStunSourcePort() == service.Snapshot().localPort,
            "TCP STUN fallback candidates reuse the keeper-selected source port");
        ExerciseRelay(service);

        service.RequestNetworkRecovery();
        Require(WaitUntil([&]
        {
            auto const snapshot = service.Snapshot();
            return snapshot.running && snapshot.generation >= 2 && snapshot.localPort != 0;
        }), "rebuild TCP mapping after network change");
        ExerciseRelay(service);

        service.Stop();
        auto const stopped = service.Snapshot();
        Require(!stopped.running && !stopped.starting && stopped.localPort == 0 &&
            stopped.publicPort == 0 && stopped.activeRelays == 0, "clear stopped TCP mapping state");
        Require(fixture.KeeperRequests() >= 2 && fixture.SilentStunConnections() >= 1 &&
            fixture.StunRequests() >= 2, "exercise keeper and TCP STUN fallback across recovery");
        Require(!fixture.Failed(), "fixture server completed without socket errors");
        std::cout << "Production TCP mapping, STUN fallback, bidirectional relay, recovery and stop passed\n";
    }
    catch (std::exception const& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
