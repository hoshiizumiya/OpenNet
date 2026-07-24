export module OpenNet.Helpers.ThemeHelper;

import OpenNet.Core.AppSettingsDatabase;
import std;
import winrt.Microsoft.UI.Composition.SystemBackdrops;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Microsoft.UI.Xaml.Media.Imaging;
import winrt.Windows.UI;
import winrt.Windows.UI.ViewManagement;
import winrt.WinUI3Package;

using namespace winrt::Microsoft::UI::Composition::SystemBackdrops;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;

export namespace OpenNet::Helpers
{
	constexpr auto kBackdropFallbackColorKey       = "backdrop_fallback_color";
	constexpr auto kBackdropTintColorKey           = "backdrop_tint_color";
	constexpr auto kBackdropLuminosityOpacityKey   = "backdrop_luminosity_opacity";
	constexpr auto kBackdropTintOpacityKey         = "backdrop_tint_opacity";
	constexpr auto kBackdropEnableWhenInactiveKey  = "backdrop_enable_when_inactive";
	constexpr auto kBackdropUseFallbackKey         = "backdrop_use_fallback";

	/// <summary>
	/// Helper class for managing application theme
	/// Based on WinUI Gallery ThemeHelper pattern
	/// </summary>
	class ThemeHelper
	{
	public:
		static winrt::Windows::UI::Color ColorFromArgb(std::int64_t argb)
		{
			auto const value = static_cast<std::uint32_t>(argb);
			return winrt::Windows::UI::Color{
				static_cast<uint8_t>((value >> 24) & 0xFF),
				static_cast<uint8_t>((value >> 16) & 0xFF),
				static_cast<uint8_t>((value >> 8) & 0xFF),
				static_cast<uint8_t>(value & 0xFF)
			};
		}

		/// <summary>
		/// Applies a backdrop effect based on the current setting.
		/// </summary>
		static void ApplyBackdropFromSettings(winrt::Microsoft::UI::Xaml::Window const& window)
		{
			if (!window) return;

			auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
			auto const backgroundType = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_type", 1)), 0, 4);
			auto const micaType = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "mica_type", 1)), 0, 1);
			auto const acrylicType = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "acrylic_type", 0)), 0, 2);
			auto const useFallback = db.GetBool(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropUseFallbackKey).value_or(true);

			auto const fallbackColor = ColorFromArgb(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropFallbackColorKey).value_or(0xFF202020));
			auto const tintColor = ColorFromArgb(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropTintColorKey).value_or(0xFF202020));
			auto const luminosityOpacity = static_cast<float>(std::clamp(db.GetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropLuminosityOpacityKey).value_or(0.8), 0.0, 1.0));
			auto const tintOpacity = static_cast<float>(std::clamp(db.GetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropTintOpacityKey).value_or(0.8), 0.0, 1.0));
			auto const enableWhenInactive = db.GetBool(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropEnableWhenInactiveKey).value_or(true);

			switch (backgroundType)
			{
				case 1:  // Native Mica
				{
					if (MicaController::IsSupported())
					{
						auto mica = MicaBackdrop{};
						mica.Kind(micaType == 1 ? MicaKind::BaseAlt : MicaKind::Base);
						window.SystemBackdrop(mica);
						return;
					}

					if (useFallback) // TenMica
					{
						auto fallback = winrt::WinUI3Package::MicaBackdropWithFallback{};
						fallback.Fallback(winrt::WinUI3Package::TenMicaBackdrop());
						window.SystemBackdrop(fallback);
						return;
					}

					window.SystemBackdrop(nullptr);
					return;
				}
				case 2:  // Custom Mica (no fallback logic)
				{
					auto mica = winrt::WinUI3Package::CustomMicaBackdrop{};
					mica.Kind(micaType == 1 ? MicaKind::BaseAlt : MicaKind::Base);
					mica.FallbackColor(fallbackColor);
					mica.TintColor(tintColor);
					mica.LuminosityOpacity(luminosityOpacity);
					mica.TintOpacity(tintOpacity);
					mica.EnableWhenInactive(enableWhenInactive);
					window.SystemBackdrop(mica);
					return;
				}
				case 3:  // Native Acrylic
				{
					if (!DesktopAcrylicController::IsSupported())
					{
						window.SystemBackdrop(nullptr);
						return;
					}

					auto acrylic = winrt::WinUI3Package::CustomAcrylicBackdrop{};
					switch (acrylicType)
					{
						case 1:
							acrylic.Kind(DesktopAcrylicKind::Base);
							break;
						case 2:
							acrylic.Kind(DesktopAcrylicKind::Thin);
							break;
						case 0:
						default:
							acrylic.Kind(DesktopAcrylicKind::Default);
							break;
					}
					acrylic.EnableWhenInactive(enableWhenInactive);
					window.SystemBackdrop(acrylic);
					return;
				}
				case 4:  // Custom Acrylic (no fallback logic)
				{
					auto acrylic = winrt::WinUI3Package::CustomAcrylicBackdrop{};
					switch (acrylicType)
					{
						case 1:
							acrylic.Kind(DesktopAcrylicKind::Base);
							break;
						case 2:
							acrylic.Kind(DesktopAcrylicKind::Thin);
							break;
						case 0:
						default:
							acrylic.Kind(DesktopAcrylicKind::Default);
							break;
					}
					acrylic.FallbackColor(fallbackColor);
					acrylic.TintColor(tintColor);
					acrylic.LuminosityOpacity(luminosityOpacity);
					acrylic.TintOpacity(tintOpacity);
					acrylic.EnableWhenInactive(enableWhenInactive);
					window.SystemBackdrop(acrylic);
					return;
				}
				case 0:  // None style
				default:
					window.SystemBackdrop(nullptr);
					return;
			}
		}

		static winrt::Microsoft::UI::Xaml::Media::Stretch StretchFromIndex(std::int32_t index)
		{
			switch (index)
			{
				case 1: return winrt::Microsoft::UI::Xaml::Media::Stretch::Fill;
				case 2: return winrt::Microsoft::UI::Xaml::Media::Stretch::Uniform;
				case 3: return winrt::Microsoft::UI::Xaml::Media::Stretch::UniformToFill;
				case 0:
				default:
					return winrt::Microsoft::UI::Xaml::Media::Stretch::None;
			}
		}

		static void ApplyImageBackgroundFromSettings(winrt::Microsoft::UI::Xaml::Window const& window)
		{
			if (!window) return;

			auto panel = window.Content().try_as<winrt::Microsoft::UI::Xaml::Controls::Panel>();
			if (!panel) return;

			auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
			auto imagePath = db.GetStringW(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_image").value_or(L"");
			if (imagePath.empty())
			{
				panel.Background(nullptr);
				return;
			}

			// Construct a file URI without depending on Shlwapi's UrlCreateFromPathW.
			// generic_wstring() normalizes Windows separators to URI separators.
			std::filesystem::path const path{ imagePath };
			std::wstring imageUri = L"file:///" + path.lexically_normal().generic_wstring();

			auto stretchIndex = std::clamp(static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "image_stretch", 3)), 0, 3);
			auto opacity = std::clamp(db.GetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "image_opacity").value_or(20.0), 0.0, 100.0) / 100.0;

			try
			{
				auto bitmap = BitmapImage{};
				bitmap.UriSource(winrt::Windows::Foundation::Uri{ imageUri });
				auto brush = ImageBrush{};
				brush.ImageSource(bitmap);
				brush.Stretch(StretchFromIndex(stretchIndex));
				brush.Opacity(opacity);
				panel.Background(brush);
			}
			catch (...)
			{
			}
		}

		static void ApplyWindowAppearanceFromSettings(winrt::Microsoft::UI::Xaml::Window const& window)
		{
			ApplyBackdropFromSettings(window);
			ApplyImageBackgroundFromSettings(window);
		}

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
