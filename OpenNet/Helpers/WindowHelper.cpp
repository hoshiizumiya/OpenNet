module;
#include <windows.h>
#include <bcrypt.h>
#include <minwindef.h>
#include <microsoft.ui.xaml.window.h>       // IWindowNative
#include <shlwapi.h> // For PathRemoveFileSpec
#include <wil/result.h>
#include <commctrl.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "bcrypt.lib")
module OpenNet.Helpers.WindowHelper;

import OpenNet.App;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Utils.Message;
import OpenNet.Helpers.ThemeHelper;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.Media.Core;
import winrt.Windows.Media.Playback;

using namespace OpenNet::Core::Utils::Message;
using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Windowing;

namespace
{
	using Brush = winrt::Microsoft::UI::Xaml::Media::Brush;
	using Image = winrt::Microsoft::UI::Xaml::Controls::Image;
	using MediaPlayerElement =
		winrt::Microsoft::UI::Xaml::Controls::MediaPlayerElement;

	struct SecondaryWindowVisualState
	{
		Brush OriginalRootBackground{ nullptr };
		Image ImagePresenter{ nullptr };
		MediaPlayerElement VideoPresenter{ nullptr };
		bool Enabled{};
	};

	std::unordered_map<HWND, SecondaryWindowVisualState>
		g_secondaryWindowVisualStates;

	bool IsSecondaryWindow(Window const& window)
	{
		auto const& mainWindow = winrt::OpenNet::implementation::App::window;
		return mainWindow && window != mainWindow;
	}

	bool ApplyWindowAppearancePolicy(Window const& window, bool const refreshBackdrop)
	{
		if (!window)
		{
			return false;
		}

		auto const isSecondary = IsSecondaryWindow(window);
		auto const applyToSecondary =
			::OpenNet::Core::AppSettingsDatabase::Instance().GetBool(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				::OpenNet::Helpers::kApplyBackgroundToSecondaryWindowsKey)
			.value_or(true);

		if (refreshBackdrop)
		{
			if (!isSecondary || applyToSecondary)
			{
				::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(window);
			}
			else
			{
				window.SystemBackdrop(nullptr);
			}
		}

		if (!isSecondary)
		{
			return false;
		}

		auto content = window.Content();
		auto root = content.try_as<winrt::Microsoft::UI::Xaml::Controls::Panel>();
		if (!root && applyToSecondary && content)
		{
			// Factory windows can host a Page directly. Wrap non-Panel content in a
			// Grid so it receives the same two presentation layers as MainWindow.
			root = winrt::Microsoft::UI::Xaml::Controls::Grid{};
			window.Content(nullptr);
			root.Children().Append(content);
			window.Content(root);
		}
		if (!root)
		{
			return false;
		}

		HWND hwnd{};
		try
		{
			hwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::
				GetWindowHandleFromWindow(window);
		}
		catch (...)
		{
			return false;
		}
		if (!hwnd)
		{
			return false;
		}

		auto [visual, inserted] = g_secondaryWindowVisualStates.try_emplace(hwnd);
		if (inserted)
		{
			visual->second.OriginalRootBackground = root.Background();
		}

		auto& state = visual->second;
		bool presenterAdded{};
		if (applyToSecondary && (!state.ImagePresenter || !state.VideoPresenter))
		{
			state.ImagePresenter = Image{};
			state.ImagePresenter.HorizontalAlignment(HorizontalAlignment::Stretch);
			state.ImagePresenter.VerticalAlignment(VerticalAlignment::Stretch);
			state.ImagePresenter.IsHitTestVisible(false);
			state.ImagePresenter.Opacity(0);
			state.ImagePresenter.UseLayoutRounding(false);
			state.ImagePresenter.Stretch(
				winrt::Microsoft::UI::Xaml::Media::Stretch::UniformToFill);

			state.VideoPresenter = MediaPlayerElement{};
			state.VideoPresenter.HorizontalAlignment(HorizontalAlignment::Stretch);
			state.VideoPresenter.VerticalAlignment(VerticalAlignment::Stretch);
			state.VideoPresenter.AreTransportControlsEnabled(false);
			state.VideoPresenter.AutoPlay(true);
			state.VideoPresenter.IsHitTestVisible(false);
			state.VideoPresenter.Opacity(0);
			state.VideoPresenter.UseLayoutRounding(false);
			state.VideoPresenter.Stretch(
				winrt::Microsoft::UI::Xaml::Media::Stretch::UniformToFill);

			winrt::Microsoft::UI::Xaml::Controls::Grid::SetRowSpan(
				state.ImagePresenter, 1000);
			winrt::Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(
				state.ImagePresenter, 1000);
			winrt::Microsoft::UI::Xaml::Controls::Grid::SetRowSpan(
				state.VideoPresenter, 1000);
			winrt::Microsoft::UI::Xaml::Controls::Grid::SetColumnSpan(
				state.VideoPresenter, 1000);

			root.Children().InsertAt(0, state.ImagePresenter);
			root.Children().InsertAt(1, state.VideoPresenter);
			presenterAdded = true;
		}

		state.Enabled = applyToSecondary;
		if (state.ImagePresenter) state.ImagePresenter.Visibility(
			applyToSecondary ? Visibility::Visible : Visibility::Collapsed);
		if (state.VideoPresenter) state.VideoPresenter.Visibility(
			applyToSecondary ? Visibility::Visible : Visibility::Collapsed);
		if (!applyToSecondary && state.ImagePresenter && state.VideoPresenter)
		{
			try
			{
				state.ImagePresenter.Opacity(0);
				state.ImagePresenter.Source(nullptr);
				if (auto player = state.VideoPresenter.MediaPlayer())
				{
					player.Pause();
					player.Source(nullptr);
				}
				state.VideoPresenter.Source(nullptr);
				state.VideoPresenter.SetMediaPlayer(nullptr);
				state.VideoPresenter.Opacity(0);
			}
			catch (...)
			{
			}
		}

		auto const backgroundType = std::clamp(static_cast<int>(
			::OpenNet::Core::AppSettingsDatabase::Instance().GetInt(
				::OpenNet::Core::AppSettingsDatabase::CAT_UI,
				"background_type", 1)), 0, 4);

		// Opaque root brushes hide Mica/Acrylic. Preserve each original brush so
		// disabling this option or selecting Static restores the XAML appearance.
		if (applyToSecondary && backgroundType != 0)
		{
			root.Background(nullptr);
		}
		else
		{
			root.Background(state.OriginalRootBackground);
		}

		return presenterAdded;
	}
}

namespace OpenNet::Helpers::WinUIWindowHelper
{
	Window WindowHelper::CreateHostWindow()
	{
		Window newWindow = Window();
		TrackWindow(newWindow);
		return newWindow;
	}

	void WindowHelper::TrackWindow(Window const& window)
	{
		if (!window)
		{
			return;
		}

		// Window creation is spread across several feature areas. Make tracking the
		// single point that restores both theme and backdrop settings, and tolerate
		// callers that track a specialized window more than once.
		if (std::find(m_activeWindows.begin(), m_activeWindows.end(), window)
			!= m_activeWindows.end())
		{
			return;
		}

		m_activeWindows.push_back(window);
		auto const presenterAdded = ApplyWindowAppearancePolicy(window, true);
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);
		if (presenterAdded)
		{
			m_backgroundPresentersChanged(nullptr, nullptr);
		}

		window.Activated([](IInspectable const& sender, WindowActivatedEventArgs const& args)
		{
			if (args.WindowActivationState() == WindowActivationState::Deactivated)
			{
				return;
			}
			if (auto activatedWindow = sender.try_as<Window>())
			{
				// Content may be assigned after TrackWindow (for factory-created
				// windows), so apply root transparency and theme after activation.
				if (ApplyWindowAppearancePolicy(activatedWindow, false))
				{
					WindowHelper::m_backgroundPresentersChanged(nullptr, nullptr);
				}
				::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(activatedWindow);
			}
		});

		HWND trackedHwnd{};
		try
		{
			trackedHwnd = GetWindowHandleFromWindow(window);
		}
		catch (...)
		{
		}

		window.Closed([trackedHwnd](winrt::Windows::Foundation::IInspectable const& sender, WindowEventArgs const& /*args*/)
		{
			auto closedWindow = sender.try_as<Window>();
			if (closedWindow)
			{
				if (trackedHwnd)
				{
					auto const visual = g_secondaryWindowVisualStates.find(trackedHwnd);
					if (visual != g_secondaryWindowVisualStates.end())
					{
						try
						{
							visual->second.ImagePresenter.Source(nullptr);
							if (auto player = visual->second.VideoPresenter.MediaPlayer())
							{
								player.Pause();
								player.Source(nullptr);
							}
							visual->second.VideoPresenter.Source(nullptr);
							visual->second.VideoPresenter.SetMediaPlayer(nullptr);
						}
						catch (...)
						{
						}
						g_secondaryWindowVisualStates.erase(visual);
					}
				}
				auto it = std::remove(m_activeWindows.begin(), m_activeWindows.end(), closedWindow);
				m_activeWindows.erase(it, m_activeWindows.end());
				WindowHelper::m_backgroundPresentersChanged(nullptr, nullptr);
			}
		});
	}

	void WindowHelper::RefreshWindowAppearances()
	{
		for (auto const& window : m_activeWindows)
		{
			ApplyWindowAppearancePolicy(window, true);
			::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(window);
		}
		m_backgroundPresentersChanged(nullptr, nullptr);
	}

	std::vector<WindowBackgroundPresenters>
		WindowHelper::SecondaryBackgroundPresenters()
	{
		std::vector<WindowBackgroundPresenters> presenters;
		presenters.reserve(g_secondaryWindowVisualStates.size());
		for (auto const& [hwnd, state] : g_secondaryWindowVisualStates)
		{
			if (state.Enabled && state.ImagePresenter && state.VideoPresenter)
			{
				presenters.push_back({ state.ImagePresenter, state.VideoPresenter });
			}
		}
		return presenters;
	}

	winrt::event_token WindowHelper::BackgroundPresentersChanged(
		winrt::Windows::Foundation::EventHandler<
		winrt::Windows::Foundation::IInspectable> const& handler)
	{
		return m_backgroundPresentersChanged.add(handler);
	}

	void WindowHelper::BackgroundPresentersChanged(
		winrt::event_token const& token) noexcept
	{
		m_backgroundPresentersChanged.remove(token);
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

	void WindowHelper::ShowMainWindow()
	{
		if (winrt::OpenNet::implementation::App::s_isExiting.load())
		{
			return;
		}

		auto& window = winrt::OpenNet::implementation::App::window;
		if (window)
		{
			try
			{
				HWND hwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(window);
				if (!hwnd || !IsWindow(hwnd))
				{
					return;
				}

				window.AppWindow().Show();

				if (IsIconic(hwnd))
				{
					ShowWindow(hwnd, SW_RESTORE);
				}
				SetForegroundWindow(hwnd);
				SetFocus(hwnd);
			}
			catch (...)
			{
				// A tray callback can race with AppWindow destruction. Treat a
				// closed window as a no-op instead of escaping a user callback.
			}
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

		auto putPersist = reinterpret_cast<put_PersistedStateId_t>(vtbl[putPersistIndex]);
		auto putBehavior = reinterpret_cast<put_PlacementRestorationBehavior_t>(vtbl[putBehaviorIndex]);

		// 设置行为
		putBehavior(experimentalRaw, 0xFFFFFFFF);

		auto typeName = winrt::get_class_name(window);
		auto persistGuid = GenerateTypeGuid(typeName);

		winrt::Windows::Foundation::IInspectable boxed = winrt::box_value(persistGuid);

		putPersist(experimentalRaw, winrt::get_abi(boxed));


		reinterpret_cast<::IUnknown*>(experimentalRaw)->Release();
	}

	void PlacementRestoration::Save(winrt::Microsoft::UI::Xaml::Window const& window)
	{
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

		auto vtbl = *reinterpret_cast<void***>(experimentalRaw);

		constexpr size_t saveIndex = 0x58 / sizeof(void*);

		auto savePlacement =
			reinterpret_cast<SaveCurrentPlacement_t>(vtbl[saveIndex]);

		savePlacement(experimentalRaw);

		reinterpret_cast<::IUnknown*>(experimentalRaw)->Release();
	}


}
