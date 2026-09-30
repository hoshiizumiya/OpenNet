#include "Core/NatMap/UdpMappingService.h"
#include "Core/NatMap/StunProtocol.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <compare>
#include <map>
#include <stdexcept>
#include <utility>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bcrypt.lib")

namespace OpenNet::Core::NatMap
{
    namespace
    {
        struct Socket
        {
            SOCKET value{ INVALID_SOCKET };
            ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
            Socket() = default;
            explicit Socket(SOCKET socket) : value(socket) {}
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

        struct PeerKey
        {
            std::uint32_t address{};
            std::uint16_t port{};
            auto operator<=>(PeerKey const&) const = default;
        };

        struct Peer
        {
            Socket socket;
            sockaddr_in address{};
            std::chrono::steady_clock::time_point lastSeen;
        };

        bool ResolveStun(std::wstring const& host, std::uint16_t port, std::stop_token stop, sockaddr_in& address)
        {
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            if (InetPtonW(AF_INET, host.c_str(), &address.sin_addr) == 1) return !stop.stop_requested();

            ADDRINFOEXW hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;
            ADDRINFOEXW* result{};
            auto const service = std::to_wstring(port);
            OVERLAPPED operation{};
            operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!operation.hEvent) return false;
            HANDLE cancellation{};
            timeval timeout{ 5, 0 };
            auto status = GetAddrInfoExW(host.c_str(), service.c_str(), NS_DNS, nullptr, &hints, &result,
                                         &timeout, &operation, nullptr, &cancellation);
            if (status == WSA_IO_PENDING)
            {
                while (WaitForSingleObject(operation.hEvent, 50) == WAIT_TIMEOUT)
                {
                    if (stop.stop_requested())
                    {
                        GetAddrInfoExCancel(&cancellation);
                        // Cancellation is asynchronous. Drain completion before releasing
                        // OVERLAPPED/result storage or calling WSACleanup.
                        WaitForSingleObject(operation.hEvent, INFINITE);
                        break;
                    }
                }
                status = GetAddrInfoExOverlappedResult(&operation);
            }
            bool const found = status == 0 && result && !stop.stop_requested();
            if (found) address = *reinterpret_cast<sockaddr_in const*>(result->ai_addr);
            if (result) FreeAddrInfoExW(result);
            CloseHandle(operation.hEvent);
            return found;
        }

        bool MakeNonblocking(SOCKET socket)
        {
            u_long enabled = 1;
            return ioctlsocket(socket, FIONBIO, &enabled) == 0;
        }
    }

    UdpMappingService::UdpMappingService(UdpMappingTimings timings) : m_timings(timings)
    {
        if (timings.keepalive.count() <= 0 || timings.responseExpiry < timings.keepalive || timings.peerExpiry.count() <= 0)
            throw std::invalid_argument("Invalid UDP mapping timers");
    }

    UdpMappingService::~UdpMappingService() { Stop(); }

    UdpMappingService& SharedUdpMappingService()
    {
        static UdpMappingService service;
        return service;
    }

    bool UdpMappingService::Start(std::uint16_t localPort, std::uint16_t targetPort,
                                  std::wstring stunHost, std::uint16_t stunPort)
    {
        if (!targetPort || !stunPort || stunHost.empty() || (localPort != 0 && localPort == targetPort)) return false;
        std::lock_guard lifecycle(m_lifecycleMutex);
        StopWorker();
        {
            std::lock_guard lock(m_mutex);
            m_snapshot = {};
            m_snapshot.targetPort = targetPort;
            m_snapshot.starting = true;
        }
        m_worker = std::jthread([this, localPort, targetPort, stunHost = std::move(stunHost), stunPort](std::stop_token stop)
        {
            try
            {
                Run(stop, localPort, targetPort, stunHost, stunPort);
            }
            catch (...)
            {
                std::lock_guard lock(m_mutex);
                m_snapshot.running = false;
                m_snapshot.starting = false;
                m_snapshot.publicAddress.clear();
                m_snapshot.publicPort = 0;
                m_snapshot.error = L"UDP mapping worker failed.";
            }
        });
        return true;
    }

    void UdpMappingService::Stop()
    {
        std::lock_guard lifecycle(m_lifecycleMutex);
        StopWorker();
    }

    void UdpMappingService::StopWorker()
    {
        if (m_worker.joinable())
        {
            m_worker.request_stop();
            m_worker.join();
        }
        std::lock_guard lock(m_mutex);
        m_snapshot.running = false;
        m_snapshot.starting = false;
        m_snapshot.publicAddress.clear();
        m_snapshot.publicPort = 0;
        m_snapshot.error.clear();
    }

    MappingSnapshot UdpMappingService::Snapshot() const
    {
        std::lock_guard lock(m_mutex);
        return m_snapshot;
    }

    void UdpMappingService::Run(std::stop_token stop, std::uint16_t localPort, std::uint16_t targetPort,
                                std::wstring stunHost, std::uint16_t stunPort)
    {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            std::lock_guard lock(m_mutex);
            m_snapshot.error = L"Winsock initialization failed";
            m_snapshot.starting = false;
            return;
        }
        struct CleanupWinsock { ~CleanupWinsock() { WSACleanup(); } } cleanup;
        auto finish = [this](std::wstring message)
        {
            std::lock_guard lock(m_mutex);
            m_snapshot.running = false;
            m_snapshot.starting = false;
            m_snapshot.publicAddress.clear();
            m_snapshot.publicPort = 0;
            if (!message.empty()) m_snapshot.error = std::move(message);
        };

        Socket outside(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
        sockaddr_in bound{};
        bound.sin_family = AF_INET;
        bound.sin_addr.s_addr = htonl(INADDR_ANY);
        bound.sin_port = htons(localPort);
        // A dedicated port is essential: routing arbitrary packets to two listeners
        // sharing the same UDP port cannot reliably preserve torrent traffic.
        BOOL exclusive = TRUE;
        if (outside.value == INVALID_SOCKET ||
            setsockopt(outside.value, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char*>(&exclusive), sizeof(exclusive)) != 0 ||
            bind(outside.value, reinterpret_cast<sockaddr*>(&bound), sizeof(bound)) != 0 || !MakeNonblocking(outside.value))
        {
            finish(L"Cannot bind mapping port. Choose a port different from the torrent listener.");
            return;
        }
        int boundSize = sizeof(bound);
        if (getsockname(outside.value, reinterpret_cast<sockaddr*>(&bound), &boundSize) != 0)
        {
            finish(L"Cannot query the bound UDP port.");
            return;
        }
        if (ntohs(bound.sin_port) == targetPort)
        {
            finish(L"The allocated mapping port equals the target port. Choose a fixed, separate port.");
            return;
        }
        sockaddr_in stun{};
        if (!ResolveStun(stunHost, stunPort, stop, stun))
        {
            finish(stop.stop_requested() ? std::wstring{} : L"Cannot resolve the STUN server.");
            return;
        }
        {
            std::lock_guard lock(m_mutex);
            m_snapshot.running = true;
            m_snapshot.starting = false;
            m_snapshot.localPort = ntohs(bound.sin_port);
            m_snapshot.error.clear();
        }

        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        target.sin_port = htons(targetPort);
        std::map<PeerKey, Peer> peers;
        Stun::TransactionId transaction{};
        auto nextStun = std::chrono::steady_clock::time_point::min();
        auto lastResponse = std::chrono::steady_clock::now();
        std::array<char, 65536> buffer{};

        while (!stop.stop_requested())
        {
            auto const now = std::chrono::steady_clock::now();
            if (now >= nextStun)
            {
                if (BCryptGenRandom(nullptr, transaction.data(), static_cast<ULONG>(transaction.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
                {
                    finish(L"Cannot generate a STUN transaction ID.");
                    return;
                }
                auto const request = Stun::BindingRequest(transaction);
                sendto(outside.value, reinterpret_cast<char const*>(request.data()), static_cast<int>(request.size()), 0,
                       reinterpret_cast<sockaddr*>(&stun), sizeof(stun));
                nextStun = now + m_timings.keepalive;
            }
            for (auto it = peers.begin(); it != peers.end();)
            {
                if (now - it->second.lastSeen > m_timings.peerExpiry) it = peers.erase(it);
                else ++it;
            }

            fd_set readers;
            FD_ZERO(&readers);
            FD_SET(outside.value, &readers);
            for (auto const& [key, peer] : peers) FD_SET(peer.socket.value, &readers);
            timeval timeout{ 0, 250000 };
            if (select(0, &readers, nullptr, nullptr, &timeout) == SOCKET_ERROR)
            {
                finish(L"Mapping socket stopped unexpectedly.");
                return;
            }
            if (FD_ISSET(outside.value, &readers))
            {
                auto processDatagram = [&]
                {
                    sockaddr_in remote{};
                    int size = sizeof(remote);
                    auto const count = recvfrom(outside.value, buffer.data(), static_cast<int>(buffer.size()), 0,
                                                reinterpret_cast<sockaddr*>(&remote), &size);
                    if (count < 0) return;
                    if (remote.sin_addr.s_addr == stun.sin_addr.s_addr && remote.sin_port == stun.sin_port)
                    {
                        auto const packet = std::span<std::uint8_t const>(reinterpret_cast<std::uint8_t const*>(buffer.data()), static_cast<std::size_t>(count));
                        auto const mapped = Stun::ParseBindingResponse(packet, transaction);
                        wchar_t publicIp[INET_ADDRSTRLEN]{};
                        if (mapped && !mapped->ipv6 && InetNtopW(AF_INET, mapped->address.data(), publicIp, INET_ADDRSTRLEN))
                        {
                            lastResponse = std::chrono::steady_clock::now();
                            std::lock_guard lock(m_mutex);
                            m_snapshot.publicAddress = publicIp;
                            m_snapshot.publicPort = mapped->port;
                            m_snapshot.error.clear();
                        }
                        return;
                    }
                    if (peers.size() >= 32 && !peers.contains({ remote.sin_addr.s_addr, remote.sin_port })) return;
                    PeerKey key{ remote.sin_addr.s_addr, remote.sin_port };
                    auto it = peers.find(key);
                    if (it == peers.end())
                    {
                        Socket inside(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
                        if (inside.value == INVALID_SOCKET || connect(inside.value, reinterpret_cast<sockaddr*>(&target), sizeof(target)) != 0 ||
                            !MakeNonblocking(inside.value)) return;
                        it = peers.emplace(key, Peer{ std::move(inside), remote, now }).first;
                    }
                    it->second.lastSeen = now;
                    send(it->second.socket.value, buffer.data(), count, 0);
                };
                processDatagram();
            }
            for (auto& [key, peer] : peers)
            {
                if (!FD_ISSET(peer.socket.value, &readers)) continue;
                auto const count = recv(peer.socket.value, buffer.data(), static_cast<int>(buffer.size()), 0);
                if (count >= 0)
                {
                    sendto(outside.value, buffer.data(), count, 0,
                           reinterpret_cast<sockaddr*>(&peer.address), sizeof(peer.address));
                    peer.lastSeen = std::chrono::steady_clock::now();
                }
            }
            if (std::chrono::steady_clock::now() - lastResponse > m_timings.responseExpiry)
            {
                std::lock_guard lock(m_mutex);
                m_snapshot.publicAddress.clear();
                m_snapshot.publicPort = 0;
                m_snapshot.error = L"STUN keepalive timed out.";
            }
        }
        finish({});
    }
}
