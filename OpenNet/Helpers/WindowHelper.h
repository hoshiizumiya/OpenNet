#pragma once

#ifndef WINDOWHELPER_H
#define WINDOWHELPER_H

#include <vector>
#include <minwindef.h>
#include <winrt/base.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.h>
#include <microsoft.ui.xaml.window.h>       // IWindowNative
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.ui.interop.h>     // GetWindowIdFromWindow, don't use `microsoft.ui.interop.h` directly
#include <winrt/Microsoft.UI.Content.h>  // for ContentIslandEnvironment
#include <winrt/Windows.Foundation.Metadata.h>

namespace MicrosoftDocsGallery::Helpers::WinUIWindowHelper
{
    struct WindowHelper
    {
        // 获取 HWND
        static HWND GetWindowHandleFromWindow(winrt::Microsoft::UI::Xaml::Window const& window)
        {
            HWND hwnd{ nullptr };
            winrt::com_ptr<IWindowNative> windowNative = window.as<IWindowNative>();
            if (windowNative) windowNative->get_WindowHandle(&hwnd);
            return hwnd;
        }

        // 获取 AppWindow
        // 对于桌面应用程序，CoreWindow 属性已经被弃用，始终设置为 nullptr，
        // 应用程序应该使用 IWindowNative 获取底层 HWND，并由此获取 AppWindow 的引用
        static winrt::Microsoft::UI::Windowing::AppWindow GetAppWindow(winrt::Microsoft::UI::Xaml::Window const& window)
        {
            HWND hwnd = GetWindowHandleFromWindow(window);
            auto windowId = winrt::Microsoft::UI::GetWindowIdFromWindow(hwnd);
            return winrt::Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);
        }

        static winrt::Microsoft::UI::Windowing::AppWindow GetAppWindow(HWND const& windowHandle)
        {
            if (windowHandle == nullptr || !IsWindow(windowHandle))
                return nullptr;
            auto windowId = winrt::Microsoft::UI::GetWindowIdFromWindow(windowHandle);
            return winrt::Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);
        }

        static HWND GetWindowHandleFromWindowId(winrt::Microsoft::UI::WindowId const& windowIdRef)
        {
            auto hWnd = winrt::Microsoft::UI::GetWindowFromWindowId(windowIdRef);
            return hWnd;
        }

        // 获取 XAML 根元素（通常是 Grid/Page）
        static winrt::Microsoft::UI::Xaml::FrameworkElement GetXamlRootContent(winrt::Microsoft::UI::Xaml::Window const& window)
        {
            return window.Content().try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>();
        }

        static winrt::Microsoft::UI::Windowing::AppWindow GetAppWindowForElement(winrt::Microsoft::UI::Xaml::UIElement const& element)
        {
            auto xr = element.XamlRoot();
            auto env = xr.ContentIslandEnvironment();
            return winrt::Microsoft::UI::Windowing::AppWindow::GetFromWindowId(env.AppWindowId());
        }

        static winrt::Microsoft::UI::Xaml::Window CreateHostWindow();
        static void TrackWindow(winrt::Microsoft::UI::Xaml::Window const& window);
        static winrt::Microsoft::UI::Xaml::Window GetWindowForElement(winrt::Microsoft::UI::Xaml::UIElement const& element);
        static HWND GetNativeWindowHandleForElement(winrt::Microsoft::UI::Xaml::UIElement const& element);
        static double GetRasterizationScaleForElement(winrt::Microsoft::UI::Xaml::UIElement const& element);
        static std::vector<winrt::Microsoft::UI::Xaml::Window> const& ActiveWindows();
        static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFolder>
            GetAppLocalFolderAsync();
    private:
        static inline std::vector<winrt::Microsoft::UI::Xaml::Window> m_activeWindows;
    };

}

#endif // WINDOWHELPER_H