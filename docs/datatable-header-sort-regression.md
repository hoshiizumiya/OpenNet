# DataTable header layout and sort restoration

## Observed behavior

After fitting all columns to content, dragging any one column causes otherwise
unchanged headers to replace their final character with an ellipsis. Navigating
away and back also reproduces it. Clicking a sort header loses its state when the
page is recreated.

## Layout investigation

`DataTable::BeginColumnResize` freezes **all** visible resolved widths into pixel
`DesiredWidth` values. Restoring saved widths enters the same finite-measure path.
Consequently, the shared trigger is Auto/Star-to-Pixel layout, independently of
SQLite serialization precision.

Do not infer the headers' rounding state from the application's background
Image/MediaPlayerElement or its UIElement style. Read each element's effective
`UseLayoutRounding` value at runtime. The user requires preserving the existing
rounding policy and logical widths: do not enable rounding, round column widths,
or add extra padding as a workaround.

WinUI's TextBlock recalculates its DirectWrite maximum text width during Arrange
when trimming is enabled. ContentPresenter also chooses either its child's
DesiredSize or its complete content slot depending on content alignment. These
are candidate points where an Auto-to-Pixel transition can change the text
constraint. Neither the rounding hypothesis nor an upstream defect is confirmed
without runtime measurements.

Source locations:

- `dxaml/xcp/core/text/TextBlock/TextBlock.cpp`: `MeasureOverride`, `ArrangeOverride`.
- `dxaml/xcp/core/core/elements/framework.cpp`: `MeasureCore` and `LayoutRoundFloor`.
- `dxaml/xcp/core/core/elements/ContentPresenter.cpp`: `MeasureOverride`.

The Toolkit change measures Auto headers with unbounded horizontal space, then
reserves Auto widths before allocating Star space. OpenNet's sortable header
buttons stretch their content into that slot;
right-aligned numeric labels retain their TextAlignment. CharacterEllipsis stays
enabled for genuinely narrow columns.

The DataRow change captures `MaxChildDesiredWidth` **by value** before updating
it. The previous reference aliased the new value, so its change comparison could
never notify the header.

## Shared sort state

`DataTableSortViewModel` owns the active column, three-state direction cycle and
ToggleCommand. Tasks, Files, Peers and Trackers use separate stable table keys.
One SQLite setting in category `table_sort` stores both fields immediately.
Headers and collection-specific sorting read that same ViewModel. The pages
only adapt PropertyChanged notifications to their existing collection routines.
Reset persists source order; returning to a page or restarting restores it.

`scripts/test_datatable_sort_state.cpp` exercises page recreation between every
click, changing columns, reset, Unicode identifiers and malformed saved records.
Canary runs these native tests before the application build on x64.

## Validation boundaries

The portable sort tests and XML checks can run on Linux. The x64/ARM64 Canary
builds verify generated IDL/XAML and C++/WinRT code on Windows. A successful build
alone does **not** verify TextBlock's actual visual trimming behavior.

Windows visual regression procedure, for all four sortable tables and the
remaining DataTable headers:

1. Use 100%, 125%, 150%, 175% and 200% display scaling; test English and Chinese.
2. Fit all columns, including when their sum exceeds the horizontal viewport.
3. Drag one column wider. Other columns' complete headers must remain unchanged.
4. Drag that column narrower. Only genuinely constrained content should trim.
5. Navigate away/back, switch detail tabs, then restart with saved pixel widths.
6. Sort ascending/descending/source order and confirm both rows and indicators
   remain consistent after each navigation. Repeat for Files' sibling nodes and
   Peers' groups. Confirm reset and hidden columns still work.

If trimming remains, capture the failing label's `DesiredSize`, `ActualWidth`,
`ActualSize.X`, layout slot, `IsTextTrimmed`, `UseLayoutRounding`, XamlRoot.RasterizationScale, containing
Button and DataColumn dimensions immediately before and after the width freeze.
Do not change persistence precision without evidence that those saved values
differ from the resolved column widths.

## Diagnostic output

Peer headers IP, download speed and reason use the reusable
`DataTableHeaderDiagnostics` observer. Filter Visual Studio's Debug output for
`[ON-DTH]`. It records Width-property changes separately from SizeChanged and
IsTextTrimmedChanged, then schedules one read-only snapshot at dispatcher low
priority. It never calls Measure, Arrange, UpdateLayout, InvalidateMeasure or
changes widths or rounding. Weak element references avoid retaining the page.

`ActualWidth` alone is insufficient for TextBlock: see WinUI issues
[#1669](https://github.com/microsoft/microsoft-ui-xaml/issues/1669) and
[#8804](https://github.com/microsoft/microsoft-ui-xaml/issues/8804). Those concern
inconsistent text/element size reporting, not a confirmed duplicate of this
DataTable trimming defect. Both ActualSize.X and LayoutInformation.GetLayoutSlot
are logged so a change of text metrics is not mistaken for a change of layout.

The independent pure WinUI candidate in `repros/DataTableHeaderTrimming` removes
Toolkit and persistence. Canary builds it in a separate job and uploads an x64
executable. Its Freeze/Recreate actions preserve the exact width doubles, and
its options compare intrinsic Auto measurement and Stretch alignment. It is
not a confirmed reproduction until run on Windows. An upstream issue draft is
included with the runtime evidence still required before submission.

## Automated runtime evidence

The latest checked production commit `3773c315f62cb1fab4a81535aebcbca224ac46a4`
completed Canary run `36767387666` successfully: both x64/ARM64 application
builds, x64 native sort-state tests and the independent sample build passed.
Toolkit HEAD `20f248b129f5db9f2fe92bb404a5c25dacafd857` has no separate Actions
run; it was compiled as OpenNet's pinned submodule by those application builds.
These are build/persistence checks, not a visual result.

The sample now has an opt-in automated capture driven by
`ON_DTH_CAPTURE_DIRECTORY`, and the independent Canary job launches it on the
Windows runner. Sixteen cases compare the two Auto measurement modes, the two
button-content alignments, sort glyph presence and wide/narrow viewports.
Snapshots are taken after loaded layout readings settle, preserving all original
logical widths across Freeze and Recreate. Widening the first column checks
only its neighbours for unexpected changes. Each result records full baseline
and post-transition measurements; only complete-to-trimmed transitions count.
WinUI-rendered header PNGs and actual scale/version information accompany the
read-only layout log. No production layout/persistence policy is changed.

A runtime capture can fail independently of the application build if the runner
cannot launch/render WinUI. Zero transitions cannot clear the production bug:
the reduced panel has no Toolkit data rows or Star sizing, and only the runner's
native DPI is covered. Production sorting/arrow restoration and other DPI scales
still require Windows application regression. Do not submit the issue draft
without inspecting concrete runtime evidence.
