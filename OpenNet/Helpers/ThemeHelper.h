#pragma once

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace OpenNet::Helpers
{
	/// <summary>
	/// Helper class for managing application theme
	/// Based on WinUI Gallery ThemeHelper pattern
	/// </summary>
	class ThemeHelper
	{
	public:
		/// <summary>
		/// Gets or sets the root theme for the application
		/// </summary>
		static winrt::Microsoft::UI::Xaml::ElementTheme RootTheme();
		static void RootTheme(winrt::Microsoft::UI::Xaml::ElementTheme value);

		/// <summary>
		/// Gets the actual theme (resolves Default to Light or Dark based on system)
		/// </summary>
		static winrt::Microsoft::UI::Xaml::ElementTheme ActualTheme();

		/// <summary>
		/// Gets whether the app is using a light theme
		/// </summary>
		static bool IsDarkTheme();

		/// <summary>
		/// Initialize theme on app startup
		/// </summary>
		static void Initialize();

		/// <summary>
		/// Update theme for a specific window
		/// </summary>
		static void UpdateThemeForWindow(winrt::Microsoft::UI::Xaml::Window const& window);

		/// <summary>
		/// Save current theme to local settings
		/// </summary>
		static void SaveThemeToSettings();

		/// <summary>
		/// Load theme from local settings
		/// </summary>
		static winrt::Microsoft::UI::Xaml::ElementTheme LoadThemeFromSettings();

		/// <summary>
		/// Convert ElementTheme to string for UI display
		/// </summary>
		static winrt::hstring ThemeToString(winrt::Microsoft::UI::Xaml::ElementTheme theme);

		/// <summary>
		/// Convert string to ElementTheme
		/// </summary>
		static winrt::Microsoft::UI::Xaml::ElementTheme StringToTheme(winrt::hstring const& themeString);

	private:
		static winrt::Microsoft::UI::Xaml::ElementTheme s_rootTheme;
		static winrt::event_token s_actualThemeChangedToken;

		static constexpr const wchar_t* THEME_SETTING_KEY = L"AppTheme";

		/// <summary>
		/// Get system's current theme (Light or Dark)
		/// </summary>
		static winrt::Microsoft::UI::Xaml::ElementTheme GetSystemTheme();

		/// <summary>
		/// Update TitleBar theme for the given window
		/// </summary>
		static void UpdateTitleBarTheme(winrt::Microsoft::UI::Xaml::Window const& window);
	};
}
