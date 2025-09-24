#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Windowing;

namespace winrt::OpenNet::implementation
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    /// <summary>
    /// Invoked when the application is launched.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        window = make<MainWindow>();
        window.Activate();

        // Language Hot-Reload support
        auto supportedLanguages = Windows::Globalization::ApplicationLanguages::Languages();

        // 构建输出字符串
        std::wstring out;
        out.reserve(256);
        out += L"Supported languages:\n";
        for (auto const& lang : supportedLanguages)
        {
            // lang 是 winrt::hstring，可以通过 c_str() 获取 wchar_t*
            out += std::wstring(lang.c_str());
            out += L"\n";
        }

        // 输出到调试输出（Visual Studio 的输出窗口）
        OutputDebugStringW(out.c_str());
    }
}
