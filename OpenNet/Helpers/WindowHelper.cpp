#include "pch.h"
#include "WindowHelper.h"

#include <shlwapi.h> // For PathRemoveFileSpec
#pragma comment(lib, "shlwapi.lib")

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Windowing;

namespace OpenNet::Helpers::WinUIWindowHelper
{
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
	void WindowHelper::SetWindowMinSize(Window const& window, double const& width, double const& height)
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
		presenter.PreferredMinimumHeight() = (int)minWidth;
		presenter.PreferredMinimumWidth() = (int)minHeight;

		GetAppWindow(window).SetPresenter(presenter);
	}

}