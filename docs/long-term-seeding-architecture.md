# OpenNet Long-Term Seeding and HTTP P2P Acceleration Architecture


> **Architecture update — 2026-09-24**
>
> The preferred HTTP P2P path has changed. For a privacy-safe public HTTP resource with a caller-supplied WholeFile SHA-256, matching size, BEP52 identity, known output path, and verified byte-range support, OpenNet now keeps aria2 paused as a temporary control/UI shell and makes the canonical BitTorrent v2 session the primary data plane. libtorrent receives the final HTTP origin as a BEP 19 URL seed and OpenNet peers as normal peers, so origin + P2P share one piece picker, one cryptographic verification path, and one disk writer. If no trusted candidate is found or the hybrid path fails, aria2 is resumed. The older separate-file full-download fallback remains only as a compatibility path.
>
> A custom aria2/libtorrent `TransferCoordinator` is no longer the default design for public HTTP acceleration. The invariant remains: `one output range -> one writer/owner`.

> Handoff/checkpoint: [long-term-seeding-development-checkpoint.md](long-term-seeding-development-checkpoint.md)  
> Next-session prompt: [long-term-seeding-next-session-prompt.zh-CN.md](long-term-seeding-next-session-prompt.zh-CN.md)

> Status: architecture baseline and implementation guide.
>
> The local content catalog, Content Directory control plane, on-demand wakeup flow, deterministic canonical BitTorrent v2 swarm, hidden libtorrent seed/download sessions, privacy-preserving HTTP ResourceKey discovery, and a verified full-file peer fallback are implemented on the feature branches described in this document. Coordinated aria2/P2P range mixing, authenticated node identity, peer tickets, and verified NAT traversal remain subsequent phases.

## 1. Goals

OpenNet Long-Term Seeding is intended to keep verified downloaded content useful after the original download task becomes inactive.

The core invariant is:

```text
Task lifetime != Content lifetime != Seed-session lifetime
```

A task is a user-facing/download-engine object. A content record is durable knowledge that verified bytes exist locally. A seed session is an on-demand transport object created only when another client needs that content.

This separation enables all of the following:

- a stopped HTTP task can still contribute the completed file;
- a stopped or unloaded torrent task can still contribute files that are locally available;
- identical files originating from different torrents or HTTP URLs can converge on one logical content object;
- OpenNet does not need to keep every completed torrent resident and active in libtorrent;
- the control plane can evolve independently from the peer data plane;
- BitComet compatibility can be added later without replacing OpenNet's native content identity.

## 2. Repository boundaries

### OpenNet client

Repository: `hoshiizumiya/OpenNet`

Responsibilities:

- maintain the durable local `ContentCatalog`;
- derive or reuse cryptographic file identities;
- validate whether catalogued local file locations still exist;
- register the currently available content inventory with OpenNet.Server;
- maintain the node lease by heartbeat;
- query peer candidates for a content identity;
- lazily create hidden canonical seed/download sessions in libtorrent;
- validate Server-supplied canonical manifests against the authoritative BEP 52 file root before download.

### OpenNet.Server

Repository: `Millennium-Science-Technology-R-D-Inst/OpenNet.Server`

Responsibilities:

- map content identities to logical content objects;
- map currently leased nodes to those content objects;
- maintain node endpoints and lease expiry;
- perform content lookup and return a bounded set of peers;
- reject ambiguous identity aliases and stale inventory generations;
- never accept a client-supplied public IP as authoritative;
- in later phases, authenticate nodes and issue short-lived connection tickets.

### OpenNet.Traversal

Repository: `Millennium-Science-Technology-R-D-Inst/OpenNet.Traversal`

Traversal remains a separate subsystem. It is not the content index.

Its future responsibility in this design is to establish and verify network candidates using STUN/NAT traversal, UPnP/NAT-PMP/PCP where applicable, direct IPv6, and eventually relay fallback if desired.

The content directory answers:

```text
"Who claims to have content X?"
```

Traversal answers:

```text
"Which endpoint can node A actually be reached through?"
```

Those concerns must remain separate.

## 3. High-level architecture

```text
                  +----------------------+
                  |    OpenNet.Server    |
                  |----------------------|
                  | Content identities   |
                  | Content -> nodes     |
                  | Node leases          |
                  | Endpoint candidates  |
                  +----+------------+----+
                       ^            |
       register/       |            | lookup
       heartbeat       |            v
                       |      requester client
                       |
             +---------+---------+
             |  OpenNet client   |
             |-------------------|
             | ContentCatalog    |
             | HTTP task history |
             | Torrent history   |
             | libtorrent        |
             +---------+---------+
                       |
                later: candidate
                verification
                       |
                       v
              +------------------+
              | OpenNet.Traversal|
              +------------------+

Implemented data-plane primitive for known BEP 52 identities:

Requester libtorrent <------ TCP / uTP ------> Seeder libtorrent
                     canonical v2 swarm
```

The Server does not proxy ordinary file payloads.

## 4. Content identity model

A logical content object may have multiple identities.

Stable algorithm identifiers currently are:

| ID | Name | Bytes | Meaning |
|---:|---|---:|---|
| 1 | `Bep52FileRootSha256` | 32 | BitTorrent v2 per-file Merkle root |
| 2 | `WholeFileSha1` | 20 | Optional whole-file SHA-1 compatibility alias |
| 3 | `WholeFileSha256` | 32 | Generic whole-file SHA-256 alias |
| 128 | `BitCometLtSeed160Opaque` | 20 | Opaque BitComet LT-Seeding compatibility identity |

The local `ContentKey` is a random internal key. It is deliberately **not** a content hash.

Why:

1. the same bytes can acquire more identities later;
2. BitComet's 160-bit identity algorithm is not publicly specified well enough to make it OpenNet's primary identity;
3. adding a compatibility alias must not require changing primary keys or migrating references.

The server merges aliases through the uniqueness of:

```text
(algorithm, digest)
```

If two supplied aliases already resolve to two different logical content objects, registration is rejected as a conflict instead of silently merging inconsistent data.

## 5. BEP 52 identity

OpenNet's preferred native file identity is the BitTorrent v2 `pieces root`.

BEP 52 defines it for each non-empty file as a binary Merkle tree:

```text
file bytes
  |
  +-- 16 KiB block -> SHA-256 ----+
  +-- 16 KiB block -> SHA-256 ----+--> SHA-256(left || right) --> ...
  +-- final short block -> SHA-256+
```

Important details:

- leaf size is exactly 16 KiB except the final block, which may be shorter;
- the final short block is hashed at its actual length;
- to balance the tree to a power of two, leaf hashes beyond the end of the file are the all-zero 32-byte digest;
- SHA-256 is used at every Merkle level;
- empty files have no `pieces root`;
- identical file bytes always produce the same root.

Reference: [BEP 52](https://www.bittorrent.org/beps/bep_0052.html).

### Current client behavior

For BitTorrent v2 or hybrid torrents, OpenNet obtains the file root directly from libtorrent's torrent metadata. This avoids rereading the entire completed file.

For v1-only torrents and completed HTTP files, the background `ContentHasher` computes a BEP-52-compatible root and also computes a whole-file SHA-256 alias.

This is deliberately background work. HTTP currently uses aria2, so Phase 1 does not invasive-hook aria2's write path solely to compute hashes incrementally.

## 6. Logical content can have multiple local locations

A logical content object is not tied to one path.

Example:

```text
Content A
  identities:
    BEP52 = 4f...
    SHA256 = d2...

  locations:
    D:\Downloads\A.iso
    E:\Torrent\images\A.iso

  sources:
    HTTP record 932...
    torrent task 3ad..., file 0
    torrent task 8f1..., file 4
```

This is important because deleting one copy must not make another valid copy unavailable.

Current local domain model:

```text
ContentRecord
  ContentKey
  Size
  Identities[]
  Locations[]
  Sources[]
```

A location tracks:

- local path;
- availability state;
- file size;
- last-write timestamp;
- optional Windows volume/file identity for future stronger cheap validation;
- last verified timestamp.

## 7. Local SQLite catalog

The client uses a dedicated `content_catalog.db`, independent from application settings and task-history databases.

Current schema conceptually contains:

```text
content
  content_key PK
  file_size
  created_at

content_identity
  content_key FK
  algorithm
  digest
  UNIQUE(algorithm, digest)

content_location
  local_path PK
  content_key FK
  file_size
  last_write_ticks
  volume_serial nullable
  file_id nullable
  availability
  verified_at

content_source
  content_key FK
  source_kind
  source_id
  source_file_index nullable
```

SQLite runs with WAL, normal synchronous mode, foreign keys enabled, and a busy timeout.

## 8. Ingestion rules

### 8.1 HTTP completion

When aria2 reports a transition into `Complete`:

1. OpenNet resolves the actual output paths reported by aria2.
2. Each completed file is enqueued into `ContentCatalogService`.
3. The background worker hashes it.
4. Existing matching content is reused if any cryptographic identity already exists.
5. The HTTP record is stored as a source reference.
6. A catalog mutation marks the Server inventory dirty.

Deleting the actual downloaded file removes that location from the catalog.

Deleting only the task record must not automatically destroy other physical locations of the same logical content.

### 8.2 BitTorrent v2/hybrid completion

For a public torrent:

1. ignore pad files;
2. ignore files whose torrent priority is zero;
3. read the BEP 52 per-file root from libtorrent;
4. use the actual current mapped filename and current storage path;
5. enqueue the location with the known root;
6. because the authoritative root is already known, do not reread the full file only to create the same identity.

### 8.3 BitTorrent v1-only completion

v1 does not provide a standard independent per-file Merkle root.

Therefore OpenNet queues the completed file for background BEP 52-compatible hashing.

### 8.4 Private torrents

Private torrents are intentionally excluded from the OpenNet long-term content directory.

A private torrent's tracker/discovery restrictions are part of its distribution semantics. Automatically advertising its files through a separate global directory would bypass those expectations.

A future explicit opt-in design would require separate policy review; it is not part of the current architecture.

## 9. Startup validation and I/O policy

OpenNet must not hash terabytes of historical files on every launch.

Startup validation is therefore metadata-first:

```text
path exists?
  |
  +-- no  -> Missing
  |
  +-- yes -> size unchanged?
                |
                +-- no  -> Modified
                |
                +-- yes -> mtime unchanged?
                              |
                              +-- no  -> Modified
                              |
                              +-- yes -> keep cached availability
```

Future Windows file ID / USN integration can make this stronger.

Filesystem metadata is only a cache-validity optimization. It is never used as cryptographic proof for network data.

## 10. Content Directory control plane

The initial Server API is versioned under:

```text
/api/v1/content
```

### 10.1 Register a full inventory

```http
POST /api/v1/content/nodes/register
```

Example:

```json
{
  "nodeId": "4ac2d9e2-7d31-4ff5-939a-fcd0a459f14c",
  "generation": 18,
  "registrationId": "8928c0e1-214b-48ac-b020-c12821640b63",
  "previousLeaseId": "b40a6136-8e0c-42bd-a743-e437671f82b7",
  "endpoints": [
    { "transport": "Tcp", "addressFamily": "Ipv4", "port": 6881 },
    { "transport": "Utp", "addressFamily": "Ipv4", "port": 6881 },
    { "transport": "Tcp", "addressFamily": "Ipv6", "port": 6881 },
    { "transport": "Utp", "addressFamily": "Ipv6", "port": 6881 }
  ],
  "contents": [
    {
      "size": 4294967296,
      "identities": [
        {
          "algorithm": 1,
          "digest": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        }
      ]
    }
  ]
}
```

The client does **not** submit a public IP field.

The server derives the currently observed remote address from the HTTP connection.

A successful request transactionally replaces that node's previous full inventory and returns:

```json
{
  "leaseId": "6c8a08b7-9359-4d33-813e-18ddfcb93437",
  "leaseExpiresAtUtc": "2026-09-23T10:30:00Z",
  "acceptedGeneration": 18,
  "contentCount": 1
}
```

### 10.2 Heartbeat

```http
POST /api/v1/content/nodes/{nodeId}/heartbeat
```

```json
{
  "leaseId": "6c8a08b7-9359-4d33-813e-18ddfcb93437"
}
```

Current default:

- lease lifetime: 300 seconds;
- client heartbeat target: approximately every 90 seconds;
- failed operations retry after a bounded delay.

Expired nodes are excluded from lookup and are periodically removed from the Server database.

### 10.3 Lookup

```http
GET /api/v1/content/lookup?algorithm=1&digest={hex}&maxPeers=20&excludeNodeId={self}
```

The response contains only currently leased nodes, a bounded number of peers, and their candidate endpoints.

Current endpoint verification is explicitly reported as:

```text
observed-address-unverified-port
```

This distinction matters: observing the request's source IP does **not** prove that the advertised TCP or UDP port is reachable from the Internet.

## 11. Generation, lease and idempotency semantics

Three values solve different problems.

### Generation

`generation` is a monotonic inventory version for a node.

A lower generation than the server has already committed is rejected.

### Lease

`leaseId` proves possession of the current short-lived server registration state.

A newer generation cannot replace an active lease unless the client supplies the previous lease ID.

This is state continuity, not cryptographic authentication.

### RegistrationId

`registrationId` is an idempotency key for one generation.

The client persists the pending generation and pending registration ID **before** sending the request.

If:

```text
server commits
      |
HTTP response is lost
      |
client retries
```

the retry sends the same generation and registration ID. The Server accepts the repeat instead of forcing the client to wait for the unknown lease to expire.

The same generation with a different registration ID is rejected.

## 12. Lossless client synchronization

A single boolean `dirty` flag is insufficient.

Consider:

```text
revision 10 snapshot starts registration
          |
          +---- new download finishes -> revision 11
          |
registration of revision 10 succeeds
```

If success simply sets `dirty = false`, revision 11 is lost.

Therefore the client keeps:

```text
localRevision
syncedRevision
```

A registration captures an attempt revision. On success only that revision becomes synchronized. If the local revision advanced during the request, another registration remains required.

This also prevents failed requests from spinning in a tight loop: unsuccessful attempts wait for the retry interval instead of waiting on a condition that is already true.

## 13. Node identity and privacy

The Content Directory node ID is a random application-local UUID persisted in OpenNet settings.

It intentionally does not reuse the existing user/machine-derived device ID.

The content directory does not need stable hardware identity. Avoiding it reduces unnecessary correlation across unrelated OpenNet services.

This random NodeId is **not** authentication. Authentication is a later hardening phase.

## 14. Endpoint security model

### What the client is allowed to declare

The current registration request declares:

- transport: TCP or uTP;
- address family: IPv4 or IPv6;
- the exact libtorrent listening port for that transport/family.

The client derives these values from libtorrent's `listen_succeeded_alert`, which distinguishes TCP and uTP sockets. It also respects the user's incoming TCP/uTP settings.

It does not declare an authoritative public address.

### What the Server records

The Server first determines whether the observed control connection is IPv4 or IPv6, discards submitted endpoints for the other family, and combines only matching submitted ports with the observed IP address.

This prevents a trivial attack where a malicious client registers an arbitrary victim IP and causes lookup clients to connect to it.

### What remains unverified

The observed IP does not prove that:

- a NAT maps the submitted port;
- a firewall accepts inbound TCP;
- UDP reaches libtorrent's listener;
- the source address seen through a reverse proxy is the real client address.

Therefore current candidates are hints, not reachability proofs.

Before a public production deployment, reverse-proxy forwarding must be configured only for explicitly trusted proxies/networks. Blindly trusting arbitrary `X-Forwarded-For` values would undo the observed-address security property.

## 15. IPv4, IPv6 and Traversal

One HTTP control connection normally provides only one observed source address family.

It is incorrect to infer both an IPv4 and IPv6 endpoint from that one connection.

The future flow should be:

```text
OpenNet client
  |
  +-- direct IPv6 candidate
  +-- observed/mapped IPv4 candidate
  +-- STUN UDP candidate
  +-- UPnP/NAT-PMP/PCP mapping
  |
  v
OpenNet.Traversal / reachability probes
  |
  v
verified candidates
  |
  v
Content Directory node lease
```

The Server should eventually store multiple independently verified candidates per node, including address family, transport, priority, verification method and expiry.

Relay should be a last resort because it transfers file payload through infrastructure and changes the cost model.

## 16. Data plane: canonical BitTorrent v2 swarm

The first canonical data-plane slice is now implemented. It is intentionally a hidden transport primitive rather than a user-visible torrent task.

Content identity alone is not enough to make two libtorrent peers communicate: both peers need to join the same BitTorrent swarm/info-hash.

OpenNet therefore plans a deterministic single-file BitTorrent v2 representation for each content object.

A proposed `OpenNet.Content.v1` info dictionary is:

```text
meta version = 2
piece length = 1 MiB                # fixed protocol constant
name = "OpenNet.Content.v1"         # fixed protocol constant
file tree:
  "content":
    "":
      length = exact file length
      pieces root = BEP52 root      # omitted for empty file
```

No timestamp, creator, URL, original filename or tracker is allowed inside the canonical info dictionary.

Consequences:

```text
same bytes
  -> same BEP52 root
  -> same canonical info dictionary
  -> same v2 info-hash
  -> same OpenNet canonical swarm
```

The actual local filename is a storage mapping detail and must not change the swarm identity.

For large files, v2 piece layers are outside the info dictionary. OpenNet now derives fixed 1 MiB canonical piece roots while hashing completed HTTP/v1 content, persists them in `content_catalog.db`, and lazily derives/caches them on first demand for v2/hybrid files that initially only supplied an authoritative file root.

The Server caches canonical metainfo only after validating the raw v2 info-hash, fixed canonical structure, registered BEP 52 root, and the piece-layer Merkle result. Requesters independently validate the manifest again before libtorrent can write payload.

### Implemented on-demand seed lifecycle

```text
ContentCatalog says file exists
        |
        | Server lookup prepare / wakeup
        v
build/load canonical v2 metadata
        |
map canonical file "content" -> actual local location
        |
add hidden seed_mode torrent to libtorrent
        |
Server marks the node ready for a short TTL
        |
requester loads/validates canonical manifest
        |
requester adds hidden download torrent
        |
inject selected TCP/uTP peers
        |
idle timeout
        |
remove temporary canonical torrent
```

The catalog remains resident after the temporary session is removed. These hidden torrents are deliberately absent from normal task persistence and task UI state.

## 17. HTTP ResourceKey discovery and canonical WebSeed hybrid

`ResourceKey` is discovery metadata, not a content identity. Exact URL and validator-qualified keys remain privacy-preserving hints; raw URLs are not uploaded to the public Content Directory, and private/authenticated/signed request contexts are excluded.

Automatic canonical-hybrid routing is deliberately strict. It requires:

1. a caller-supplied whole-file SHA-256;
2. an exact matching `WholeFileSha256` alias on the candidate;
3. matching known Content-Length;
4. a BEP52 file-root identity;
5. a known output path;
6. a public/privacy-safe HTTP request;
7. an active `GET Range: bytes=0-0` probe that actually returns `206 Partial Content` with `Content-Range`.

`Accept-Ranges: bytes` is only a UI hint and is not sufficient to enable WebSeed routing.

### Primary trusted data plane

When the request is eligible, aria2 is initially created paused only as a temporary compatibility shell for the existing HTTP GID / SQLite / UI model. It does not own payload writes while discovery decides the route.

```text
HTTP task
  +-- paused aria2 control shell (no payload writes)
  |
  v
canonical OpenNet.Content.v1 torrent
  +-- BEP 19 HTTP URL Seed
  +-- OpenNet TCP/uTP peer(s)
  |
  v
one libtorrent piece picker
  |
  v
one disk writer
```

The final public HTTP origin is passed through `add_torrent_params::url_seeds`. A ready OpenNet peer is no longer required before opening the canonical download when a valid URL seed is available. HTTP and P2P therefore contribute pieces through one libtorrent scheduler and one cryptographic verification path.

The current primary hybrid writes the destination directly because aria2 remains paused. On hybrid failure, OpenNet closes the hidden canonical session, removes the incomplete libtorrent-owned destination/control residue, and only then resumes aria2. Ownership is transferred, never shared.

After libtorrent finishes, OpenNet also re-hashes the complete file and requires caller WholeFile SHA-256/size to match before marking the HTTP record Complete and cataloguing the final file. The older separate `.opennet-p2p-<gid>.part` path remains available as a compatibility/error-recovery fallback.

Explicit Pause/Cancel/Remove/Delete cancels hidden work and suppresses late discovery results.

### TransferCoordinator is not the default HTTP P2SP design

The old design assumed two independent writers and therefore required a custom range owner. That is no longer necessary for ordinary public HTTP acceleration: BEP 19 lets libtorrent coordinate HTTP URL-seed requests and BitTorrent peer requests inside the same torrent.

The invariant is:

```text
one physical output -> one active data writer
```

OpenNet must never point active aria2 and libtorrent sessions at the same physical file. A future `TransferCoordinator` is only justified for heterogeneous sources that cannot be represented as libtorrent URL Seeds or peers.

## 18. Content integrity vs transport encryption

These are different security properties.

### Content integrity

BEP 52 SHA-256 hashes answer:

> Did I receive the exact expected bytes?

A malicious peer that sends corrupt blocks cannot make those bytes pass the content hash.

### Transport confidentiality/authentication

TLS, Noise, BitTorrent protocol encryption, and future post-quantum schemes answer different questions:

- can an observer read transport bytes?
- can a man-in-the-middle modify the session?
- is the remote peer authenticated?

The current architecture deliberately does not invent a new file-transfer protocol. The planned first data plane reuses libtorrent's peer protocol over TCP/uTP.

This keeps TLS/Noise/PQC decisions outside the ContentCatalog and Content Directory schemas. If the transport security policy changes later, content identity does not need to migrate.

BitTorrent MSE/PE should not be treated as equivalent to a modern mutually authenticated TLS channel. If OpenNet later requires stronger peer authentication/confidentiality, it should be added at the transport/session boundary.

## 19. BitComet interoperability strategy

BitComet's public documentation confirms several architectural concepts that are compatible with this design:

- LT-Seeding is independent of ordinary BitTorrent uploading;
- stopped tasks can continue to provide LT-Seeding data;
- a central server is used to locate LT-Seeds;
- each file has a 160-bit LT-Seeding `filehash`;
- LT-Seeding can use TCP, UDP, or both;
- private torrents disable LT-Seeding.

References:

- [BitComet Long-Term Seeding](https://wiki.bitcomet.com/long-term-seeding)
- [Inside BitComet](https://wiki.bitcomet.com/inside-bitcomet)

However, the public material does **not** define enough of the following to claim wire interoperability:

- exact LT `filehash` derivation;
- LT server registration/lookup wire format;
- peer handshake/framing/block request protocol;
- authentication semantics;
- whether BitComet's LT UDP transport is uTP.

OpenNet must **not** assume:

```text
BitComet LT UDP == uTP
```

The current enum value `BitCometLtSeed160Opaque` therefore preserves a raw 20-byte compatibility identity without claiming what algorithm produced it.

### Compatibility research plan

Use progressively more invasive methods only as needed:

1. Generate controlled files and BitComet-created torrents.
2. Compare `filehash` against candidate algorithms such as whole-file SHA-1.
3. Test boundary sizes around 16 KiB, piece sizes and powers of two.
4. Capture traffic between two owned BitComet clients in TCP-only and UDP-only LT modes.
5. Identify server discovery, handshake, file identifier, block framing and keepalive behavior.
6. Only if black-box evidence cannot answer a specific interoperability question, inspect the executable for that narrow question.
7. Keep every finding versioned and reproducible; do not bake guesses into the OpenNet core identity model.

A future official cooperation path can then implement a `BitCometCompatibilityAdapter` without changing the native OpenNet catalog.

## 20. Server threat model and hardening roadmap

The current implementation is an **alpha control plane** and is not yet sufficient for an untrusted public Internet deployment.

Implemented protections:

- cryptographic content identities have strict lengths;
- aliases that contradict existing content mappings are rejected;
- stale inventory generations are rejected;
- registration retries have explicit idempotency keys;
- clients cannot submit an arbitrary public IP;
- leases expire;
- expired nodes are removed;
- private torrent files are not announced;
- lookup result count is bounded;
- inventory item count is bounded;
- canonical publication requires an authoritative BEP 52 file-root identity;
- canonical metainfo and piece layers are validated server-side before publication;
- conflicting canonical manifests/info-hashes are rejected;
- wakeups use finite lifetimes, retry backoff and short seed-ready TTLs.

Still required before public production:

1. node key pair and signed control requests;
2. server-issued or mutually authenticated node session;
3. short-lived peer connection tickets bound to requester, seeder and content;
4. per-node/IP rate limits and abuse quotas;
5. verified endpoint ownership/reachability through Traversal;
6. trusted reverse-proxy configuration;
7. replay protection beyond registration idempotency;
8. metrics for inventory size, lease churn, lookup rate and failed verification;
9. database migrations instead of development-time `EnsureCreated`;
10. retention policy for permanently orphaned content metadata.

## 21. Current implementation status

### Implemented in OpenNet client

- [x] multi-identity/multi-location ContentCatalog with background hashing and startup validation;
- [x] BEP52 root reuse, canonical 1 MiB piece layer, and deterministic `OpenNet.Content.v1` v2 swarm;
- [x] Content Directory inventory/lease synchronization, demand wakeup, manifest validation and TCP/uTP peer injection;
- [x] privacy-safe HTTP ResourceKey discovery and resource observations;
- [x] candidate selection gated by caller WholeFile SHA-256 and size;
- [x] active 206 + Content-Range probe for BEP 19 eligibility;
- [x] hidden canonical download sessions carrying HTTP URL seeds;
- [x] primary canonical HTTP hybrid using a paused aria2 control shell and one libtorrent writer;
- [x] HTTP URL Seed + OpenNet peers scheduled by one libtorrent piece picker;
- [x] hybrid progress bridged back to the existing HTTP task UI/persistence;
- [x] caller SHA-256 post-verification and Complete/ContentCatalog integration;
- [x] hybrid failure cleanup before aria2 ownership resumes;
- [x] older separate-file peer fallback retained for compatibility/recovery.

### Implemented in OpenNet.Server

- [x] logical content/multi-identity aliases, node presence, leases, generation and idempotency;
- [x] bounded content lookup, demand wakeup and canonical manifest validation/cache;
- [x] ResourceObservation announce/lookup with digest-only keys, TTL, bounded candidates and stale-ownership invalidation;
- [x] SQLite/MySQL wiring and service tests.

### Remaining gaps

- [ ] deterministic end-to-end HTTP Range + WebSeed + OpenNet peer hybrid test;
- [ ] automated confirmation of the exact libtorrent request-path semantics for the current canonical single-file layout;
- [ ] cooperative cancellation of in-flight wakeup/lookup during shutdown;
- [ ] automatic re-discovery after Resume;
- [ ] activating hybrid when the output filename arrives late via redirect/Content-Disposition;
- [ ] stale hybrid/P2P partial cleanup after abnormal termination;
- [ ] same-target duplicate HTTP-task exclusion;
- [ ] stronger crash/recovery semantics around HTTP task-shell cleanup/session persistence;
- [ ] Traversal-verified IPv4/IPv6 candidates, hole punching, relay, node keys and peer tickets;
- [ ] production abuse controls/migrations and BitComet LT wire compatibility.

## 22. Recommended next implementation sequence

1. Add a deterministic local HTTP Range server fixture and record the exact libtorrent WebSeed request path for `OpenNet.Content.v1`.
2. Test URL -> ResourceKey -> candidate -> wakeup -> manifest -> URL Seed + peer -> BEP52 -> WholeFile SHA-256 -> HTTP Complete -> ContentCatalog end to end.
3. Harden shutdown/cancel/resume, stale partial cleanup and same-target exclusion.
4. Support output names discovered after task creation.
5. Integrate OpenNet.Traversal for verified IPv4/IPv6 candidates and hole punching.
6. Add node keys, request signatures, peer tickets, rate limits and production migrations.
7. Continue BitComet compatibility work independently.

Do not reintroduce a general `TransferCoordinator` as the default HTTP path unless a future source type cannot be represented as a libtorrent URL Seed or peer.
