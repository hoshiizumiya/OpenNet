# DataTable header layout and sort restoration

## Result and scope

The user confirmed on 2026-10-01 that the original header truncation is fixed
in OpenNet. The repair combines Toolkit column measurement changes with
OpenNet's shared sortable-header Button content alignment. No layout-rounding
override, column rounding, extra padding or persistence precision change is used.
Individual changes were not visually isolated, so do not claim that one change
alone explains every previously truncated header or that WinUI has a proven bug.

## Toolkit measurement

`DataTable::BeginColumnResize` freezes all visible Auto/Star column widths to
fixed logical widths. Auto means content-sized; Star means proportional sizing.
Dragging one column therefore changes the measurement mode of its neighbours.
Restoring saved fixed widths after navigation enters the same finite-width path.

The old `MeasureOverride` measured Auto headers against the remaining viewport
width. Later headers could be constrained before their complete content was
measured, although horizontal scrolling permits the table to exceed the viewport.
It also measured Star columns without first reserving the Auto columns' widths.
The corrected two-pass measurement first measures Auto headers with unbounded
horizontal space, reserves their complete requirements and fixed column widths,
then allocates the remaining width to Star columns. Fixed columns continue to
use their requested widths, so deliberately narrow text still uses ellipsis.

`DataRow` previously retained `MaxChildDesiredWidth` by reference as its old
value. Updating the property also changed that referenced value, so the change
comparison could never notify the table. Capturing the old width by value
restores that notification. These two common fixes also belong on the fork's
`fix/DataTable` branch; unrelated branch differences must be preserved.

## OpenNet header alignment

`DataTableColumnHeaderButtonStyle.HorizontalContentAlignment` is now `Stretch`.
The Button's content uses the full assigned content slot during finite-width
layout rather than being limited to its requested content width. Text alignment
is still controlled by each label; using the full slot does not round or enlarge
the saved column width. This change applies to the shared sortable-header style.

## Shared sort state

`DataTableSortViewModel` owns the column, direction and ToggleCommand. Tasks,
Files, Peers and Trackers have separate stable table keys. One SQLite setting in
category `table_sort` stores both fields immediately; headers and collection
sorting read the same restored state. Page adapters connect PropertyChanged to
their existing collection sorting routines. Reset persists source order.

## Completion

The user confirmed the application header behavior is fixed. The prior full
x64/ARM64 Canary build for `7121de4f6f0afae18e9dcae37fec5aa71e2e29d3`
passed (run `36785048186`). Individual changes and every display scale were not
visually isolated, so this does not identify one exclusive cause.

The independent pure WinUI candidate did not reproduce the original transition.
The supplied manual log has `trimmed=False` throughout loaded layout and no
Auto-to-fixed or reconstruction transition. Zero sizes on Unloaded are teardown
readings. These logs do not establish a WinUI defect.

The temporary candidate, added CI checks, sort-state test file, issue draft and
production debug observer were removed at the user's request. They remain in
Git history. No upstream WinUI issue was submitted from this non-reproduction.
