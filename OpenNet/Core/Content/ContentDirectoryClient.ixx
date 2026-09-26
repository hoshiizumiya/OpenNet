export module OpenNet.Core.Content.ContentDirectoryClient;

import std;
export import OpenNet.Core.Content.ContentDirectoryContracts;

export namespace OpenNet::Core::Content
{
    // Synchronous by design: ContentDirectorySyncService calls this object only
    // from its MTA background worker. Keeping std::optional/std::vector out of
    // WinRT IAsyncOperation<T> avoids trying to project non-WinRT C++ types.
    class ContentDirectoryClient
    {
    public:
        std::optional<ContentDirectoryLease> Register(
            std::string const& nodeId,
            std::uint64_t generation,
            std::string const& registrationId,
            std::optional<std::string> const& previousLeaseId,
            std::vector<ContentRecord> const& contents,
            std::vector<ContentDirectoryEndpoint> const& endpoints,
            std::stop_token stopToken = {});

        bool Heartbeat(
            std::string const& nodeId,
            std::string const& leaseId,
            std::stop_token stopToken = {});

        std::optional<std::vector<ContentWakeup>> PollWakeups(
            std::string const& nodeId,
            std::string const& leaseId,
            std::uint32_t maxItems = 32,
            std::stop_token stopToken = {});

        bool CompleteWakeup(
            std::string const& nodeId,
            std::string const& leaseId,
            std::string const& wakeRequestId,
            bool succeeded,
            std::string const& canonicalInfoHashV2 = {},
            std::vector<std::uint8_t> const& canonicalTorrent = {},
            std::string const& error = {},
            std::stop_token stopToken = {});

        bool AnnounceResource(
            std::string const& nodeId,
            std::string const& leaseId,
            ResourceKey const& resourceKey,
            ContentIdentity const& contentIdentity,
            std::stop_token stopToken = {});

        std::optional<ResourceLookupResult> LookupResource(
            ResourceKey const& resourceKey,
            std::uint32_t maxCandidates = 8,
            std::stop_token stopToken = {});

        std::optional<ContentLookupResult> Lookup(
            ContentIdentity const& identity,
            std::optional<std::string> const& excludeNodeId = std::nullopt,
            std::uint32_t maxPeers = 20,
            bool prepare = true,
            std::stop_token stopToken = {});

        std::optional<std::vector<std::uint8_t>> GetManifest(
            std::string const& contentId,
            std::stop_token stopToken = {});
    };
}
