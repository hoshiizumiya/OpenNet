# Draft: Text trimming after retaining an Auto-measured header's exact width

Not submitted: awaiting a Windows runtime result from the standalone sample.
If the sample does not reproduce, investigate the Toolkit implementation first.

## Reported behavior in OpenNet

Auto-fitting headers initially displays complete text. Resizing any one column
causes other headers to ellipsize their last character. Recreating the page
with retained column widths also causes trimming.

## Expected

Text should continue to fit when an Auto-measured column is arranged with the
same retained width. Resizing one column should not remove space from text in
other columns if their allocated widths remain identical.

## Isolation

OpenNet uses a fork of Toolkit Labs DataTable. On resize start it freezes all
resolved widths as Pixel widths. The attached candidate reduction uses only
WinUI, matching the header's nested Grid/ContentPresenter/Button/TextBlock
structure and reproducing that transition without saving or loading settings.

No `UseLayoutRounding` overrides, width rounding, extra padding, `UpdateLayout`,
or manual measurement of existing live text are used by diagnostics.

## Required evidence before submission

- OS build, Windows App SDK/WinUI version, display scale, font and locale.
- Exact sample revision and reproduction options; whether pure WinUI reproduces.
- Screenshot/video and paired diagnostic logs before/after the transition.
- Whether untouched columns' resolved widths, layout slots and `ActualSize.X`
  stay identical; the first ancestor where available width changes.
- Whether `IsTextTrimmed` changes without any allocated width change.
- Results with original/Stretch alignment and finite/intrinsic Auto measurement.

## Related issues, not confirmed duplicates

- [#1669](https://github.com/microsoft/microsoft-ui-xaml/issues/1669)
- [#8804](https://github.com/microsoft/microsoft-ui-xaml/issues/8804)

These explain why `TextBlock.ActualWidth` alone is insufficient evidence.
