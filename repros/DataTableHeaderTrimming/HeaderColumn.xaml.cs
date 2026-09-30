using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.Foundation;

namespace DataTableHeaderTrimming;

public sealed partial class HeaderColumn : UserControl
{
    private GridLength _desiredWidth = GridLength.Auto;
    public string Label => HeaderText.Text;
    public double ResolvedWidth { get; internal set; }
    public HeaderTable? Owner { get; internal set; }
    public GridLength DesiredWidth
    {
        get => _desiredWidth;
        set
        {
            _desiredWidth = value;
            Trace.Write($"{Label} DesiredWidth={value.Value:G17} unit={value.GridUnitType}");
            InvalidateMeasure();
            Owner?.InvalidateMeasure();
        }
    }

    public HeaderColumn() => InitializeComponent();

    public HeaderColumn(string label, bool stretch, bool showSort) : this()
    {
        HeaderText.Text = label;
        HeaderButton.HorizontalContentAlignment = stretch ? HorizontalAlignment.Stretch : HorizontalAlignment.Left;
        SortIcon.Visibility = showSort ? Visibility.Visible : Visibility.Collapsed;
        ResizeThumb.DragStarted += (_, _) => Owner?.FreezeAll();
        ResizeThumb.DragDelta += (_, args) => DesiredWidth = new GridLength(Math.Max(24, DesiredWidth.Value + args.HorizontalChange));
        Loaded += (_, _) =>
        {
            Trace.ObserveTree(this, Label);
            Snapshot("Loaded");
        };
        Unloaded += (_, _) => Snapshot("Unloaded");
    }

    public void Snapshot(string reason)
    {
        Trace.Write($"{Label} {reason} DesiredWidth={DesiredWidth.Value:G17}/{DesiredWidth.GridUnitType} resolved={ResolvedWidth:G17}");
        Trace.SnapshotTree(this, Label);
    }

    internal HeaderState ReadState() => new(Label, ResolvedWidth, DesiredWidth.Value, DesiredWidth.GridUnitType.ToString(), HeaderText.IsTextTrimmed, HeaderText.ActualWidth, HeaderText.ActualSize.X, HeaderText.DesiredSize.Width, Microsoft.UI.Xaml.Controls.Primitives.LayoutInformation.GetLayoutSlot(HeaderText).Width, HeaderText.UseLayoutRounding, HeaderText.XamlRoot?.RasterizationScale ?? 0);
}

internal sealed record HeaderState(string Label, double ResolvedWidth, double DesiredWidth, string Unit, bool Trimmed, double TextActualWidth, double TextActualSize, double TextDesiredWidth, double TextSlotWidth, bool UseLayoutRounding, double Scale);

// Reduction of Toolkit DataTable's header measurement/freeze/arrange sequence.
// This project deliberately has no Toolkit or OpenNet dependency.
public sealed class HeaderTable : Panel
{
    public bool IntrinsicAutoMeasurement { get; set; }
    public void FreezeAll()
    {
        foreach (HeaderColumn column in Children)
        {
            Trace.Write($"freeze {column.Label} exact resolved={column.ResolvedWidth:G17}");
            column.DesiredWidth = new GridLength(column.ResolvedWidth);
        }
    }

    public void AutoFit()
    {
        foreach (HeaderColumn column in Children) column.DesiredWidth = GridLength.Auto;
    }

    public void Snapshot(string reason)
    {
        foreach (HeaderColumn column in Children) column.Snapshot(reason);
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        double used = 0;
        double height = 0;
        Trace.Write($"table Measure available={availableSize.Width:G17} intrinsicAuto={IntrinsicAutoMeasurement}");
        foreach (HeaderColumn column in Children)
        {
            double constraint = column.DesiredWidth.IsAuto
                ? (IntrinsicAutoMeasurement ? double.PositiveInfinity : Math.Max(0, availableSize.Width - used))
                : column.DesiredWidth.Value;
            column.Measure(new Size(constraint, availableSize.Height));
            column.ResolvedWidth = column.DesiredWidth.IsAuto ? column.DesiredSize.Width : column.DesiredWidth.Value;
            Trace.Write($"{column.Label} Measure constraint={constraint:G17} desired={column.DesiredSize.Width:G17} resolved={column.ResolvedWidth:G17}");
            used += column.ResolvedWidth;
            height = Math.Max(height, column.DesiredSize.Height);
        }
        return new Size(double.IsInfinity(availableSize.Width) ? used : Math.Min(used, availableSize.Width), height);
    }

    protected override Size ArrangeOverride(Size finalSize)
    {
        double x = 0;
        Trace.Write($"table Arrange final={finalSize.Width:G17}");
        foreach (HeaderColumn column in Children)
        {
            column.Arrange(new Rect(x, 0, column.ResolvedWidth, finalSize.Height));
            x += column.ResolvedWidth;
        }
        return finalSize;
    }
}
