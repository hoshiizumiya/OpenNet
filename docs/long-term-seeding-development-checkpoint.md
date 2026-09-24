# Long-Term Seeding Development Checkpoint — 2026-09-24


> **Architecture update — 2026-09-24**
>
> The preferred HTTP P2P path has changed. For a privacy-safe public HTTP resource with a caller-supplied WholeFile SHA-256, matching size, BEP52 identity, known output path, and verified byte-range support, OpenNet now keeps aria2 paused as a temporary control/UI shell and makes the canonical BitTorrent v2 session the primary data plane. libtorrent receives the final HTTP origin as a BEP 19 URL seed and OpenNet peers as normal peers, so origin + P2P share one piece picker, one cryptographic verification path, and one disk writer. If no trusted candidate is found or the hybrid path fails, aria2 is resumed. The older separate-file full-download fallback remains only as a compatibility path.
>
> A custom aria2/libtorrent `TransferCoordinator` is no longer the default design for public HTTP acceleration. The invariant remains: `one output range -> one writer/owner`.

This document is the handoff checkpoint for the OpenNet long-term-seeding / HTTP P2P acceleration work.

It is intentionally operational: it records exactly what is implemented, what has been verified, what remains unsafe or incomplete, and what the next development session should do.

## Repositories and feature branches

### OpenNet client

Repository:

- https://github.com/hoshiizumiya/OpenNet

Feature branch:

- `feat/long-term-seeding-content-catalog`

Current client checkpoint for the canonical HTTP hybrid slice is this document's parent code lineage. It includes ResourceKey discovery, active 206 range verification, BEP19 URL-seed injection, paused aria2 control-shell routing, one-writer libtorrent hybrid transfer, HTTP progress bridging, caller SHA-256 verification, and fallback to aria2.

The implementation commit is based directly on:

- `master` at `420e8d54869d760d2944d30422bc863f80646f46`

### OpenNet.Server

Repository:

- https://github.com/Millennium-Science-Technology-R-D-Inst/OpenNet.Server

Feature branch:

- `feat/long-term-seeding-directory`

Current implementation checkpoint for the resource-hint slice:

- `a7d6c5def75a421673eda8096163151475ccdd3f`
- `fix: initialize required resource identity digest`

The implementation commit is based directly on:

- `master` at `c235cc09008c660cf5a7a0bfecdc6d81bbbf6b68`

The Server resource-hint slice at the checkpoint above passed restore/build/tests.

The earlier Server CI also exposed an EF Core SQLite `DateTimeOffset` relational-comparison issue. Content Directory instants remain stored through UTC Unix-millisecond value converters so lease/readiness queries stay database-side on SQLite and MySQL.

## Development / CI policy

Do **not** block development waiting for OpenNet's long Canary workflow.

Current policy for this feature:

1. batch related changes into one coherent development slice;
2. perform repository/source review before updating the branch;
3. commit the slice;
4. continue development immediately;
5. treat the long Windows Canary run as asynchronous feedback;
6. when a real compiler/test failure is available, collect related failures and fix them in one batch instead of repeatedly pushing one-line fixes.

OpenNet's Windows Canary can spend most of its runtime provisioning MSVC Preview Build Tools / vcpkg. Do not interpret "workflow still running" as a reason to stop implementation.

## Architectural invariant

The implementation is built around:

```text
Task lifetime != Content lifetime != Seed-session lifetime
```

A user-visible download task may stop or disappear while the verified local content remains registered and can later be exposed through a temporary hidden seed session.

The system deliberately does **not** keep every completed torrent permanently resident in libtorrent.

## Implemented: durable content model

Client-side content is represented independently from task state.

Implemented concepts:

- protocol-neutral `ContentIdentity`;
- one logical content object may have multiple identities;
- one logical content object may have multiple local file locations;
- task/source references are metadata, not ownership of the content lifetime;
- dedicated SQLite `content_catalog.db`;
- startup metadata validation;
- background hashing;
- HTTP completion ingestion;
- torrent completion ingestion;
- private torrents are excluded from the global long-seed directory.

Stable identity algorithm IDs currently are:

| ID | Name | Size | Meaning |
|---:|---|---:|---|
| 1 | `Bep52FileRootSha256` | 32 bytes | preferred native per-file identity |
| 2 | `WholeFileSha1` | 20 bytes | optional compatibility alias |
| 3 | `WholeFileSha256` | 32 bytes | generic full-file alias |
| 128 | `BitCometLtSeed160Opaque` | 20 bytes | opaque future BitComet compatibility identity |

Do not rename ID 2 back to `Bep47WholeFileSha1`. There is no basis for calling generic whole-file SHA-1 a BEP 47 identity.

## Implemented: BEP 52 and canonical hash material

For non-empty content, the preferred identity is the BEP 52 per-file Merkle root.

Current behavior:

- v2/hybrid torrent files reuse libtorrent's authoritative per-file root;
- v1-only torrent files are hashed in the background;
- completed HTTP files are hashed in the background;
- hashing also produces a whole-file SHA-256 alias;
- a fixed 1 MiB canonical piece-root layer is persisted for canonical swarm construction;
- v2/hybrid files that initially only have the authoritative file root can lazily derive/cache the canonical 1 MiB piece layer on first real wakeup demand.

Canonical piece-count arithmetic was hardened to avoid unsigned overflow at large file sizes.

Empty files are deliberately not published through `OpenNet.Content.v1` yet because BEP 52 omits `pieces root` for empty files and the current long-seed identity path is root-driven.

## Implemented: Content Directory control plane

The Server API is under:

```text
/api/v1/content
```

Implemented behaviors include:

- full node inventory registration;
- random application-level NodeId;
- generation ordering;
- persisted registration idempotency;
- node lease and heartbeat;
- content -> node presence mapping;
- bounded lookup;
- endpoint family/transport metadata;
- observed-address policy: the client submits ports, not an authoritative public IP;
- expired-node cleanup;
- multiple content identities / alias merging;
- alias conflict detection;
- SQLite and MySQL provider wiring.

The client sync worker uses revision tracking rather than a single dirty boolean, so an inventory mutation occurring during registration is not lost.

## Implemented: on-demand wakeup

A lookup can request preparation of a seed:

```text
requester lookup(prepare=true)
        |
        v
Server selects leased presences
        |
        v
Server assigns wakeRequestId
        |
        v
seeder polls wakeups
        |
        v
seeder resolves local ContentRecord
        |
        v
build/load canonical v2 metadata
        |
        v
open hidden libtorrent seed session
        |
        v
complete wakeup to Server
        |
        v
Server marks presence ready for short TTL
```

Server state includes:

- wake request ID/time;
- wake request expiration;
- retry backoff;
- seed-ready TTL.

Inventory refreshes for unchanged content preserve wake/readiness state rather than destroying an already-open seed session.

## Implemented: deterministic canonical v2 swarm

The native OpenNet canonical swarm currently uses a single-file BitTorrent v2 representation:

```text
name = "OpenNet.Content.v1"
piece length = 1 MiB
path = "OpenNet.Content.v1/content"
meta version = 2
length = exact content size
pieces root = authoritative BEP 52 file root
```

The canonical info dictionary excludes task-local or origin-local data:

- no original filename;
- no origin URL;
- no tracker;
- no creator;
- no timestamp.

Therefore equal file bytes + equal size + equal canonical protocol version produce the same canonical swarm identity.

Local filenames are mapped through libtorrent storage renaming and do not participate in the swarm identity.

## Implemented: canonical manifest security

The Server does not blindly trust the first node that reports a canonical torrent.

Successful canonical publication requires:

- an existing authoritative `Bep52FileRootSha256` identity;
- canonical protocol version 1;
- a 64-hex-character v2 info-hash;
- valid bencode;
- exactly the expected canonical structure;
- exact content size;
- expected canonical name/path/piece length;
- canonical torrent `pieces root` equal to the registered BEP 52 root;
- for multi-piece files, piece-layer bytes that Merkle-reduce to the registered file root;
- the claimed v2 info-hash equal to SHA-256 of the raw bencoded `info` dictionary.

Conflicting canonical info-hashes/manifests for one logical content object are rejected.

The requester independently validates the Server-supplied canonical manifest against:

- requested BEP 52 identity;
- expected size;
- canonical path/piece length;
- returned v2 info-hash.

The manifest is validated **before** libtorrent is allowed to use it for writing payload.

## Implemented: hidden libtorrent seed/download sessions

The client now has hidden long-seed transport primitives separate from normal task persistence.

Seeder path:

- load canonical v2 metainfo;
- map canonical file to the real local file;
- use libtorrent `seed_mode`;
- disable DHT/LSD/PEX for this directory-driven session;
- keep it out of normal torrent task state;
- remove after idle lifetime.

Requester path:

- lookup by authoritative BEP 52 identity;
- exclude its own Content Directory NodeId from lookup;
- wait briefly for a ready peer after wakeup;
- download canonical manifest;
- validate the manifest;
- open hidden download torrent at the requested target path;
- inject directory-provided TCP/uTP peer endpoints.

The hidden full-file download primitive is intentionally standalone.

## Implemented: HTTP ResourceKey discovery and verified fallback

HTTP resource discovery now uses hashed descriptors rather than raw URLs.

Current resource-key algorithms:

- `ExactUrlSha256V1`: SHA-256 over a normalized public HTTP(S) URL, exact query included;
- `HttpValidatorSha256V1`: SHA-256 over final URL + strong ETag + Content-Length.

Secret-bearing request contexts and suspicious signed/auth query names are excluded. The Server stores only ResourceKey digests and finite-lived observations bound to a current node ContentPresence.

The HTTP preflight carries final URI / size / strong ETag into DownloadManager. Lookup runs asynchronously and never blocks `aria2.addUri`.

A Server resource match remains untrusted. Automatic peer fallback is queued only when:

- the caller supplied a WholeFile SHA-256;
- a candidate contains that exact `WholeFileSha256` alias;
- known size matches;
- a BEP52 identity is present;
- the output filename is known.

The fallback downloads into a separate `.opennet-p2p-<gid>.part` file. libtorrent verifies BEP52 during transfer; OpenNet hashes the completed temporary file again and verifies the caller SHA-256. If the origin succeeds first, fallback is discarded. If the origin reaches Error and the fallback is verified, the temporary file is atomically promoted and the HTTP record is completed.

Explicit pause/cancel/remove/delete suppresses late resource lookups from resurrecting hidden P2P work.

## Important boundary: one physical output has one active writer

The normal trusted HTTP P2P route no longer uses two data writers. aria2 remains paused as a compatibility shell while libtorrent owns the payload path and combines BEP19 URL Seeds with OpenNet peers.

```text
paused aria2 shell (no payload writes)
        |
        v
libtorrent URL Seed + OpenNet peers
        |
        v
one piece picker / one writer
```

If the hybrid route fails, OpenNet closes libtorrent and removes its incomplete output before resuming aria2. Writer ownership is transferred, never shared.

A general `TransferCoordinator` is no longer the default HTTP plan. Only introduce one for future source types that libtorrent cannot model as URL Seeds or peers.

## Important boundary: endpoint reachability is not proven

Current Server endpoint verification string is effectively:

```text
observed-address-unverified-port
```

The HTTP control connection proves only the observed source address. It does not prove:

- the submitted TCP port is reachable;
- the submitted UDP/uTP port is reachable;
- the NAT mapping is correct;
- both IPv4 and IPv6 are reachable;
- a reverse proxy did not alter source-address semantics.

OpenNet.Traversal remains the future source of verified candidates / NAT traversal.

Do not confuse the current Content Directory with an ICE/STUN implementation.

## Important boundary: transport security remains separate

Content integrity and transport confidentiality are different layers.

Current native data plane reuses libtorrent's peer protocol over TCP/uTP.

Do not redesign ContentCatalog around TLS / Noise / post-quantum transport ideas.

Also do not describe BitTorrent MSE/PE as equivalent to modern mutually authenticated TLS.

If stronger peer authentication/confidentiality is required later, add it at the session/transport boundary without changing content identity.

## BitComet compatibility status

The architecture intentionally stays compatible at the **model** level without pretending to be wire-compatible.

Public BitComet documentation supports these high-level observations:

- LT-Seeding can outlive normal active task state;
- a central service participates in LT-Seed discovery;
- there is a per-file 160-bit `filehash`;
- LT-Seeding supports TCP/UDP modes;
- private torrents disable LT-Seeding.

Unknown / not publicly specified enough to implement interoperability:

- exact 160-bit filehash derivation;
- LT Server registration/lookup protocol;
- peer handshake/framing/block requests;
- authentication semantics;
- whether LT UDP is uTP.

Therefore:

- keep `BitCometLtSeed160Opaque` opaque;
- do not assume `filehash == SHA1(file)`;
- do not assume BitComet LT UDP == uTP.

Recommended research remains:

1. controlled-file hash characterization;
2. two owned clients / VMs;
3. TCP-only and UDP-only packet capture;
4. only then targeted executable analysis for specific unanswered interoperability questions.

## Known implementation limitations

- no deterministic end-to-end URL Seed + OpenNet peer hybrid test yet;
- exact libtorrent WebSeed request-path behavior for the current canonical layout still needs an automated fixture;
- hybrid requires caller WholeFile SHA-256, known size/path, privacy-safe public request semantics and an observed 206 + Content-Range response;
- in-flight wakeup/lookup is not cooperatively cancellable during shutdown;
- Resume does not automatically re-run discovery;
- late redirect/Content-Disposition output names cannot yet activate hybrid;
- stale hybrid/P2P partial cleanup after abnormal termination remains incomplete;
- same-target duplicate HTTP tasks need explicit exclusion;
- task-shell cleanup/session persistence needs stronger crash-recovery testing;
- Traversal/NAT/security/production hardening and BitComet wire compatibility remain future work.

## Next milestone

Build deterministic validation and lifecycle hardening for the canonical HTTP hybrid.

```text
local HTTP Range origin
 -> ResourceKey
 -> authoritative candidate
 -> wakeup / canonical manifest
 -> libtorrent URL Seed + OpenNet peer(s)
 -> BEP52 verification
 -> WholeFile SHA-256 verification
 -> HTTP Complete
 -> ContentCatalog
```

Recommended order:

1. Add a local deterministic HTTP Range fixture and record libtorrent's real URL requests.
2. Test the complete hybrid chain without public-network dependencies.
3. Harden shutdown/cancel/resume, stale partial cleanup and same-target exclusion.
4. Support late output filenames.
5. Integrate OpenNet.Traversal and then node keys/tickets/rate limits.
6. Continue BitComet compatibility separately.

Do not make `TransferCoordinator` the default HTTP milestone again.

## Files to read first next session

Client:

- `docs/long-term-seeding-architecture.md`
- `docs/long-term-seeding-architecture.zh-CN.md`
- this file
- `OpenNet/Core/Content/ContentIdentity.ixx`
- `OpenNet/Core/Content/ContentCatalog.ixx`
- `OpenNet/Core/Content/ContentCatalogService.*`
- `OpenNet/Core/Content/ContentHasher.*`
- `OpenNet/Core/Content/CanonicalV2Swarm.*`
- `OpenNet/Core/Content/ContentDirectoryContracts.ixx`
- `OpenNet/Core/Content/ContentDirectoryClient.*`
- `OpenNet/Core/Content/ContentDirectorySyncService.*`
- `OpenNet/Core/P2PManager.*`
- `OpenNet/Core/torrentCore/libtorrentHandle.*`
- `OpenNet/Core/DownloadManager.cpp`
- the current aria2 engine/models and HTTP task creation path

Server:

- `Application/ContentDirectoryService.cs`
- `Application/CanonicalTorrentValidator.cs`
- `Contracts/ContentDirectoryContracts.cs`
- `Domain/ContentDirectoryModels.cs`
- `Infrastructure/ContentDirectoryDbContext.cs`
- `Controllers/ContentDirectoryController.cs`
- `ContentDirectoryServiceTests.cs`

## Non-negotiable rules for continuation

- Search/read the actual repository before changing a subsystem.
- Preserve `Task lifetime != Content lifetime != Seed-session lifetime`.
- Do not make active tasks own durable content records.
- Do not invent a second custom P2P data protocol while libtorrent is sufficient.
- Do not mix aria2 and libtorrent disk writes without explicit range ownership.
- Do not trust URL/ETag as a content hash.
- Do not store secrets from private/signed URLs in the public directory.
- Do not call generic SHA-1 "BEP47".
- Do not assume BitComet proprietary details without evidence.
- Batch changes; do not block development waiting for the long Windows Canary workflow.
