# VueTorrent WebUI integration

OpenNet treats VueTorrent as a second, replaceable frontend for the same
qBittorrent-compatible Web API. The checked-in `upstream/public` directory is a
built VueTorrent release, not a fork of its Vue/Vuetify source tree.

Run `scripts/Update-VueTorrentWebUI.ps1` to replace the assets from an official
release archive. This keeps upstream upgrades independent from OpenNet's host and
from the WinUI design-token overlay in `overrides`.

WinUIonWeb is also Vue-based, but it is an application/component source library,
not a drop-in CSS theme. OpenNet therefore shares its Fluent/WinUI design tokens
with VueTorrent and maps them to Vuetify variables at runtime. Replacing Vuetify
components with WinUIonWeb components would require maintaining a source fork and
is intentionally outside the asset boundary.

Upstream: https://github.com/VueTorrent/VueTorrent

