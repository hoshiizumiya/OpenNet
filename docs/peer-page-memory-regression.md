# Files and peers page lifetime regression

## Navigation evidence and the shared header behavior

The subsequent navigation log pairs multiple TaskPeersListPage construction and
destruction messages, but the user still observes a large memory increase per
visit to both files and peers. A Page destructor is not evidence that its former
child controls have all been destroyed.

Both pages attach StickyHeaderBehavior to the table header. In the toolkit fork,
Behavior's strong AssociatedObject closes the attached-property ownership cycle:
header -> BehaviorCollection -> StickyHeaderBehavior -> header. HeaderBehaviorBase
also retained the ancestor ScrollViewer and composition resources. Its generic
Unloaded path did not detach the behavior. These cycles can retain a table and
scroll content independently of the Page.

The toolkit now uses weak behavior owners and a weak cached ScrollViewer. Header
behaviors revoke events, stop animations and clear composition handles on
Unloaded, and reacquire them on Loaded. The files page also stops its timer,
detaches ItemsSource, recursively clears its outgoing data tree and releases
DataContext/selected-file/header handles on Unloaded. A weak navigation view-model
reference allows a temporary unload/reload to restore subscriptions and data.

## Automatic reports instead of individual heap inspection

Build the updated branch and pinned toolkit. Debug builds enable the report by
default, including when started without the debugger. For Release builds, set
OPENNET_MEMORY_DIAGNOSTICS=1 in the process environment before starting the app.
Set OPENNET_MEMORY_DIAGNOSTICS=0 to disable it in Debug as well.
The report implementation is compiled in both configurations, so Release CI
also checks the diagnostic code even though collection is opt-in there.

The first visit logs its exact report path as `OpenNet memory report: <path>`.
The file is `%TEMP%\OpenNet-memory-<process-id>.log`; each line is flushed, and
the file remains readable while the application runs. It contains no task paths,
IP addresses or peer names.

Each visit records weak references to selected visual/control categories,
attached behaviors, image sources and display items. Row Loaded handlers also
record templates that might be recycled before leaving the page. After Unloaded,
a UI-thread timer reports the outgoing visit at approximately 2 and 10 seconds.
The timer captures only the visit number, never the Page or its visual tree.
`elapsed_ms` reports the actual delay, since a busy UI thread can delay a probe.

Example format (illustrative, not a measurement from this workspace):

```text
OpenNet memory: page=Peers visit=3 stage=unloaded+10s elapsed_ms=10000 memory_valid=true private_bytes=... working_set=... omitted=0 failed=0 alive/observed: DataRow=0/80 StickyHeaderBehavior=0/1 ...
```

`alive/observed` means objects whose weak references still resolve divided by
distinct tracked identities. It is not a process-wide allocation count and does
not measure retained bytes per object. The observation limit is 4,096 objects per
visit and at most four outgoing visits; `omitted` records observations skipped
at the limit, and `failed` records failed object/tree capture attempts. Weak
reference control blocks introduce bounded diagnostic overhead. Process private
bytes and working set describe the whole process, including heap reservations,
caches, background downloads and rendering allocations.

Open and leave each page five times, waiting ten seconds after each departure.
Send the log directly; no paused debugger or individual allocation clicks are
needed. In normal navigation, outgoing header behaviors, scrolling controls,
DataRows and display items should approach zero alive. Surviving categories show
which ownership branch to investigate next. If these objects expire but private
bytes keep rising, investigate other/native allocations rather than treating a
Page destructor as proof that the entire memory issue is fixed.

Also verify sticky headers after scrolling and temporary unload/reload, column
resize/visibility, file priority changes, context menus and peer group expansion.

The ownership cycles are confirmed by source inspection. Windows navigation
measurements remain pending; neither static checks nor CI compilation prove that
all reported memory growth is resolved.

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
