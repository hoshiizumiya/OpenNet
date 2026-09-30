using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace DataTableHeaderTrimming;

public sealed class MainWindow : Window
{
    private readonly Grid _host = new();
    private readonly CheckBox _intrinsic = new() { Content = "Intrinsic Auto measurement", IsChecked = false };
    private readonly CheckBox _stretch = new() { Content = "Stretch button content", IsChecked = false };
    private readonly CheckBox _sort = new() { Content = "Show sort glyph", IsChecked = false };
    private HeaderTable _table = null!;
    private static readonly string[] Labels = ["IP 地址", "下载速度", "上传速度", "连接原因", "Connection duration", "Client version"];

    public MainWindow()
    {
        Title = "DataTable header trimming — pure WinUI reduction";
        StackPanel root = new() { Spacing = 16, Margin = new Thickness(24) };
        StackPanel toolbar = new() { Orientation = Orientation.Horizontal, Spacing = 8 };
        AddButton(toolbar, "1. Auto-fit", () => _table.AutoFit());
        AddButton(toolbar, "2. Freeze all (no resize)", () => _table.FreezeAll());
        AddButton(toolbar, "3. Resize first +16", () =>
        {
            _table.FreezeAll();
            HeaderColumn first = (HeaderColumn)_table.Children[0];
            first.DesiredWidth = new GridLength(first.DesiredWidth.Value + 16);
        });
        AddButton(toolbar, "4. Recreate exact widths", Recreate);
        AddButton(toolbar, "Snapshot", () => _table.Snapshot("manual"));
        root.Children.Add(toolbar);
        StackPanel options = new() { Orientation = Orientation.Horizontal, Spacing = 16 };
        options.Children.Add(_intrinsic); options.Children.Add(_stretch); options.Children.Add(_sort);
        root.Children.Add(options);
        AddButton(options, "Apply options + Auto-fit", () => Create(null));
        root.Children.Add(new TextBlock { Text = "Drag the 8-DIP right edge of any header. Freeze/recreate use the exact same doubles, without a database. No layout rounding overrides or width rounding are used.", TextWrapping = TextWrapping.Wrap });
        root.Children.Add(_host);
        root.Children.Add(new TextBlock { Text = $"Debug output: [ON-DTH-REPRO]\nLog: {Trace.LogPath}", TextWrapping = TextWrapping.Wrap, IsTextSelectionEnabled = true });
        Content = root;
        Create(null);
        Trace.Write($"WinUI={typeof(Window).Assembly.FullName} OS={Environment.OSVersion} log={Trace.LogPath}");
    }

    private static void AddButton(Panel parent, string text, Action action)
    {
        Button button = new() { Content = text };
        button.Click += (_, _) => action();
        parent.Children.Add(button);
    }

    private void Create(double[]? widths)
    {
        _host.Children.Clear();
        _table = new HeaderTable { IntrinsicAutoMeasurement = _intrinsic.IsChecked == true, MinHeight = 40 };
        for (int i = 0; i < Labels.Length; ++i)
        {
            HeaderColumn column = new(Labels[i], _stretch.IsChecked == true, _sort.IsChecked == true && i == 0) { Owner = _table };
            if (widths != null)
            {
                column.DesiredWidth = new GridLength(widths[i]);
                if (column.DesiredWidth.Value != widths[i]) throw new InvalidOperationException("Width changed during recreation");
                Trace.Write($"recreate {Labels[i]} saved={widths[i]:G17} restored={column.DesiredWidth.Value:G17} equal=True");
            }
            _table.Children.Add(column);
        }
        _host.Children.Add(_table);
        DispatcherQueue.TryEnqueue(DispatcherQueuePriority.Low, () => _table.Snapshot("after-create"));
    }

    private void Recreate()
    {
        double[] widths = _table.Children.Cast<HeaderColumn>().Select(column => column.ResolvedWidth).ToArray();
        _table.Snapshot("before-recreate");
        Create(widths);
    }
}
