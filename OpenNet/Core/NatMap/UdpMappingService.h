#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace OpenNet::Core::NatMap
{
    // Windows-native implementation of natmap's UDP bind/keepalive/forward path.
    // The external socket owns a dedicated port; the target is a separate local service.
    struct MappingSnapshot
    {
        bool running{};
        bool starting{};
        bool followsTorrentTarget{};
        bool targetAvailable{};
        std::uint16_t localPort{};
        std::uint16_t targetPort{};
        std::uint16_t publicPort{};
        std::wstring publicAddress;
        std::uint64_t observationGeneration{};
        // STUN only observes an endpoint toward one server. These fields are
        // set only after an independent traversal server probes that exact
        // observation from outside the local network.
        bool externalProbeCompleted{};
        bool externallyReachable{};
        std::wstring targetError;
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

        bool Start(std::uint16_t localPort, std::uint16_t targetPort, std::wstring stunHost,
                   std::uint16_t stunPort, bool followTorrentTarget = false);
        // Update the relay destination without rebinding the public mapping
        // socket. A zero port pauses forwarding while the torrent listener is
        // stopped; a later nonzero port resumes it and discards stale peers.
        void RequestAutomaticTargetPort(std::uint16_t targetPort) noexcept;
        // Re-resolve the STUN route and discard relay peers after an adapter,
        // address, or connectivity change. This is non-blocking and safe to
        // call from Windows.Networking.Connectivity event callbacks.
        void RequestNetworkRecovery() noexcept;
        // Apply an asynchronous external-probe result only if it still refers
        // to the current bound port and STUN observation.
        bool RecordExternalProbe(std::uint16_t localPort, std::wstring const& publicAddress,
                                 std::uint16_t publicPort, std::uint64_t observationGeneration,
                                 bool completed, bool reachable) noexcept;
        void Stop();
        MappingSnapshot Snapshot() const;

    private:
        void StopWorker();
        void Run(std::stop_token stop, std::uint16_t localPort, std::uint16_t targetPort,
                 std::vector<std::wstring> stunHosts, std::uint16_t stunPort, bool utpOnly);

        mutable std::mutex m_mutex;
        std::mutex m_lifecycleMutex;
        UdpMappingTimings m_timings;
        std::jthread m_worker;
        std::atomic_bool m_networkRecoveryRequested{};
        std::atomic_bool m_targetPortUpdateRequested{};
        std::atomic_bool m_followTorrentTarget{};
        std::atomic_uint16_t m_requestedTargetPort{};
        MappingSnapshot m_snapshot;
    };

    UdpMappingService& SharedUdpMappingService();
}
