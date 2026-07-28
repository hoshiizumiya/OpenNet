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

OpenNet-specific changes belong in a separate patch or override layer. The
runtime compatibility target is recorded in
`docs/qbittorrent-webapi-compatibility-matrix.md`.

The qBittorrent source is GPL-2.0-or-later. Binary distributions containing its
GPLv3+ assets are GPL-3.0-or-later. Retain all file-level notices and review the
repository-wide licensing gate before publishing an OpenNet build containing
these assets.
