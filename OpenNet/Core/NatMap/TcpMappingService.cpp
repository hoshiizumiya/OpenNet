#include "Core/NatMap/TcpMappingService.h"
#include "Core/NatMap/StunProtocol.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Bcrypt.lib")

namespace OpenNet::Core::NatMap
{
    namespace
    {
        constexpr std::size_t MaximumPendingBytes = 64 * 1024;

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

        struct Relay
        {
            Socket outside;
            Socket inside;
            std::vector<char> toOutside;
            std::vector<char> toInside;
            bool outsideReadable{ true };
            bool insideReadable{ true };
            std::chrono::steady_clock::time_point lastSeen;
        };

        bool IsWouldBlock()
        {
            int const error = WSAGetLastError();
            return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEALREADY;
        }

        bool MakeNonblocking(SOCKET socket)
        {
            u_long enabled = 1;
            return ioctlsocket(socket, FIONBIO, &enabled) == 0;
        }

        bool EnableReuse(SOCKET socket)
        {
            BOOL const enabled = TRUE;
            return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
                reinterpret_cast<char const*>(&enabled), sizeof(enabled)) == 0;
        }

        bool Resolve(std::wstring const& host, std::uint16_t port, std::stop_token stop,
                     std::atomic_bool const& recovery, sockaddr_in& address)
        {
            address = {};
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            if (InetPtonW(AF_INET, host.c_str(), &address.sin_addr) == 1)
                return !stop.stop_requested() && !recovery.load();

            ADDRINFOEXW hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            ADDRINFOEXW* result{};
            std::wstring const service = std::to_wstring(port);
            OVERLAPPED operation{};
            operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!operation.hEvent) return false;
            HANDLE cancellation{};
            timeval timeout{ 5, 0 };
            int status = GetAddrInfoExW(host.c_str(), service.c_str(), NS_DNS, nullptr, &hints,
                &result, &timeout, &operation, nullptr, &cancellation);
            if (status == WSA_IO_PENDING)
            {
                while (WaitForSingleObject(operation.hEvent, 50) == WAIT_TIMEOUT)
                {
                    if (stop.stop_requested() || recovery.load())
                    {
                        GetAddrInfoExCancel(&cancellation);
                        WaitForSingleObject(operation.hEvent, INFINITE);
                        break;
                    }
                }
                status = GetAddrInfoExOverlappedResult(&operation);
            }
            bool const found = status == 0 && result && !stop.stop_requested() && !recovery.load();
            if (found) address = *reinterpret_cast<sockaddr_in const*>(result->ai_addr);
            if (result) FreeAddrInfoExW(result);
            CloseHandle(operation.hEvent);
            return found;
        }

        bool WaitSocket(SOCKET socket, bool write, std::stop_token stop,
                        std::atomic_bool const& recovery, std::chrono::milliseconds limit)
        {
            std::chrono::steady_clock::time_point const until = std::chrono::steady_clock::now() + limit;
            while (!stop.stop_requested() && !recovery.load() && std::chrono::steady_clock::now() < until)
            {
                fd_set readers;
                fd_set writers;
                FD_ZERO(&readers);
                FD_ZERO(&writers);
                if (write) FD_SET(socket, &writers);
                else FD_SET(socket, &readers);
                timeval timeout{ 0, 100000 };
                int const ready = select(0, &readers, &writers, nullptr, &timeout);
                if (ready == SOCKET_ERROR) return false;
                if (ready == 1) return true;
            }
            return false;
        }

        bool BindReusable(Socket& socket, sockaddr_in const& local)
        {
            socket = Socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
            return socket.value != INVALID_SOCKET && EnableReuse(socket.value) &&
                bind(socket.value, reinterpret_cast<sockaddr const*>(&local), sizeof(local)) == 0 &&
                MakeNonblocking(socket.value);
        }

        bool ConnectBound(Socket& socket, sockaddr_in const& local, sockaddr_in const& remote,
                          std::stop_token stop, std::atomic_bool const& recovery)
        {
            if (!BindReusable(socket, local)) return false;
            int const result = connect(socket.value, reinterpret_cast<sockaddr const*>(&remote), sizeof(remote));
            if (result == 0) return true;
            if (!IsWouldBlock() || !WaitSocket(socket.value, true, stop, recovery, std::chrono::seconds(5)))
                return false;
            int error{};
            int size = sizeof(error);
            return getsockopt(socket.value, SOL_SOCKET, SO_ERROR,
                reinterpret_cast<char*>(&error), &size) == 0 && error == 0;
        }

        bool ConnectTarget(Socket& socket, std::uint16_t port, std::stop_token stop,
                           std::atomic_bool const& recovery)
        {
            socket = Socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
            if (socket.value == INVALID_SOCKET || !MakeNonblocking(socket.value)) return false;
            sockaddr_in target{};
            target.sin_family = AF_INET;
            target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            target.sin_port = htons(port);
            int const result = connect(socket.value, reinterpret_cast<sockaddr*>(&target), sizeof(target));
            if (result == 0) return true;
            if (!IsWouldBlock() || !WaitSocket(socket.value, true, stop, recovery, std::chrono::seconds(3)))
                return false;
            int error{};
            int size = sizeof(error);
            return getsockopt(socket.value, SOL_SOCKET, SO_ERROR,
                reinterpret_cast<char*>(&error), &size) == 0 && error == 0;
        }

        bool SendAll(SOCKET socket, std::span<char const> bytes, std::stop_token stop,
                     std::atomic_bool const& recovery)
        {
            std::size_t offset{};
            while (offset < bytes.size() && !stop.stop_requested() && !recovery.load())
            {
                int const count = send(socket, bytes.data() + offset,
                    static_cast<int>(bytes.size() - offset), 0);
                if (count > 0)
                {
                    offset += static_cast<std::size_t>(count);
                    continue;
                }
                if (!IsWouldBlock() || !WaitSocket(socket, true, stop, recovery, std::chrono::seconds(3)))
                    return false;
            }
            return offset == bytes.size();
        }

        bool ReceiveExact(SOCKET socket, std::span<char> bytes, std::stop_token stop,
                          std::atomic_bool const& recovery)
        {
            std::size_t offset{};
            while (offset < bytes.size() && !stop.stop_requested() && !recovery.load())
            {
                int const count = recv(socket, bytes.data() + offset,
                    static_cast<int>(bytes.size() - offset), 0);
                if (count > 0)
                {
                    offset += static_cast<std::size_t>(count);
                    continue;
                }
                if (count == 0 || !IsWouldBlock() ||
                    !WaitSocket(socket, false, stop, recovery, std::chrono::seconds(3))) return false;
            }
            return offset == bytes.size();
        }

        std::string Utf8(std::wstring const& value)
        {
            if (value.empty()) return {};
            int const size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                nullptr, 0, nullptr, nullptr);
            if (size <= 0) return {};
            std::string result(static_cast<std::size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                result.data(), size, nullptr, nullptr);
            return result;
        }
    }

    TcpMappingService::TcpMappingService(TcpMappingTimings timings) : m_timings(timings)
    {
        if (timings.keepalive.count() <= 0 || timings.retry.count() <= 0 || timings.relayExpiry.count() <= 0)
            throw std::invalid_argument("Invalid TCP mapping timers");
    }

    TcpMappingService::~TcpMappingService() { Stop(); }

    TcpMappingService& SharedTcpMappingService()
    {
        static TcpMappingService service;
        return service;
    }

    bool TcpMappingService::Start(TcpMappingOptions options)
    {
        if (options.localAddress.empty() || !options.targetPort || options.keepaliveHost.empty() ||
            !options.keepalivePort || options.stunHost.empty() || !options.stunPort ||
            options.maximumRelays == 0 || options.maximumRelays > 30) return false;
        std::lock_guard lifecycle(m_lifecycleMutex);
        StopWorker();
        m_networkRecoveryRequested.store(false);
        {
            std::lock_guard lock(m_mutex);
            m_snapshot = {};
            m_snapshot.starting = true;
            m_snapshot.localAddress = options.localAddress;
            m_snapshot.localPort = options.localPort;
            m_snapshot.targetPort = options.targetPort;
        }
        m_worker = std::jthread([this, options = std::move(options)](std::stop_token stop)
        {
            try
            {
                Run(stop, options);
            }
            catch (...)
            {
                std::lock_guard lock(m_mutex);
                m_snapshot.running = false;
                m_snapshot.starting = false;
                m_snapshot.publicAddress.clear();
                m_snapshot.publicPort = 0;
                m_snapshot.activeRelays = 0;
                m_snapshot.error = L"TCP mapping worker failed.";
            }
        });
        return true;
    }

    void TcpMappingService::RequestNetworkRecovery() noexcept
    {
        m_networkRecoveryRequested.store(true);
    }

    void TcpMappingService::Stop()
    {
        std::lock_guard lifecycle(m_lifecycleMutex);
        StopWorker();
    }

    void TcpMappingService::StopWorker()
    {
        if (m_worker.joinable())
        {
            m_worker.request_stop();
            m_worker.join();
        }
        m_networkRecoveryRequested.store(false);
        std::lock_guard lock(m_mutex);
        m_snapshot = {};
    }

    TcpMappingSnapshot TcpMappingService::Snapshot() const
    {
        std::lock_guard lock(m_mutex);
        return m_snapshot;
    }

    void TcpMappingService::Run(std::stop_token stop, TcpMappingOptions options)
    {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            std::lock_guard lock(m_mutex);
            m_snapshot.starting = false;
            m_snapshot.error = L"Winsock initialization failed.";
            return;
        }
        struct CleanupWinsock { ~CleanupWinsock() { WSACleanup(); } } cleanup;

        while (!stop.stop_requested())
        {
            m_networkRecoveryRequested.store(false);
            std::wstring error;
            bool const retry = RunSession(stop, options, error);
            if (stop.stop_requested()) break;
            {
                std::lock_guard lock(m_mutex);
                m_snapshot.running = false;
                m_snapshot.starting = retry;
                m_snapshot.publicAddress.clear();
                m_snapshot.publicPort = 0;
                m_snapshot.activeRelays = 0;
                m_snapshot.error = std::move(error);
            }
            if (!retry) break;
            std::chrono::steady_clock::time_point const until =
                std::chrono::steady_clock::now() + m_timings.retry;
            while (!stop.stop_requested() && std::chrono::steady_clock::now() < until)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::lock_guard lock(m_mutex);
        m_snapshot.running = false;
        m_snapshot.starting = false;
        m_snapshot.publicAddress.clear();
        m_snapshot.publicPort = 0;
        m_snapshot.activeRelays = 0;
    }

    bool TcpMappingService::RunSession(std::stop_token stop, TcpMappingOptions const& options,
                                       std::wstring& error)
    {
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(options.localPort);
        if (InetPtonW(AF_INET, options.localAddress.c_str(), &local.sin_addr) != 1 ||
            local.sin_addr.s_addr == htonl(INADDR_ANY))
        {
            error = L"TCP mapping requires a specific local IPv4 address.";
            return false;
        }

        sockaddr_in keeperAddress{};
        sockaddr_in stunAddress{};
        if (!Resolve(options.keepaliveHost, options.keepalivePort, stop,
                m_networkRecoveryRequested, keeperAddress) ||
            !Resolve(options.stunHost, options.stunPort, stop,
                m_networkRecoveryRequested, stunAddress))
        {
            error = m_networkRecoveryRequested.load() ? L"Network changed; rebuilding TCP mapping."
                                                      : L"Cannot resolve TCP mapping endpoints.";
            return !stop.stop_requested();
        }

        Socket keeper;
        if (!ConnectBound(keeper, local, keeperAddress, stop, m_networkRecoveryRequested))
        {
            error = L"Cannot establish the TCP keepalive connection.";
            return !stop.stop_requested();
        }
        int localSize = sizeof(local);
        if (getsockname(keeper.value, reinterpret_cast<sockaddr*>(&local), &localSize) != 0)
        {
            error = L"Cannot query the TCP mapping source port.";
            return false;
        }

        Socket stun;
        if (!ConnectBound(stun, local, stunAddress, stop, m_networkRecoveryRequested))
        {
            error = L"Cannot establish TCP STUN from the mapping port.";
            return !stop.stop_requested();
        }
        Stun::TransactionId transaction{};
        if (BCryptGenRandom(nullptr, transaction.data(), static_cast<ULONG>(transaction.size()),
                BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        {
            error = L"Cannot generate a TCP STUN transaction ID.";
            return false;
        }
        std::array<std::uint8_t, 20> const request = Stun::BindingRequest(transaction);
        if (!SendAll(stun.value, std::span<char const>(reinterpret_cast<char const*>(request.data()), request.size()),
                stop, m_networkRecoveryRequested))
        {
            error = L"TCP STUN request failed.";
            return !stop.stop_requested();
        }
        std::array<char, 20> header{};
        if (!ReceiveExact(stun.value, std::span<char>(header), stop, m_networkRecoveryRequested))
        {
            error = L"TCP STUN response header failed.";
            return !stop.stop_requested();
        }
        std::uint16_t const bodyLength = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(static_cast<unsigned char>(header[2])) << 8) |
            static_cast<unsigned char>(header[3]));
        std::size_t const bodySize = bodyLength;
        if (bodySize == 0 || bodySize > 2048 || bodySize % 4 != 0)
        {
            error = L"TCP STUN response length is invalid.";
            return false;
        }
        std::vector<char> response(20 + bodySize);
        std::copy(header.begin(), header.end(), response.begin());
        if (!ReceiveExact(stun.value, std::span<char>(response.data() + 20, bodySize),
                stop, m_networkRecoveryRequested))
        {
            error = L"TCP STUN response body failed.";
            return !stop.stop_requested();
        }
        std::optional<Stun::MappedEndpoint> const mapped = Stun::ParseBindingResponse(
            std::span<std::uint8_t const>(reinterpret_cast<std::uint8_t const*>(response.data()), response.size()),
            transaction);
        wchar_t publicIp[INET_ADDRSTRLEN]{};
        if (!mapped || mapped->ipv6 || !InetNtopW(AF_INET, mapped->address.data(), publicIp, INET_ADDRSTRLEN))
        {
            error = L"TCP STUN response has no mapped IPv4 endpoint.";
            return false;
        }

        Socket listener;
        if (!BindReusable(listener, local) || listen(listener.value, SOMAXCONN) != 0)
        {
            error = L"Cannot listen on the shared TCP mapping port.";
            return false;
        }

        {
            std::lock_guard lock(m_mutex);
            m_snapshot.running = true;
            m_snapshot.starting = false;
            m_snapshot.localPort = ntohs(local.sin_port);
            m_snapshot.publicAddress = publicIp;
            m_snapshot.publicPort = mapped->port;
            ++m_snapshot.generation;
            m_snapshot.error.clear();
        }

        std::string const keeperHost = Utf8(options.keepaliveHost);
        std::string const keepalive = "HEAD / HTTP/1.1\r\nHost: " + keeperHost +
            "\r\nConnection: keep-alive\r\n\r\n";
        std::chrono::steady_clock::time_point nextKeepalive = std::chrono::steady_clock::time_point::min();
        std::vector<Relay> relays;
        std::array<char, 16384> buffer{};

        while (!stop.stop_requested() && !m_networkRecoveryRequested.load())
        {
            std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
            if (now >= nextKeepalive)
            {
                if (!SendAll(keeper.value, std::span<char const>(keepalive.data(), keepalive.size()),
                        stop, m_networkRecoveryRequested))
                {
                    error = L"TCP keepalive connection closed; rebuilding mapping.";
                    return !stop.stop_requested();
                }
                nextKeepalive = now + m_timings.keepalive;
            }

            for (auto iterator = relays.begin(); iterator != relays.end();)
            {
                if (now - iterator->lastSeen > m_timings.relayExpiry) iterator = relays.erase(iterator);
                else ++iterator;
            }

            fd_set readers;
            fd_set writers;
            FD_ZERO(&readers);
            FD_ZERO(&writers);
            FD_SET(listener.value, &readers);
            FD_SET(keeper.value, &readers);
            for (Relay const& relay : relays)
            {
                if (relay.outsideReadable && relay.toInside.size() < MaximumPendingBytes)
                    FD_SET(relay.outside.value, &readers);
                if (relay.insideReadable && relay.toOutside.size() < MaximumPendingBytes)
                    FD_SET(relay.inside.value, &readers);
                if (!relay.toOutside.empty()) FD_SET(relay.outside.value, &writers);
                if (!relay.toInside.empty()) FD_SET(relay.inside.value, &writers);
            }
            timeval timeout{ 0, 100000 };
            if (select(0, &readers, &writers, nullptr, &timeout) == SOCKET_ERROR)
            {
                error = L"TCP mapping socket selection failed.";
                return true;
            }

            if (FD_ISSET(keeper.value, &readers))
            {
                int const count = recv(keeper.value, buffer.data(), static_cast<int>(buffer.size()), 0);
                if (count == 0 || (count < 0 && !IsWouldBlock()))
                {
                    error = L"TCP keepalive peer closed; rebuilding mapping.";
                    return true;
                }
            }

            if (FD_ISSET(listener.value, &readers))
            {
                Socket outside(accept(listener.value, nullptr, nullptr));
                if (outside.value != INVALID_SOCKET)
                {
                    if (relays.size() < options.maximumRelays && MakeNonblocking(outside.value))
                    {
                        Socket inside;
                        if (ConnectTarget(inside, options.targetPort, stop, m_networkRecoveryRequested))
                            relays.push_back({ std::move(outside), std::move(inside), {}, {}, true, true, now });
                    }
                }
            }

            for (Relay& relay : relays)
            {
                auto receive = [&](Socket const& source, std::vector<char>& pending, bool& readable)
                {
                    if (!readable || !FD_ISSET(source.value, &readers)) return;
                    std::size_t const available = MaximumPendingBytes - pending.size();
                    int const count = recv(source.value, buffer.data(),
                        static_cast<int>(std::min(buffer.size(), available)), 0);
                    if (count > 0)
                    {
                        pending.insert(pending.end(), buffer.begin(), buffer.begin() + count);
                        relay.lastSeen = std::chrono::steady_clock::now();
                    }
                    else if (count == 0 || !IsWouldBlock()) readable = false;
                };
                auto sendPending = [&](Socket const& destination, std::vector<char>& pending)
                {
                    if (pending.empty() || !FD_ISSET(destination.value, &writers)) return;
                    int const count = send(destination.value, pending.data(), static_cast<int>(pending.size()), 0);
                    if (count > 0)
                    {
                        pending.erase(pending.begin(), pending.begin() + count);
                        relay.lastSeen = std::chrono::steady_clock::now();
                    }
                    else if (!IsWouldBlock())
                    {
                        relay.outsideReadable = false;
                        relay.insideReadable = false;
                        relay.toOutside.clear();
                        relay.toInside.clear();
                    }
                };
                receive(relay.outside, relay.toInside, relay.outsideReadable);
                receive(relay.inside, relay.toOutside, relay.insideReadable);
                sendPending(relay.outside, relay.toOutside);
                sendPending(relay.inside, relay.toInside);
            }
            std::erase_if(relays, [](Relay const& relay)
            {
                return !relay.outsideReadable && !relay.insideReadable &&
                    relay.toOutside.empty() && relay.toInside.empty();
            });
            {
                std::lock_guard lock(m_mutex);
                m_snapshot.activeRelays = relays.size();
            }
        }

        error = m_networkRecoveryRequested.load() ? L"Network changed; rebuilding TCP mapping." : std::wstring{};
        return m_networkRecoveryRequested.load() && !stop.stop_requested();
    }
}
