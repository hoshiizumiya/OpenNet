#include "pch.h"

#include "Core/AppSettingsDatabase.h"

#include "ThemeHelper.h"
#include "WindowHelper.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Microsoft.Windows.Storage.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Microsoft.UI.Windowing.h>

using namespace winrt;
using namespace winrt::Microsoft::UI;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::Windows::Storage;
using namespace winrt::Windows::UI::ViewManagement;

namespace OpenNet::Helpers
{
	// Static member initialization
	ElementTheme ThemeHelper::s_rootTheme = ElementTheme::Default;
	event_token ThemeHelper::s_actualThemeChangedToken{};

	ElementTheme ThemeHelper::RootTheme()
	{
		return s_rootTheme;
	}

	void ThemeHelper::RootTheme(ElementTheme value)
	{
		s_rootTheme = value;
		SaveThemeToSettings();
	}

	ElementTheme ThemeHelper::ActualTheme()
	{
		if (s_rootTheme == ElementTheme::Default)
		{
			return GetSystemTheme();
		}
		return s_rootTheme;
	}

	bool ThemeHelper::IsDarkTheme()
	{
		return ActualTheme() == ElementTheme::Dark;
	}

	void ThemeHelper::Initialize()
	{
		s_rootTheme = LoadThemeFromSettings();

		try
		{
			UISettings uiSettings;
			s_actualThemeChangedToken = uiSettings.ColorValuesChanged([](auto&&, auto&&)
				{
					if (s_rootTheme == ElementTheme::Default)
					{
						// System theme changed
					}
				});
		}
		catch (...)
		{
		}
	}

	void ThemeHelper::UpdateThemeForWindow(Window const& window)
	{
		if (!window) return;

		try
		{
			auto content = window.Content();
			if (!content) return;

			auto rootElement = content.try_as<FrameworkElement>();
			if (!rootElement) return;

			rootElement.RequestedTheme(s_rootTheme);
			UpdateTitleBarTheme(window);
		}
		catch (...)
		{
		}
	}

	void ThemeHelper::SaveThemeToSettings()
	{
		try
		{
			auto localSettings = ApplicationData::GetDefault().LocalSettings();
			auto values = localSettings.Values();
			int32_t themeValue = static_cast<int32_t>(s_rootTheme);
			values.Insert(THEME_SETTING_KEY, box_value(themeValue));
		}
		catch (...)
		{
		}
	}

	ElementTheme ThemeHelper::LoadThemeFromSettings()
	{
		try
		{
			auto localSettings = ApplicationData::GetDefault().LocalSettings();
			auto values = localSettings.Values();

			if (values.HasKey(THEME_SETTING_KEY))
			{
				auto value = values.Lookup(THEME_SETTING_KEY);
				int32_t themeValue = unbox_value<int32_t>(value);
				return static_cast<ElementTheme>(themeValue);
			}
		}
		catch (...)
		{
		}

		return ElementTheme::Default;
	}

	hstring ThemeHelper::ThemeToString(ElementTheme theme)
	{
		switch (theme)
		{
		case ElementTheme::Light: return L"Light";
		case ElementTheme::Dark: return L"Dark";
		case ElementTheme::Default:
		default: return L"Default";
		}
	}

	ElementTheme ThemeHelper::StringToTheme(hstring const& themeString)
	{
		if (themeString == L"Light") return ElementTheme::Light;
		else if (themeString == L"Dark") return ElementTheme::Dark;
		else return ElementTheme::Default;
	}

	constexpr auto kBackdropFallbackColorKey = "backdrop_fallback_color";
	constexpr auto kBackdropTintColorKey = "backdrop_tint_color";
	constexpr auto kBackdropLuminosityOpacityKey = "backdrop_luminosity_opacity";
	constexpr auto kBackdropTintOpacityKey = "backdrop_tint_opacity";
	constexpr auto kBackdropEnableWhenInactiveKey = "backdrop_enable_when_inactive";
	constexpr auto kBackdropUseFallbackKey = "backdrop_use_fallback";

	void ThemeHelper::ApplyBackdropFromSettings()
	{
	}

	ElementTheme ThemeHelper::GetSystemTheme()
	{
		try
		{
			UISettings uiSettings;
			auto background = uiSettings.GetColorValue(UIColorType::Background);
			int luminance = background.R + background.G + background.B;
			return (luminance < 384) ? ElementTheme::Dark : ElementTheme::Light;
		}
		catch (...)
		{
			return ElementTheme::Light;
		}
	}

	void ThemeHelper::UpdateTitleBarTheme(Window const& window)
	{
		try
		{
			auto appWindow = window.AppWindow();
			if (!appWindow) return;

			auto titleBar = appWindow.TitleBar();
			if (!titleBar) return;

			bool isDark = IsDarkTheme();

			Windows::UI::Color buttonForeground, buttonBackground;
			Windows::UI::Color buttonHoverForeground, buttonHoverBackground;
			Windows::UI::Color buttonPressedForeground, buttonPressedBackground;

			if (isDark)
			{
				buttonForeground = Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255);
				buttonBackground = Windows::UI::ColorHelper::FromArgb(0, 255, 255, 255);
				buttonHoverForeground = Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255);
				buttonHoverBackground = Windows::UI::ColorHelper::FromArgb(20, 255, 255, 255);
				buttonPressedForeground = Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255);
				buttonPressedBackground = Windows::UI::ColorHelper::FromArgb(30, 255, 255, 255);
			}
			else
			{
				buttonForeground = Windows::UI::ColorHelper::FromArgb(255, 0, 0, 0);
				buttonBackground = Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0);
				buttonHoverForeground = Windows::UI::ColorHelper::FromArgb(255, 0, 0, 0);
				buttonHoverBackground = Windows::UI::ColorHelper::FromArgb(20, 0, 0, 0);
				buttonPressedForeground = Windows::UI::ColorHelper::FromArgb(255, 0, 0, 0);
				buttonPressedBackground = Windows::UI::ColorHelper::FromArgb(30, 0, 0, 0);
			}

			titleBar.ButtonForegroundColor(buttonForeground);
			titleBar.ButtonBackgroundColor(buttonBackground);
			titleBar.ButtonHoverForegroundColor(buttonHoverForeground);
			titleBar.ButtonHoverBackgroundColor(buttonHoverBackground);
			titleBar.ButtonPressedForegroundColor(buttonPressedForeground);
			titleBar.ButtonPressedBackgroundColor(buttonPressedBackground);
		}
		catch (...)
		{
		}
	}

}

