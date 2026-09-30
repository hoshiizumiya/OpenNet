using Microsoft.UI.Xaml;
using System.Text.Json;

namespace DataTableHeaderTrimming;

public partial class App : Application
{
    private Window? _window;
    public App()
    {
        UnhandledException += (_, args) =>
        {
            RecordStartupError(args.Exception, args.Message);
            args.Handled = true;
            Environment.Exit(1);
        };
        Trace.Write("Application constructor entered");
        DebugSettings.IsXamlResourceReferenceTracingEnabled = true;
        DebugSettings.XamlResourceReferenceFailed += (_, args) => Trace.Write($"resource reference failed: {args.Message}");
        InitializeComponent();
    }
    protected override async void OnLaunched(LaunchActivatedEventArgs args)
    {
        try
        {
            Trace.Write("OnLaunched entered");
            if (!string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("ON_DTH_CAPTURE_DIRECTORY"))) ProbeHeaderMarkup();
            MainWindow window = new();
            _window = window;
            _window.Activate();
            string? captureDirectory = Environment.GetEnvironmentVariable("ON_DTH_CAPTURE_DIRECTORY");
            if (!string.IsNullOrWhiteSpace(captureDirectory)) await window.CaptureAsync(captureDirectory);
        }
        catch (Exception error)
        {
            RecordStartupError(error, "OnLaunched");
            Environment.Exit(1);
        }
    }

    private static void RecordStartupError(Exception error, string context)
    {
        Trace.Write($"startup error {context}: {error}");
        string? directory = Environment.GetEnvironmentVariable("ON_DTH_CAPTURE_DIRECTORY");
        if (string.IsNullOrWhiteSpace(directory)) return;
        Directory.CreateDirectory(directory);
        File.WriteAllText(Path.Combine(directory, "summary.json"), JsonSerializer.Serialize(new { Status = "startup-error", Context = context, Error = error.ToString() }, new JsonSerializerOptions { WriteIndented = true }));
        File.Copy(Trace.LogPath, Path.Combine(directory, "layout.log"), true);
    }

    private static void ProbeHeaderMarkup()
    {
        foreach (var probe in new (string Name, Func<object> Create)[]
        {
            ("SubtleButtonStyle resource", () => Current.Resources["SubtleButtonStyle"]),
            ("ContentPresenter content", () => Microsoft.UI.Xaml.Markup.XamlReader.Load("<ContentPresenter xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' HorizontalAlignment='Stretch'><Button Padding='0' MinWidth='0'/></ContentPresenter>")),
            ("Thumb template", () => Microsoft.UI.Xaml.Markup.XamlReader.Load("<Thumb xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' Width='8'><Thumb.Template><ControlTemplate TargetType='Thumb'><Border Background='Transparent'/></ControlTemplate></Thumb.Template></Thumb>"))
        })
        {
            try { object value = probe.Create(); Trace.Write($"markup probe {probe.Name}: created {value.GetType().FullName}"); }
            catch (Exception error) { Trace.Write($"markup probe {probe.Name}: failed {error}"); }
        }
    }
}
