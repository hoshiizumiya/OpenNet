#include "pch.h"
#include "WindowHelper.h"

#include <shlwapi.h> // For PathRemoveFileSpec
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")  // For SetWindowSubclass

#include <commctrl.h>  // For SetWindowSubclass

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Windowing;

namespace OpenNet::Helpers::WinUIWindowHelper
{
	// 窗口子类化回调函数 / Window Subclass Callback
	LRESULT CALLBACK WindowMinSizeSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		if (uMsg == WM_GETMINMAXINFO)
		{
			LPMINMAXINFO lpMMI = reinterpret_cast<LPMINMAXINFO>(lParam);

			// 从窗口属性中获取最小尺寸
			HANDLE minWidthHandle = GetPropW(hWnd, L"MinWidth");
			HANDLE minHeightHandle = GetPropW(hWnd, L"MinHeight");

			if (minWidthHandle && minHeightHandle)
			{
				int minWidth = static_cast<int>(reinterpret_cast<INT_PTR>(minWidthHandle));
				int minHeight = static_cast<int>(reinterpret_cast<INT_PTR>(minHeightHandle));

				lpMMI->ptMinTrackSize.x = minWidth;
				lpMMI->ptMinTrackSize.y = minHeight;
			}
		}
		else if (uMsg == WM_NCDESTROY)
		{
			// 清理：移除子类化
			RemoveWindowSubclass(hWnd, WindowMinSizeSubclassProc, uIdSubclass);
			RemovePropW(hWnd, L"MinWidth");
			RemovePropW(hWnd, L"MinHeight");
		}

		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	Window WindowHelper::CreateHostWindow()
	{
		Window newWindow = Window();

		m_activeWindows.push_back(newWindow);

		auto appWindow = GetAppWindow(newWindow);

		newWindow.Closed([](winrt::Windows::Foundation::IInspectable const& sender, WindowEventArgs const&)
			{
				auto closedWindow = sender.try_as<Window>();
				auto it = std::remove(m_activeWindows.begin(), m_activeWindows.end(), closedWindow);
				m_activeWindows.erase(it, m_activeWindows.end());
			});

		return newWindow;
	}

	void WindowHelper::TrackWindow(Window const& window)
	{
		m_activeWindows.push_back(window);

		auto appWindow = GetAppWindow(window);

		window.Closed([](winrt::Windows::Foundation::IInspectable const& sender, WindowEventArgs const& /*args*/)
			{
				auto closedWindow = sender.try_as<Window>();
				if (closedWindow)
				{
					auto it = std::remove(m_activeWindows.begin(), m_activeWindows.end(), closedWindow);
					m_activeWindows.erase(it, m_activeWindows.end());
				}
			});
	}

	Window WindowHelper::GetWindowForElement(UIElement const& element)
	{
		if (!element || !element.XamlRoot()) return nullptr;

		for (auto const& window : m_activeWindows)
		{
			if (window.Content() && window.Content().XamlRoot() == element.XamlRoot())
			{
				return window;
			}
		}
		return nullptr;
	}

	HWND WinUIWindowHelper::WindowHelper::GetNativeWindowHandleForElement(winrt::Microsoft::UI::Xaml::UIElement const& element)
	{
		auto window = GetWindowForElement(element);
		if (window)
		{
			HWND hwnd{ nullptr };
			winrt::com_ptr<IWindowNative> windowNative = window.as<IWindowNative>();
			if (windowNative) windowNative->get_WindowHandle(&hwnd);
			return hwnd;
		}
		return nullptr;
	}

	double WindowHelper::GetRasterizationScaleForElement(UIElement const& element)
	{
		if (!element || !element.XamlRoot()) return 0.0;

		for (auto const& window : m_activeWindows)
		{
			if (window.Content() && window.Content().XamlRoot() == element.XamlRoot())
			{
				return element.XamlRoot().RasterizationScale();
			}
		}
		return 0.0;
	}

	std::vector<Window> const& WindowHelper::ActiveWindows()
	{
		return m_activeWindows;
	}

	winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFolder>
		WindowHelper::GetAppLocalFolderAsync()
	{
		if (!Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent(L"Windows.Foundation.UniversalApiContract", 1))
		{
			co_return ApplicationData::Current().LocalFolder();
		}
		else
		{
			WCHAR exePath[MAX_PATH]{};
			GetModuleFileNameW(nullptr, exePath, MAX_PATH);
			PathRemoveFileSpecW(exePath);

			auto&& folder = co_await StorageFolder::GetFolderFromPathAsync(exePath);
			co_return folder;
		}
	}

	// Need more investegation, this function is not working as expected. We now use essential lib to set the window size.
	void WindowHelper::SetWindowMinSize2(Window const& window, double const& width, double const& height)
	{
		auto windowContent = window.Content().try_as<FrameworkElement>();
		//OverlappedPresenter presenter = OverlappedPresenter::Create();
		OverlappedPresenter presenter = window.AppWindow().Presenter().as<OverlappedPresenter>();
		if (window.Content() != windowContent)
		{
			OutputDebugString(L"Window content is not a FrameworkElement.");
			return;
		}

		if (windowContent.XamlRoot() == nullptr)
		{
			OutputDebugString(L"Window content's XamlRoot is null.");
			return;
		}
		if (window.AppWindow().Presenter() != presenter)
		{
			OutputDebugString(L"Window's AppWindow.Presenter is not an OverlappedPresenter.");
			return;
		}
		auto scale = GetXamlRootContent(window).XamlRoot().RasterizationScale();
		double minWidth = width * scale;
		double minHeight = height * scale;
		presenter.PreferredMinimumHeight() = (int)minHeight;
		presenter.PreferredMinimumWidth() = (int)minWidth;

		// GetAppWindow(window).SetPresenter(presenter);
	}

	// Win32 version - Set Window Minimum Size
	void WindowHelper::SetWindowMinSize(Window const& window, double const& width, double const& height)
	{
		auto windowContent = window.Content().try_as<FrameworkElement>();
		if (!windowContent)
		{
			OutputDebugString(L"Window content is not a FrameworkElement.\n");
			return;
		}

		if (!windowContent.XamlRoot())
		{
			OutputDebugString(L"Window content's XamlRoot is null.\n");
			return;
		}

		auto scale = windowContent.XamlRoot().RasterizationScale();
		int minWidth = static_cast<int>(width * scale);
		int minHeight = static_cast<int>(height * scale);

		HWND hwnd = GetWindowHandleFromWindow(window);
		if (!hwnd)
		{
			OutputDebugString(L"Failed to get HWND from window.\n");
			return;
		}

		// 存储最小尺寸到窗口属性
		SetPropW(hwnd, L"MinWidth", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(minWidth)));
		SetPropW(hwnd, L"MinHeight", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(minHeight)));

		// 子类化窗口以处理 WM_GETMINMAXINFO 消息
		SetWindowSubclass(hwnd, WindowMinSizeSubclassProc, 0, 0);
#ifdef _DEBUG
		std::wstring debugMsg = L"Set minimum size: " + std::to_wstring(minWidth) + L"x" + std::to_wstring(minHeight) + L"\n";
		OutputDebugString(debugMsg.c_str());

#endif

	}

}