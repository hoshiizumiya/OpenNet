# qBittorrent WebUI

This directory contains the unmodified qBittorrent WebUI used by OpenNet.

- Source: `qBittorrent/src/webui/www`
- Revision: see `UPSTREAM_COMMIT`
- Integrity manifest: `manifests/files.sha256`
- License texts and authors: `licenses/`

Do not edit files under `upstream`. Update them with:

```powershell
.\scripts\Update-QBittorrentWebUI.ps1
```

OpenNet-specific changes belong under `overrides`; `WebUIHost` serves that
directory through `/opennet/`. `WebUIHost` inlines only the compatibility adapter
into authenticated pages and HTML fragments (including qBittorrent's preferences
and property views), so fragment responses without `<head>`/`<body>` are covered
while the upstream HTML/CSS and visual design remain unchanged. The adapter
queries `/api/v2/opennet/capabilities` and disables qBittorrent-only server
options instead of pretending they were persisted by OpenNet. The runtime
compatibility target is recorded in
`docs/qbittorrent-webapi-compatibility-matrix.md`.

The qBittorrent source is GPL-2.0-or-later. Retain all file-level notices and
review the repository-wide licensing gate before publishing an OpenNet build
containing these assets.
