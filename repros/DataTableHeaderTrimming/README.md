# DataTable header trimming: standalone WinUI reduction

This is a **diagnostic candidate**, not yet a confirmed upstream reproduction.
It removes OpenNet, SQLite and the Toolkit dependency, retaining the relevant
header template and Auto-to-Pixel measure/arrange sequence. The production
Toolkit branch and this reduction must both be tested on Windows before calling
this a WinUI defect. The simplified panel supports Auto and Pixel, not Star,
and the sample has no data rows.

## Build and run

Windows, Visual Studio WinUI workload, .NET 10 SDK:

```powershell
dotnet publish repros/DataTableHeaderTrimming/DataTableHeaderTrimming.csproj -c Release -p:Platform=x64 -r win-x64 -o out/datatable-repro
.\out\datatable-repro\DataTableHeaderTrimming.exe
```

The Windows App SDK version matches OpenNet's current `2.5.1`. The unpackaged
self-contained executable requires no OpenNet account, service or database.

## Capture

1. Leave both measurement/alignment options unchecked. Press **Auto-fit**.
2. Record whether all headers are complete. Press **Freeze all (no resize)**.
   This converts every resolved width from Auto to Pixel without changing it.
3. Auto-fit again, then drag one header edge or press **Resize first +16**.
   Look at **other** headers, not just the resized one.
4. Auto-fit again, then **Recreate exact widths**. This rebuilds the visual tree
   using an in-memory array of the same doubles, without serialization.
5. Repeat with **Intrinsic Auto measurement** and **Stretch button content**,
   each separately and together. Show the sort glyph as a separate case.
6. Repeat at Windows display scales 100%, 125%, 150% and 200%, recording the
   actual root scale logged by the application. Do not force layout rounding.

Logs go to the displayed `%TEMP%` file and `OutputDebugString` with prefix
`[ON-DTH-REPRO]`. Use Visual Studio Debug Output or DebugView. Logs include
measure constraints, resolved/retained widths, layout slots, `ActualSize.X`,
`ActualWidth`, `DesiredSize`, border/padding, effective `UseLayoutRounding`,
and `IsTextTrimmed` transitions. Probes do not measure, arrange, invalidate,
round widths or change layout properties.

`TextBlock.ActualWidth` can describe text extent rather than the allocated
element width. Do not interpret it alone as evidence of a column width change.
See [WinUI #1669](https://github.com/microsoft/microsoft-ui-xaml/issues/1669)
and [#8804](https://github.com/microsoft/microsoft-ui-xaml/issues/8804).
Neither is a confirmed duplicate of the OpenNet problem.

If the sample does not reproduce, retain that result and compare its slots and
constraints against OpenNet's `[ON-DTH]` Peers header logs. Do not report an
upstream defect solely from static source inspection or a successful build.

## Automated Windows capture

Canary also launches the real WinUI executable, rather than treating its build
as a runtime test. To run the same capture locally:

```powershell
$env:ON_DTH_CAPTURE_DIRECTORY = Join-Path $PWD 'out/datatable-evidence'
Start-Process .\out\datatable-repro\DataTableHeaderTrimming.exe -Wait
Remove-Item Env:ON_DTH_CAPTURE_DIRECTORY
```

The capture drives all eight measurement/alignment/sort-glyph combinations with
1200-DIP and 360-DIP viewports. It waits for a loaded visual tree and unchanged
layout readings across two dispatcher timer intervals before recording each
stage. It checks unchanged columns' exact logical widths after Freeze, widening
the first column and Recreate; it also records a deliberately narrow column as
a control. No column-width rounding, extra padding or UseLayoutRounding override
is applied.

The `DataTableHeaderEvidence-x64-<SHA>` artifact contains `summary.json`, the
read-only layout log and PNGs rendered by WinUI for Auto, Freeze and Recreate.
The summary counts only newly trimmed labels whose baseline was complete;
baseline clipping in the finite-viewport case is retained separately. A capture
error, missing visual tree or timeout fails the capture step. A completed capture
with zero transitions means this candidate did not reproduce on that runner,
not that OpenNet is fixed. Newly trimmed transitions are evidence for further
investigation, not an automatic finding of an upstream defect.

The runner's actual XamlRoot scale, effective rounding values, OS and loaded
native Microsoft.UI.Xaml.dll version are recorded. This does not simulate other
DPI settings or replace the production Toolkit/OpenNet navigation, data-row and
sorting regressions. The original manual toolbar remains available when the
capture environment variable is absent.
