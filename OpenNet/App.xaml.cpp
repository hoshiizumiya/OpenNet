#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "UI/Shell/NotifyIconContextMenu.xaml.h"
#include "Helpers/WindowHelper.h"
#include "Helpers/ThemeHelper.h"
#include "Core/P2PManager.h"

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.Storage.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Windowing;
using namespace winrt::Microsoft::Windows::AppLifecycle;

namespace winrt::OpenNet::implementation
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
        // Initialize theme system early
        ::OpenNet::Helpers::ThemeHelper::Initialize();

        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                OutputDebugStringW((L"UnhandledException: " + std::wstring(errorMessage.c_str()) + L"\r\n").c_str());
                __debugbreak();
            }
        });
#endif
    }

    /// <summary>
    /// Invoked when the application is launched.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched([[maybe_unused]] Microsoft::UI::Xaml::LaunchActivatedEventArgs const& e)
    {
        auto icon = make<OpenNet::UI::Shell::implementation::NotifyIconContextMenu>();
        // 创建窗口（首次启动）
        window = make<MainWindow>();
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
        
        // Apply saved theme to the window
        ::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);
        
        // 注册窗口关闭事件以清理资源
        window.Closed([](const auto& sender, const auto& args)
        {
            // 在窗口完全关闭前进行清理
            OutputDebugStringA("App: MainWindow closed event triggered\n");
        });
        
        window.Activate();

#if _DEBUG
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
        OutputDebugStringW(out.c_str());

#endif
        // 处理初始激活参数（例如命令行参数、文件打开等）
        auto activationArgs = AppInstance::GetCurrent().GetActivatedEventArgs();
        HandleActivation(activationArgs);
    }

    // To do：自定义激活，Windows集成
    void App::HandleActivation(winrt::Microsoft::Windows::AppLifecycle::AppActivationArguments const& args)
    {
        // 检查窗口是否存在
        if (!window)
        {
            // 如果窗口不存在，创建新窗口
            // 这种情况发生在重新激活时窗口已关闭的情况
            window = make<MainWindow>();
            ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
            
            // Apply theme to new window
            ::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);
        }

        // 获取窗口句柄
        HWND hwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(window);
        if (!hwnd)
        {
            return;
        }

        // 如果窗口被最小化，恢复它
        if (IsIconic(hwnd))
        {
            ShowWindow(hwnd, SW_RESTORE);
        }

        // 显示并激活窗口
        window.AppWindow().Show();
        
        // 将窗口置于前台
        SetForegroundWindow(hwnd);
        
        // 确保窗口获得焦点
        SetFocus(hwnd);

        // 根据激活类型处理不同的激活参数
        ExtendedActivationKind kind = args.Kind();
        
        if (kind == ExtendedActivationKind::Launch)
        {
            // 处理启动激活（包括命令行参数）
            auto launchArgs = args.Data().try_as<winrt::Windows::ApplicationModel::Activation::ILaunchActivatedEventArgs>();
            if (launchArgs)
            {
                // 在这里处理命令行参数
                // auto cmdLineArgs = launchArgs.Arguments();
                // OutputDebugStringW((L"Launch args: " + std::wstring(cmdLineArgs.c_str()) + L"\n").c_str());
            }
        }
        else if (kind == ExtendedActivationKind::File)
        {
            // 处理文件激活
            auto fileArgs = args.Data().try_as<winrt::Windows::ApplicationModel::Activation::IFileActivatedEventArgs>();
            if (fileArgs)
            {
                // 可以在这里处理打开的文件
                // auto files = fileArgs.Files();
                // for (auto const& file : files)
                // {
                //     auto storageFile = file.try_as<winrt::Windows::Storage::IStorageFile>();
                //     // 处理文件
                // }
            }
            
            // 文件激活时闪烁提示
            FLASHWINFO fw = {};
            fw.cbSize = sizeof(FLASHWINFO);
            fw.hwnd = hwnd;
            fw.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
            fw.uCount = 3;
            fw.dwTimeout = 0;
            FlashWindowEx(&fw);
        }
        // 可以添加更多激活类型的处理
    }

    App::~App()
    {
        // 在应用销毁时清理P2PManager资源
        try
        {
            OutputDebugStringA("App: Destructor called, cleaning up P2PManager...\n");
            ::OpenNet::Core::P2PManager::Instance().Shutdown();
            OutputDebugStringA("App: Destructor completed\n");
        }
        catch (const std::exception& ex)
        {
            OutputDebugStringW((L"App: Cleanup error in destructor: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
        }
        catch (...)
        {
            OutputDebugStringA("App: Unknown error in destructor\n");
        }
    }

}
