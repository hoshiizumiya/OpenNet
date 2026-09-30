using Microsoft.UI.Xaml;

namespace DataTableHeaderTrimming;

public partial class App : Application
{
    private Window? _window;
    public App() => InitializeComponent();
    protected override async void OnLaunched(LaunchActivatedEventArgs args)
    {
        MainWindow window = new();
        _window = window;
        _window.Activate();
        string? captureDirectory = Environment.GetEnvironmentVariable("ON_DTH_CAPTURE_DIRECTORY");
        if (!string.IsNullOrWhiteSpace(captureDirectory)) await window.CaptureAsync(captureDirectory);
    }
}
