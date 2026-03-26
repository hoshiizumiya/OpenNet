#include "pch.h"
#include "ThemeSettingBackdropCustomizePage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/ThemeSettingBackdropCustomizePage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/ThemeSettingBackdropCustomizePage.g.cpp"
#endif

#include "Core/AppSettingsDatabase.h"
#include "Helpers/WindowHelper.h"
#include <winrt/WinUI3Package.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace
{
	constexpr auto kBackdropFallbackColorKey = "backdrop_fallback_color";
	constexpr auto kBackdropTintColorKey = "backdrop_tint_color";
	constexpr auto kBackdropLuminosityOpacityKey = "backdrop_luminosity_opacity";
	constexpr auto kBackdropTintOpacityKey = "backdrop_tint_opacity";
	constexpr auto kBackdropEnableWhenInactiveKey = "backdrop_enable_when_inactive";

	int64_t ColorToArgb(winrt::Windows::UI::Color const& color)
	{
		return (static_cast<int64_t>(color.A) << 24)
			| (static_cast<int64_t>(color.R) << 16)
			| (static_cast<int64_t>(color.G) << 8)
			| static_cast<int64_t>(color.B);
	}

	winrt::Windows::UI::Color ColorFromArgb(int64_t argb)
	{
		auto const value = static_cast<uint32_t>(argb);
		return winrt::Windows::UI::Color{
			static_cast<uint8_t>((value >> 24) & 0xFF),
			static_cast<uint8_t>((value >> 16) & 0xFF),
			static_cast<uint8_t>((value >> 8) & 0xFF),
			static_cast<uint8_t>(value & 0xFF)
		};
	}
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	ThemeSettingBackdropCustomizePage::ThemeSettingBackdropCustomizePage()
	{
		InitializeComponent();
	}

	void ThemeSettingBackdropCustomizePage::Page_Loaded(IInspectable const&, RoutedEventArgs const&)
	{
		LoadFromSettings();

		// Only sync from current backdrop if NOT in custom mode (indices 2 or 4).
		// Custom modes should only load persisted settings, not override with window state.
		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto const backgroundType = static_cast<int>(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, "background_type", 1));
		bool const isCustomMode = (backgroundType == 2 || backgroundType == 4);

		if (!isCustomMode)
		{
			SyncFromCurrentBackdrop();
		}
	}

	void ThemeSettingBackdropCustomizePage::BackdropValueChanged(IInspectable const&, RoutedEventArgs const&)
	{
		if (m_isUpdatingUI) return;
		SaveToSettings();
		ApplyToCurrentBackdrop();
	}

	void ThemeSettingBackdropCustomizePage::BackdropValueChanged(IInspectable const&, ColorChangedEventArgs const&)
	{
		if (m_isUpdatingUI) return;
		SaveToSettings();
		ApplyToCurrentBackdrop();
	}

	void ThemeSettingBackdropCustomizePage::BackdropValueChanged(IInspectable const&, Controls::Primitives::RangeBaseValueChangedEventArgs const&)
	{
		if (m_isUpdatingUI) return;
		SaveToSettings();
		ApplyToCurrentBackdrop();
	}

	void ThemeSettingBackdropCustomizePage::LoadFromSettings()
	{
		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto const fallbackColor = ColorFromArgb(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropFallbackColorKey).value_or(0xFF202020));
		auto const tintColor = ColorFromArgb(db.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropTintColorKey).value_or(0xFF202020));
		auto const luminosityOpacity = std::clamp(db.GetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropLuminosityOpacityKey).value_or(0.8), 0.0, 1.0);
		auto const tintOpacity = std::clamp(db.GetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropTintOpacityKey).value_or(0.8), 0.0, 1.0);
		auto const enableWhenInactive = db.GetBool(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropEnableWhenInactiveKey).value_or(true);

		m_isUpdatingUI = true;
		FallbackColorPicker().Color(fallbackColor);
		TintColorPicker().Color(tintColor);
		LuminosityOpacitySlider().Value(luminosityOpacity);
		TintOpacitySlider().Value(tintOpacity);
		EnableSwitch().IsOn(enableWhenInactive);
		m_isUpdatingUI = false;
	}

	void ThemeSettingBackdropCustomizePage::SaveToSettings()
	{
		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		db.SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropFallbackColorKey, ColorToArgb(FallbackColorPicker().Color()));
		db.SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropTintColorKey, ColorToArgb(TintColorPicker().Color()));
		db.SetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropLuminosityOpacityKey, LuminosityOpacitySlider().Value());
		db.SetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropTintOpacityKey, TintOpacitySlider().Value());
		db.SetBool(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropEnableWhenInactiveKey, EnableSwitch().IsOn());
	}

	void ThemeSettingBackdropCustomizePage::SyncFromCurrentBackdrop()
	{
		auto window = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowForElement(*this);
		if (!window)
		{
			return;
		}

		auto backdrop = window.SystemBackdrop();
		if (!backdrop)
		{
			return;
		}

		m_isUpdatingUI = true;

		if (auto mica = backdrop.try_as<winrt::WinUI3Package::CustomMicaBackdrop>())
		{
			FallbackColorPicker().Color(mica.FallbackColor());
			LuminosityOpacitySlider().Value(static_cast<double>(mica.LuminosityOpacity()));
			TintColorPicker().Color(mica.TintColor());
			TintOpacitySlider().Value(static_cast<double>(mica.TintOpacity()));
			EnableSwitch().IsOn(mica.EnableWhenInactive());
		}
		else if (auto acrylic = backdrop.try_as<winrt::WinUI3Package::CustomAcrylicBackdrop>())
		{
			FallbackColorPicker().Color(acrylic.FallbackColor());
			LuminosityOpacitySlider().Value(static_cast<double>(acrylic.LuminosityOpacity()));
			TintColorPicker().Color(acrylic.TintColor());
			TintOpacitySlider().Value(static_cast<double>(acrylic.TintOpacity()));
			EnableSwitch().IsOn(acrylic.EnableWhenInactive());
		}

		m_isUpdatingUI = false;
		SaveToSettings();
	}

	void ThemeSettingBackdropCustomizePage::ApplyToCurrentBackdrop()
	{
		auto const fallbackColor = FallbackColorPicker().Color();
		auto const luminosityOpacity = static_cast<float>(LuminosityOpacitySlider().Value());
		auto const tintColor = TintColorPicker().Color();
		auto const tintOpacity = static_cast<float>(TintOpacitySlider().Value());
		auto const enableWhenInactive = EnableSwitch().IsOn();

		for (auto const& window : ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::ActiveWindows())
		{
			if (!window)
			{
				continue;
			}

			auto backdrop = window.SystemBackdrop();
			if (!backdrop)
			{
				continue;
			}

			if (auto mica = backdrop.try_as<winrt::WinUI3Package::CustomMicaBackdrop>())
			{
				mica.FallbackColor(fallbackColor);
				mica.LuminosityOpacity(luminosityOpacity);
				mica.TintColor(tintColor);
				mica.TintOpacity(tintOpacity);
				mica.EnableWhenInactive(enableWhenInactive);
				continue;
			}

			if (auto acrylic = backdrop.try_as<winrt::WinUI3Package::CustomAcrylicBackdrop>())
			{
				acrylic.FallbackColor(fallbackColor);
				acrylic.LuminosityOpacity(luminosityOpacity);
				acrylic.TintColor(tintColor);
				acrylic.TintOpacity(tintOpacity);
				acrylic.EnableWhenInactive(enableWhenInactive);
			}
		}
	}
}
