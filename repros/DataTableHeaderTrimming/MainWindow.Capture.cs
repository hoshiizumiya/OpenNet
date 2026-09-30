using System.Diagnostics;
using System.Text.Json;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media.Imaging;
using Windows.Graphics.Imaging;
using Windows.Storage.Streams;

namespace DataTableHeaderTrimming;

public sealed partial class MainWindow
{
    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };

    // Drives the same operations as the manual toolbar. The observer only reads
    // layout: it never measures, arranges, invalidates or adjusts column widths.
    public async Task CaptureAsync(string directory)
    {
        Directory.CreateDirectory(directory);
        List<object> cases = [];
        int transitions = 0;
        int exitCode = 0;
        try
        {
            AppWindow.Resize(new Windows.Graphics.SizeInt32(1440, 900));
            foreach (double viewport in new double[] { 1200, 360 })
            foreach (bool intrinsic in new[] { false, true })
            foreach (bool stretch in new[] { false, true })
            foreach (bool sort in new[] { false, true })
            {
                string name = $"v{viewport}-intrinsic{intrinsic}-stretch{stretch}-sort{sort}";
                _host.Width = viewport;
                _intrinsic.IsChecked = intrinsic;
                _stretch.IsChecked = stretch;
                _sort.IsChecked = sort;
                Create(null);
                HeaderState[] auto = await SettleAsync();
                await CaptureImageAsync(Path.Combine(directory, $"{name}-auto.png"));

                _table.FreezeAll();
                HeaderState[] frozen = await SettleAsync();
                RequireWidths(auto, frozen, 0);
                await CaptureImageAsync(Path.Combine(directory, $"{name}-freeze.png"));
                string[] freezeTransitions = NewlyTrimmed(auto, frozen, 0);

                _table.AutoFit();
                HeaderState[] resizeBaseline = await SettleAsync();
                _table.FreezeAll();
                HeaderColumn first = (HeaderColumn)_table.Children[0];
                first.DesiredWidth = new GridLength(first.DesiredWidth.Value + 16);
                HeaderState[] resized = await SettleAsync();
                RequireWidths(resizeBaseline, resized, 1);
                string[] resizeTransitions = NewlyTrimmed(resizeBaseline, resized, 1);

                _table.AutoFit();
                HeaderState[] recreateBaseline = await SettleAsync();
                Recreate();
                HeaderState[] recreated = await SettleAsync();
                RequireWidths(recreateBaseline, recreated, 0);
                await CaptureImageAsync(Path.Combine(directory, $"{name}-recreate.png"));
                string[] recreateTransitions = NewlyTrimmed(recreateBaseline, recreated, 0);

                // A deliberately narrow Pixel column is a control case. A
                // trimmed baseline is not counted as an Auto-to-Pixel failure.
                ((HeaderColumn)_table.Children[4]).DesiredWidth = new GridLength(40);
                HeaderState[] narrow = await SettleAsync();
                transitions += freezeTransitions.Length + resizeTransitions.Length + recreateTransitions.Length;
                cases.Add(new { Name = name, Viewport = viewport, Intrinsic = intrinsic, Stretch = stretch, Sort = sort, Auto = auto, Frozen = frozen, ResizeBaseline = resizeBaseline, Resized = resized, RecreateBaseline = recreateBaseline, Recreated = recreated, Narrow = narrow, FreezeTransitions = freezeTransitions, ResizeTransitions = resizeTransitions, RecreateTransitions = recreateTransitions });
                Trace.Write($"capture {name} newly-trimmed freeze={freezeTransitions.Length} resize-neighbours={resizeTransitions.Length} recreate={recreateTransitions.Length} narrow-control={narrow[4].Trimmed}");
            }
            File.WriteAllText(Path.Combine(directory, "summary.json"), JsonSerializer.Serialize(new
            {
                Status = "capture-complete",
                OS = Environment.OSVersion.ToString(),
                WindowsAppSDKPackage = "2.5.1",
                XamlAssembly = typeof(Window).Assembly.FullName,
                NativeXamlVersion = NativeXamlVersion(),
                HeaderButtonStyle = "Default WinUI Button; Padding=0, MinWidth=0. Production uses SubtleButtonStyle.",
                NewlyTrimmedTransitions = transitions,
                Boundary = "Pure WinUI candidate only; native runner DPI, no rounding override, no Toolkit, data rows or OpenNet sort-navigation regression. Zero transitions does not prove the production defect is fixed.",
                Cases = cases
            }, JsonOptions));
        }
        catch (Exception error)
        {
            exitCode = 1;
            File.WriteAllText(Path.Combine(directory, "summary.json"), JsonSerializer.Serialize(new { Status = "capture-error", Error = error.ToString(), Cases = cases }, JsonOptions));
            Trace.Write($"capture failed: {error}");
        }
        finally
        {
            File.Copy(Trace.LogPath, Path.Combine(directory, "layout.log"), true);
            Environment.Exit(exitCode);
        }
    }

    private HeaderState[] ReadStates() => _table.Children.Cast<HeaderColumn>().Select(column => column.ReadState()).ToArray();

    private async Task<HeaderState[]> SettleAsync()
    {
        // Yield to the real UI dispatcher between reads. Require two unchanged
        // intervals and a loaded tree; a delay alone cannot prove layout settled.
        HeaderState[] previous = [];
        int unchanged = 0;
        for (int attempt = 0; attempt < 40; ++attempt)
        {
            await DispatcherDelayAsync();
            HeaderState[] current = ReadStates();
            if (_table.IsLoaded && current.All(state => state.Scale > 0) && current.SequenceEqual(previous)) ++unchanged;
            else unchanged = 0;
            if (unchanged >= 2)
            {
                _table.Snapshot("capture-settled");
                return current;
            }
            previous = current;
        }
        throw new TimeoutException("Header layout did not settle on the Windows UI dispatcher.");
    }

    private static Task DispatcherDelayAsync()
    {
        TaskCompletionSource<bool> completion = new();
        DispatcherTimer timer = new() { Interval = TimeSpan.FromMilliseconds(200) };
        timer.Tick += (_, _) => { timer.Stop(); completion.SetResult(true); };
        timer.Start();
        return completion.Task;
    }

    private static void RequireWidths(HeaderState[] before, HeaderState[] after, int skip)
    {
        for (int i = skip; i < before.Length; ++i)
            if (before[i].ResolvedWidth != after[i].ResolvedWidth || before[i].ResolvedWidth != after[i].DesiredWidth)
                throw new InvalidOperationException($"Logical width changed for {before[i].Label}: {before[i].ResolvedWidth:G17} -> {after[i].ResolvedWidth:G17}/{after[i].DesiredWidth:G17}");
    }

    private static string[] NewlyTrimmed(HeaderState[] before, HeaderState[] after, int skip) => before.Skip(skip).Where((state, index) => !state.Trimmed && after[index + skip].Trimmed).Select(state => state.Label).ToArray();

    private async Task CaptureImageAsync(string path)
    {
        RenderTargetBitmap bitmap = new();
        await bitmap.RenderAsync(_host);
        if (bitmap.PixelWidth == 0 || bitmap.PixelHeight == 0) throw new InvalidOperationException("The runner did not render the header visual tree.");
        var buffer = await bitmap.GetPixelsAsync();
        byte[] pixels = new byte[buffer.Length];
        using (DataReader reader = DataReader.FromBuffer(buffer)) reader.ReadBytes(pixels);
        using InMemoryRandomAccessStream stream = new();
        BitmapEncoder encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.PngEncoderId, stream);
        encoder.SetPixelData(BitmapPixelFormat.Bgra8, BitmapAlphaMode.Premultiplied, (uint)bitmap.PixelWidth, (uint)bitmap.PixelHeight, 96, 96, pixels);
        await encoder.FlushAsync();
        byte[] png = new byte[checked((int)stream.Size)];
        using (DataReader reader = new(stream.GetInputStreamAt(0)))
        {
            await reader.LoadAsync((uint)png.Length);
            reader.ReadBytes(png);
        }
        await File.WriteAllBytesAsync(path, png);
    }

    private static string? NativeXamlVersion()
    {
        foreach (ProcessModule module in Process.GetCurrentProcess().Modules)
            if (module.ModuleName.Equals("Microsoft.UI.Xaml.dll", StringComparison.OrdinalIgnoreCase)) return module.FileVersionInfo.FileVersion;
        return null;
    }
}
