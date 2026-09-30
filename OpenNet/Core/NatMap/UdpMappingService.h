#pragma once

#include <cstdint>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

namespace OpenNet::Core::NatMap
{
    // Windows-native implementation of natmap's UDP bind/keepalive/forward path.
    // The external socket owns a dedicated port; the target is a separate local service.
    struct MappingSnapshot
    {
        bool running{};
        bool starting{};
        std::uint16_t localPort{};
        std::uint16_t targetPort{};
        std::uint16_t publicPort{};
        std::wstring publicAddress;
        std::wstring error;
    };

    struct UdpMappingTimings
    {
        std::chrono::milliseconds keepalive{ 25000 };
        std::chrono::milliseconds responseExpiry{ 70000 };
        std::chrono::milliseconds peerExpiry{ 120000 };
    };

    class UdpMappingService final
    {
    public:
        explicit UdpMappingService(UdpMappingTimings timings = {});
        ~UdpMappingService();
        UdpMappingService(UdpMappingService const&) = delete;
        UdpMappingService& operator=(UdpMappingService const&) = delete;

        bool Start(std::uint16_t localPort, std::uint16_t targetPort, std::wstring stunHost, std::uint16_t stunPort);
        void Stop();
        MappingSnapshot Snapshot() const;

    private:
        void StopWorker();
        void Run(std::stop_token stop, std::uint16_t localPort, std::uint16_t targetPort,
                 std::wstring stunHost, std::uint16_t stunPort);

        mutable std::mutex m_mutex;
        std::mutex m_lifecycleMutex;
        UdpMappingTimings m_timings;
        std::jthread m_worker;
        MappingSnapshot m_snapshot;
    };

    UdpMappingService& SharedUdpMappingService();
}
