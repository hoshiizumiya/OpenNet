using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Markup;
using Windows.Foundation;

namespace DataTableHeaderTrimming;

public sealed class HeaderColumn : UserControl
{
    private readonly TextBlock HeaderText = new() { Name = "HeaderText", FontWeight = Microsoft.UI.Text.FontWeights.SemiBold, TextTrimming = TextTrimming.CharacterEllipsis };
    private readonly Button HeaderButton = new() { Name = "HeaderButton", Padding = new Thickness(0), MinWidth = 0, HorizontalContentAlignment = HorizontalAlignment.Left };
    private readonly FontIcon SortIcon = new() { Name = "SortIcon", Width = 16, Margin = new Thickness(4, 0, 0, 0), FontSize = 10, Glyph = "\uE70E", Visibility = Visibility.Collapsed };
    private readonly Thumb ResizeThumb = new() { Name = "ResizeThumb", Width = 8, HorizontalAlignment = HorizontalAlignment.Right };
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

    public HeaderColumn()
    {
        // Use native WinUI controls without generated sample XBF. The same
        // presenter/button/two-column grids remain the experiment's inputs.
        Grid header = new() { Name = "HeaderTemplate" };
        header.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        header.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        ContentPresenter presenter = new() { Name = "HeaderPresenter", Margin = new Thickness(4, 0, 4, 0), HorizontalAlignment = HorizontalAlignment.Stretch, VerticalAlignment = VerticalAlignment.Center };
        Grid sort = new() { Name = "SortHeader" };
        sort.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        sort.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        Grid.SetColumn(SortIcon, 1);
        sort.Children.Add(HeaderText);
        sort.Children.Add(SortIcon);
        HeaderButton.Content = sort;
        presenter.Content = HeaderButton;
        ResizeThumb.Template = (ControlTemplate)XamlReader.Load("<ControlTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' TargetType='Thumb'><Border Background='Transparent'/></ControlTemplate>");
        Grid.SetColumn(ResizeThumb, 1);
        header.Children.Add(presenter);
        header.Children.Add(ResizeThumb);
        Content = header;
        Trace.Write("HeaderColumn native visual tree created");
    }

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
