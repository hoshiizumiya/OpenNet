#include "pch.h"
#include "App.xaml.h"
#include "WindowHelper.h"
#include "Core/Utils/Message.h"

#include <shlwapi.h> // For PathRemoveFileSpec
#include <wil/result.h>
#include <commctrl.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "bcrypt.lib")

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Windowing;
using namespace Core::Utils::Message;

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

	winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFolder> WindowHelper::GetAppLocalFolderAsync()
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

	void WindowHelper::SetWindowMinSize(Window const& window, double const& width, double const& height)
	{
		auto windowContent = window.Content().try_as<FrameworkElement>();
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

		UINT dpi = GetDpiForWindow(GetWindowHandleFromWindow(window));
		THROW_HR_IF(E_INVALIDARG, 0 == dpi);
		auto scale = std::floor((dpi * 100.0f / 96.0f) + 0.5f) / 100.0f;
		presenter.try_as<OverlappedPresenter>().PreferredMinimumHeight(static_cast<int32_t>(width * scale));
		presenter.try_as<OverlappedPresenter>().PreferredMinimumWidth(static_cast<int32_t>(height * scale));
	}
	/*
	void WindowHelper::AddNotifyIcon()
	{

		hstring appname = ResourceGetString(const_cast<wchar_t*>(L"NotifyIconName"));
		guid gNotifyIcon("21a2acbc-3a44-43c8-860a-f8e7151b2623");
		NOTIFYICONDATAW nid = {};
		nid.cbSize = sizeof(NOTIFYICONDATAW);
		nid.hWnd = GetWindowHandle();
		nid.uID = 0;
		nid.guidItem = gNotifyIcon;
		nid.hBalloonIcon = 0;
		nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_SHOWTIP | NIF_TIP | NIF_GUID | NIF_STATE;
		nid.uCallbackMessage = NotifyIconCallbackMessage;
		nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
		wcscpy_s(nid.szTip, appname.c_str());
		if (Shell_NotifyIconW(NIM_ADD, &nid))
		{
			Shell_NotifyIconW(NIM_SETVERSION, &nid);
		}
	}
	*/

	guid PlacementRestoration::GenerateTypeGuid(hstring const& typeName)
	{
		std::wstring_view name = typeName;

		BCRYPT_ALG_HANDLE hAlg{};
		BCRYPT_HASH_HANDLE hHash{};
		DWORD hashLen{};
		DWORD cbData{};

		check_hresult(BCryptOpenAlgorithmProvider(
			&hAlg,
			BCRYPT_MD5_ALGORITHM,
			nullptr,
			0));

		check_hresult(BCryptGetProperty(
			hAlg,
			BCRYPT_HASH_LENGTH,
			reinterpret_cast<PUCHAR>(&hashLen),
			sizeof(hashLen),
			&cbData,
			0));

		std::vector<uint8_t> hash(hashLen);

		check_hresult(BCryptCreateHash(
			hAlg,
			&hHash,
			nullptr,
			0,
			nullptr,
			0,
			0));

		check_hresult(BCryptHashData(
			hHash,
			reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(name.data())),
			static_cast<ULONG>(name.size() * sizeof(wchar_t)),
			0));

		check_hresult(BCryptFinishHash(
			hHash,
			hash.data(),
			hashLen,
			0));

		BCryptDestroyHash(hHash);
		BCryptCloseAlgorithmProvider(hAlg, 0);

		return guid{
			*reinterpret_cast<GUID*>(hash.data())
		};
	}

	void PlacementRestoration::Enable(winrt::Microsoft::UI::Xaml::Window const& window)
	{
		using put_PersistedStateId_t = HRESULT(__stdcall*)(void*, void*);

		using put_PlacementRestorationBehavior_t = HRESULT(__stdcall*)(void*, uint32_t);

		using SaveCurrentPlacement_t = HRESULT(__stdcall*)(void*);

		static const winrt::guid iidExperimental
		{
			L"{04DB96C7-DEB6-5BE4-BFDC-1BC0361C8A12}"
		};

		auto appWindow = window.AppWindow();
		auto unk = appWindow.try_as<::IUnknown>();
		if (!unk)
			return;

		void* experimentalRaw{};
		if (FAILED(unk->QueryInterface(
			reinterpret_cast<IID const&>(iidExperimental),
			&experimentalRaw)))
			return;

		// 获取 vtable
		auto vtbl = *reinterpret_cast<void***>(experimentalRaw);

		// 计算 index
		constexpr size_t putPersistIndex = 0x38 / sizeof(void*);
		constexpr size_t putBehaviorIndex = 0x48 / sizeof(void*);
		constexpr size_t saveIndex = 0x58 / sizeof(void*);

		auto putPersist = reinterpret_cast<put_PersistedStateId_t>(vtbl[putPersistIndex]);

		auto putBehavior = reinterpret_cast<put_PlacementRestorationBehavior_t>(vtbl[putBehaviorIndex]);

		auto savePlacement = reinterpret_cast<SaveCurrentPlacement_t>(vtbl[saveIndex]);

		// 设置行为
		putBehavior(experimentalRaw, 0xFFFFFFFF);

		auto typeName = winrt::get_class_name(window);
		auto persistGuid = GenerateTypeGuid(typeName);

		winrt::Windows::Foundation::IInspectable boxed = winrt::box_value(persistGuid);

		putPersist(experimentalRaw, winrt::get_abi(boxed));

		// 立即保存
		savePlacement(experimentalRaw);

		reinterpret_cast<::IUnknown*>(experimentalRaw)->Release();
	}


}