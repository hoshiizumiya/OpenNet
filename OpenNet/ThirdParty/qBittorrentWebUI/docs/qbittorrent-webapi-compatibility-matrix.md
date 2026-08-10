# OpenNet / qBittorrent WebUI compatibility

The bundled qBittorrent WebUI remains an upstream MooTools/native-DOM client.
OpenNet preserves its upstream HTML/CSS without a WinUIonWeb theme layer. The
host only inlines `adapter.js` into authenticated pages and qBittorrent's
headless HTML fragments. The adapter queries `/api/v2/opennet/capabilities` and
disables controls that have no OpenNet persistence or runtime implementation.

This boundary is intentional: replacing the files under `upstream` must not
require manually merging behavioral compatibility into upstream templates.

## Preference ownership

| qBittorrent preference group | OpenNet source of truth | Behaviour |
| --- | --- | --- |
| Save path, preallocation, automatic start | `TorrentSettings` | Two-way, applied to new downloads |
| Listen port, TCP/uTP, UPnP, connection limit | `TorrentSettings` | Two-way, live libtorrent apply |
| Proxy type, endpoint, authentication, peer use | `TorrentSettings` | Two-way, live libtorrent apply |
| Global/alternative speed limits | `TorrentSettings` + `webui_transfer` | Two-way, live apply |
| DHT, LSD, encryption, anonymous mode | `TorrentSettings` | Two-way, live apply |
| Queue and seeding ratio/time limits | `TorrentSettings` | Two-way, live apply; only Stop is supported |
| RSS maximum articles per feed | `CAT_RSS/max_items_per_feed` | Shared with the native RSS page |
| AIO/checking memory, multi-IP peers, announce-all | `TorrentSettings` | Two-way, live libtorrent apply |
| WebUI address, port, credentials | `webui_host` | Two-way; host restart is required |
| WebUI locale | OpenNet application language by default; optional `webui_host/locale` override | Normalized from BCP-47 tags to qBittorrent locale IDs; host restart is required |
| qBittorrent table/theme interaction options | `clientdata` | Stored by the compatibility API |
| qBittorrent-only server features | None | Visible but disabled by `adapter.js` |

The capability response is the contract. A control must not be enabled merely
because qBittorrent renders it; it is enabled only after its value has a real
OpenNet read path, write path, persistence location and runtime effect.
