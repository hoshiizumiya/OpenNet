#include "XamlWorkaround.h"
#include "ThemeSettingBackdropCustomizePage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/ThemeSettingBackdropCustomizePage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/ThemeSettingBackdropCustomizePage.g.cpp"
#endif

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.WindowHelper;
import winrt.WinUI3Package;
import winrt.WinUI.LiquidGlass;
import winrt.Windows.UI;

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace
{
	constexpr auto kBackdropFallbackColorKey      = "backdrop_fallback_color";
	constexpr auto kBackdropTintColorKey          = "backdrop_tint_color";
	constexpr auto kBackdropLuminosityOpacityKey  = "backdrop_luminosity_opacity";
	constexpr auto kBackdropTintOpacityKey        = "backdrop_tint_opacity";
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

	void ThemeSettingBackdropCustomizePage::BackdropValueChanged(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_isUpdatingUI) return;
		if (auto standard = sender.try_as<ToggleSwitch>()) m_enableWhenInactive = standard.IsOn();
		else if (auto glass = sender.try_as<winrt::WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) m_enableWhenInactive = glass.IsOn();
		UpdateMaterialControls();
		SaveToSettings();
		ApplyToCurrentBackdrop();
	}

	void ThemeSettingBackdropCustomizePage::BackdropValueChanged(IInspectable const&, ColorChangedEventArgs const&)
	{
		if (m_isUpdatingUI) return;
		SaveToSettings();
		ApplyToCurrentBackdrop();
	}

	void ThemeSettingBackdropCustomizePage::BackdropValueChanged(IInspectable const& sender, Controls::Primitives::RangeBaseValueChangedEventArgs const&)
	{
		if (m_isUpdatingUI) return;
		if (auto slider = sender.try_as<Slider>())
		{
			auto const name = slider.Name();
			if (name == L"LuminosityOpacitySlider" || name == L"LuminosityOpacityGlassSlider") m_luminosityOpacity = slider.Value();
			else if (name == L"TintOpacitySlider" || name == L"TintOpacityGlassSlider") m_tintOpacity = slider.Value();
		}
		UpdateMaterialControls();
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
		m_luminosityOpacity = luminosityOpacity;
		m_tintOpacity = tintOpacity;
		m_enableWhenInactive = enableWhenInactive;
		UpdateMaterialControls();
		m_isUpdatingUI = false;
	}

	void ThemeSettingBackdropCustomizePage::UpdateMaterialControls()
	{
		const bool wasUpdating = m_isUpdatingUI;
		m_isUpdatingUI = true;
		if (auto standard = LuminosityOpacitySlider()) standard.Value(m_luminosityOpacity);
		if (auto glass = LuminosityOpacityGlassSlider()) glass.Value(m_luminosityOpacity);
		if (auto standard = TintOpacitySlider()) standard.Value(m_tintOpacity);
		if (auto glass = TintOpacityGlassSlider()) glass.Value(m_tintOpacity);
		if (auto standard = EnableSwitch()) standard.IsOn(m_enableWhenInactive);
		if (auto glass = EnableGlassSwitch()) glass.IsOn(m_enableWhenInactive);
		m_isUpdatingUI = wasUpdating;
	}

	void ThemeSettingBackdropCustomizePage::MaterialControl_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		const bool wasUpdating = m_isUpdatingUI;
		m_isUpdatingUI = true;
		if (auto slider = sender.try_as<Slider>())
		{
			auto const name = slider.Name();
			if (name == L"LuminosityOpacitySlider" || name == L"LuminosityOpacityGlassSlider") slider.Value(m_luminosityOpacity);
			else if (name == L"TintOpacitySlider" || name == L"TintOpacityGlassSlider") slider.Value(m_tintOpacity);
		}
		if (auto standard = sender.try_as<ToggleSwitch>()) standard.IsOn(m_enableWhenInactive);
		if (auto glass = sender.try_as<winrt::WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) glass.IsOn(m_enableWhenInactive);
		m_isUpdatingUI = wasUpdating;
	}

	void ThemeSettingBackdropCustomizePage::SaveToSettings()
	{
		auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
		db.SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropFallbackColorKey, ColorToArgb(FallbackColorPicker().Color()));
		db.SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropTintColorKey, ColorToArgb(TintColorPicker().Color()));
		db.SetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropLuminosityOpacityKey, m_luminosityOpacity);
		db.SetDouble(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropTintOpacityKey, m_tintOpacity);
		db.SetBool(::OpenNet::Core::AppSettingsDatabase::CAT_UI, kBackdropEnableWhenInactiveKey, m_enableWhenInactive);
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
			m_luminosityOpacity = static_cast<double>(mica.LuminosityOpacity());
			TintColorPicker().Color(mica.TintColor());
			m_tintOpacity = static_cast<double>(mica.TintOpacity());
			m_enableWhenInactive = mica.EnableWhenInactive();
		}
		else if (auto acrylic = backdrop.try_as<winrt::WinUI3Package::CustomAcrylicBackdrop>())
		{
			FallbackColorPicker().Color(acrylic.FallbackColor());
			m_luminosityOpacity = static_cast<double>(acrylic.LuminosityOpacity());
			TintColorPicker().Color(acrylic.TintColor());
			m_tintOpacity = static_cast<double>(acrylic.TintOpacity());
			m_enableWhenInactive = acrylic.EnableWhenInactive();
		}
		UpdateMaterialControls();

		m_isUpdatingUI = false;
		SaveToSettings();
	}

	void ThemeSettingBackdropCustomizePage::ApplyToCurrentBackdrop()
	{
		auto const fallbackColor = FallbackColorPicker().Color();
		auto const luminosityOpacity = static_cast<float>(m_luminosityOpacity);
		auto const tintColor = TintColorPicker().Color();
		auto const tintOpacity = static_cast<float>(m_tintOpacity);
		auto const enableWhenInactive = m_enableWhenInactive;

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
