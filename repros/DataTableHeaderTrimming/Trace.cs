using System.Diagnostics;
using System.Runtime.InteropServices;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Media;

namespace DataTableHeaderTrimming;

internal static class Trace
{
    private static readonly Stopwatch Clock = Stopwatch.StartNew();
    private static readonly System.Runtime.CompilerServices.ConditionalWeakTable<FrameworkElement, object> Observed = new();
    public static readonly string LogPath = Path.Combine(Path.GetTempPath(), $"DataTableHeaderTrimming-{Environment.ProcessId}.log");
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern void OutputDebugString(string message);

    public static void Write(FormattableString message) => Write(FormattableString.Invariant(message));
    public static void Write(string message)
    {
        string line = $"[ON-DTH-REPRO] t={Clock.ElapsedMilliseconds} {message}{Environment.NewLine}";
        OutputDebugString(line);
        try { File.AppendAllText(LogPath, line); } catch (IOException) { }
    }

    public static void ObserveTree(DependencyObject node, string label)
    {
        if (node is FrameworkElement element && !Observed.TryGetValue(element, out _))
        {
            Observed.Add(element, new object());
            element.SizeChanged += (_, args) =>
            {
                Write($"{label} SizeChanged {element.GetType().Name}/{element.Name} old={args.PreviousSize.Width:G17} new={args.NewSize.Width:G17}");
                QueueSnapshot(element, label);
            };
            element.RegisterPropertyChangedCallback(FrameworkElement.WidthProperty, (_, _) =>
            {
                Write($"{label} Width-property {element.GetType().Name}/{element.Name} Width={element.Width:G17}");
                QueueSnapshot(element, label);
            });
            if (element is TextBlock text)
                text.IsTextTrimmedChanged += (_, _) =>
                {
                    Write($"{label} IsTextTrimmedChanged={text.IsTextTrimmed}");
                    QueueSnapshot(element, label);
                };
        }
        for (int i = 0; i < VisualTreeHelper.GetChildrenCount(node); ++i)
            ObserveTree(VisualTreeHelper.GetChild(node, i), label);
    }

    private static void QueueSnapshot(FrameworkElement element, string label)
    {
        WeakReference<FrameworkElement> weak = new(element);
        element.DispatcherQueue.TryEnqueue(DispatcherQueuePriority.Low, () =>
        {
            if (weak.TryGetTarget(out FrameworkElement? current)) SnapshotTree(current, label);
        });
    }

    public static void SnapshotTree(DependencyObject node, string label)
    {
        if (node is FrameworkElement element)
        {
            Windows.Foundation.Rect slot = LayoutInformation.GetLayoutSlot(element);
            Write($"{label} {element.GetType().Name}/{element.Name} Width={element.Width:G17} ActualWidth={element.ActualWidth:G17} ActualSize.X={element.ActualSize.X:G17} DesiredSize.W={element.DesiredSize.Width:G17} slotX={slot.X:G17} slotW={slot.Width:G17} rounding={element.UseLayoutRounding} scale={element.XamlRoot?.RasterizationScale:G17}");
            if (element is TextBlock text)
                Write($"  text={text.Text} trimmed={text.IsTextTrimmed} padding={text.Padding.Left:G17},{text.Padding.Right:G17}");
            if (element is Control control)
                Write($"  alignment={control.HorizontalContentAlignment} padding={control.Padding.Left:G17},{control.Padding.Right:G17} border={control.BorderThickness.Left:G17},{control.BorderThickness.Right:G17}");
            if (element is Grid grid)
                foreach (ColumnDefinition column in grid.ColumnDefinitions)
                    Write($"  grid ActualWidth={column.ActualWidth:G17} configured={column.Width.Value:G17}/{column.Width.GridUnitType}");
        }
        for (int i = 0; i < VisualTreeHelper.GetChildrenCount(node); ++i)
            SnapshotTree(VisualTreeHelper.GetChild(node, i), label);
    }
}
