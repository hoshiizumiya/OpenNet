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

The application's main-window roots disable `UseLayoutRounding`. In WinUI's
`CTextBlock::MeasureOverride`, the desired text width is rounded **up** to a
physical pixel only when layout rounding is enabled. With rounding disabled,
the fractional text width travels through DataColumn's template, Button's
ContentPresenter and the sort Grid. On the later finite pass, subtraction and
float conversion can make the text constraint smaller than its natural width.
This is a source-supported mechanism, not a visually confirmed upstream bug.

Source locations:

- `dxaml/xcp/core/text/TextBlock/TextBlock.cpp`: `MeasureOverride`, `ArrangeOverride`.
- `dxaml/xcp/core/core/elements/framework.cpp`: `MeasureCore` and `LayoutRoundFloor`.
- `dxaml/xcp/core/core/elements/ContentPresenter.cpp`: `MeasureOverride`.

The Toolkit fix opts DataColumn into layout rounding, measures Auto headers with
unbounded horizontal space, then reserves Auto widths before allocating Star
space. OpenNet's sortable header buttons stretch their content into that slot;
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
`IsTextTrimmed`, `UseLayoutRounding`, XamlRoot.RasterizationScale, containing
Button and DataColumn dimensions immediately before and after the width freeze.
Do not change persistence precision without evidence that those saved values
differ from the resolved column widths.
