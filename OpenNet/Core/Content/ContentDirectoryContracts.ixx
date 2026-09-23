export module OpenNet.Core.Content.ContentDirectoryContracts;

import std;
import OpenNet.Core.Content.ContentCatalog;
import OpenNet.Core.Content.ResourceKey;

export namespace OpenNet::Core::Content
{
    enum class ContentPeerTransport : std::uint8_t
    {
        Tcp = 1,
        Utp = 2,
    };

    enum class ContentAddressFamily : std::uint8_t
    {
        Ipv4 = 4,
        Ipv6 = 6,
    };

    struct ContentDirectoryEndpoint
    {
        ContentPeerTransport transport{};
        ContentAddressFamily addressFamily{};
        std::uint16_t port{};
    };

    struct ContentDirectoryLease
    {
        std::string leaseId;
        std::uint64_t acceptedGeneration{};
        std::size_t contentCount{};
    };

    struct ContentWakeup
    {
        std::string wakeRequestId;
        std::string contentId;
        std::uint64_t size{};
        std::vector<ContentIdentity> identities;
    };

    struct ContentPeerEndpoint
    {
        std::string address;
        bool isIpv6{};
        std::uint16_t port{};
        ContentPeerTransport transport{};
        std::string verification;
    };

    struct ContentPeer
    {
        std::string nodeId;
        bool ready{};
        std::vector<ContentPeerEndpoint> endpoints;
    };

    struct ResourceCandidate
    {
        std::string contentId;
        std::uint64_t size{};
        std::uint32_t observationCount{};
        std::vector<ContentIdentity> identities;
    };

    struct ResourceLookupResult
    {
        ResourceKey key;
        std::vector<ResourceCandidate> candidates;
    };

    struct ContentLookupResult
    {
        std::string contentId;
        ContentIdentity identity;
        std::uint64_t size{};
        std::uint32_t canonicalProtocolVersion{};
        std::string canonicalInfoHashV2;
        bool manifestAvailable{};
        std::uint32_t retryAfterMilliseconds{};
        std::vector<ContentPeer> peers;
    };
}
