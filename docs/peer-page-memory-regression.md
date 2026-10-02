# Peer page lifetime regression

## Evidence and ownership

The supplied Debug heap snapshots compare 1,520.96 s with 1,818.73 s.

| Allocation | Net change | Interpretation |
| --- | ---: | --- |
| IPv4 filter range nodes | +3,558 / +142,320 bytes | 106,356 new allocations; most older nodes were freed |
| IPv6 filter range nodes | +3,549 / +170,352 bytes | 20,196 new allocations; net growth is smaller than allocation churn |
| Peer row RangePieceBar objects | +206 | Row/control lifetime needs investigation |
| Peer template-related callbacks | +206 | Grows with the row controls |
| DataTable row registration nodes | +83 | The header's strong row registry can retain discarded rows |

The earlier snapshot contains one 6,589,247-byte IPRule array allocated by
GetRuleLookupSnapshot during session initialization. IPFilterManager owns one
cached snapshot until a rule mutation invalidates it or Close releases it.
BuildFilter also constructs the separate runtime libtorrent filter. Leaving
TaskPeersListPage should not remove a running session's IP protection.

The table registered rows during MeasureOverride, storing strong DataRow
references in DataTable::_rows. DataRow cached strong references to the header
table/panel. Unloaded broke this cycle only for rows that received that event;
a row measured and discarded before loading could retain the header and its
own template content. The fork now stores weak row registrations and weak
parent references. Layout invalidation resolves a temporary snapshot of live
rows and prunes expired entries.

TaskPeersListPage additionally detaches and empties its source collections,
releases endpoint/flag caches and DataContext in Unloaded, and queries peers
through a static worker that holds only a weak page reference. UI cleanup is
kept out of OnNavigatedFrom because Frame may still be navigating. Temporary
unload/reload can reattach through a weak navigation view-model reference.

## Windows verification

1. Build Debug, open a torrent with many peers, and select download/upload-rate
   sorting. Scroll through the rows and change the sort repeatedly.
2. Switch to a different detail tab, wait for the navigation animation and
   outstanding worker results, then take a heap snapshot. Repeat at least 20
   times. Also test leaving TasksPage entirely.
3. In Output, match `TaskPeersListPage: constructed <address>` with
   `TaskPeersListPage: destroyed <address>`. The number of live outgoing pages
   must not grow with navigation. Peer/control/template allocations should
   settle after leaving the peer page; do not use process private bytes alone
   as the lifetime assertion.
4. Collapse a group, change tasks, leave and return. Its expansion setting must
   survive source cleanup. Check column resize, hide/show, horizontal scrolling,
   selected-peer context-menu targeting, and selection after sorting.
5. Reload/import/edit an IP list. Each invalidated Debug lookup snapshot should
   eventually log `IPFilterManager: released rule lookup snapshot <address>`.
   A single current `cached rule lookup snapshot` remaining while the torrent
   session runs is expected. Compare rule counts when interpreting node growth.

The existing SQL regression suites run on Linux, but they do not verify WinUI
lifetimes. Windows CI compiles the application and the pinned toolkit changes;
the navigation/heap measurements above still require a Windows runtime.
