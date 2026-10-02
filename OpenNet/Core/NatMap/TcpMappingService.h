#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace OpenNet::Core::NatMap
{
    struct TcpMappingOptions
    {
        // TCP source-port reuse on Windows must use one specific interface.
        // Wildcard addresses are rejected.
        std::wstring localAddress;
        std::uint16_t localPort{};
        std::uint16_t targetPort{};
        std::wstring keepaliveHost;
        std::uint16_t keepalivePort{ 80 };
        std::wstring stunHost;
        std::uint16_t stunPort{ 3478 };
        // Two sockets per relay plus the listener and keeper must fit the
        // Winsock FD_SETSIZE (64) select set.
        std::size_t maximumRelays{ 8 };
    };

    struct TcpMappingSnapshot
    {
        bool running{};
        bool starting{};
        std::wstring localAddress;
        std::uint16_t localPort{};
        std::uint16_t targetPort{};
        std::wstring publicAddress;
        std::uint16_t publicPort{};
        std::size_t activeRelays{};
        std::uint64_t generation{};
        std::wstring error;
    };

    struct TcpMappingTimings
    {
        std::chrono::milliseconds keepalive{ 30000 };
        std::chrono::milliseconds retry{ 5000 };
        std::chrono::milliseconds relayExpiry{ 300000 };
    };

    class TcpMappingService final
    {
    public:
        explicit TcpMappingService(TcpMappingTimings timings = {});
        ~TcpMappingService();
        TcpMappingService(TcpMappingService const&) = delete;
        TcpMappingService& operator=(TcpMappingService const&) = delete;

        bool Start(TcpMappingOptions options);
        void RequestNetworkRecovery() noexcept;
        void Stop();
        [[nodiscard]] TcpMappingSnapshot Snapshot() const;

    private:
        void StopWorker();
        void Run(std::stop_token stop, TcpMappingOptions options);
        bool RunSession(std::stop_token stop, TcpMappingOptions const& options,
                        std::wstring& error);

        mutable std::mutex m_mutex;
        std::mutex m_lifecycleMutex;
        TcpMappingTimings m_timings;
        std::jthread m_worker;
        std::atomic_bool m_networkRecoveryRequested{};
        TcpMappingSnapshot m_snapshot;
    };

    TcpMappingService& SharedTcpMappingService();
}
