#include "XamlWorkaround.h"
#include "MaterialControlState.h"
#include "ControlAppearance.h"
#include "LiquidGlassProfile.h"
#if __has_include("UI/Xaml/Control/Theme/MaterialControlState.g.cpp")
#include "UI/Xaml/Control/Theme/MaterialControlState.g.cpp"
#endif
import std;
import OpenNet.XamlApplicationLifetime;
import OpenNet.Core.AppSettingsDatabase;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Controls.Primitives;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.WinUI3Package;
import winrt.WinUI.LiquidGlass;
import winrt.WinUI.Composition.Hlsl;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
	namespace
	{
		constexpr char Category[] = "liquid_glass";
		struct ManagedControl
		{
			weak_ref<FrameworkElement> Element;
			WinUI::Composition::Hlsl::LiquidGlassBrush Brush{ nullptr };
			::OpenNet::UI::LiquidGlassOptics::Snapshot Defaults{};
		};
		std::vector<ManagedControl> s_managed;
		struct SurfaceDefaults
		{
			weak_ref<WinUI::Composition::Hlsl::LiquidGlassBrush> Brush;
			::OpenNet::UI::LiquidGlassOptics::Snapshot Defaults{};
		};
		bool s_brushRefreshQueued{};
		// Keep baselines across unload/reload without retaining controls or brushes.
		std::vector<SurfaceDefaults> s_controlDefaults;
		hstring LibraryKind(FrameworkElement const& element)
		{
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassButton>()) return L"Button";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassToggleButton>()) return L"ToggleButton";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassHyperlinkButton>()) return L"HyperlinkButton";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassCheckBox>()) return L"CheckBox";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassRadioButton>()) return L"RadioButton";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassSlider>()) return L"Slider";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassTextBox>()) return L"TextBox";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassSearchBox>()) return L"SearchBox";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassPasswordBox>()) return L"PasswordBox";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassComboBox>()) return L"ComboBox";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) return L"ToggleSwitch";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassCard>()) return L"Card";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassFloatingPanel>()) return L"FloatingPanel";
			if (element.try_as<WinUI::LiquidGlass::LiquidGlassTabBar>()) return L"TabBar";
			return L"";
		}
		WinUI::Composition::Hlsl::LiquidGlassBrush GetBrush(FrameworkElement const& element)
		{
			if (auto brush = ControlAppearance::GetBrush(element).try_as<WinUI::Composition::Hlsl::LiquidGlassBrush>()) return brush;
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassSearchBox>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassButton>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassToggleButton>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassHyperlinkButton>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassCheckBox>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassRadioButton>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassSlider>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassTextBox>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassPasswordBox>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassComboBox>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassCard>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassFloatingPanel>()) return control.GlassBrush();
			if (auto control = element.try_as<WinUI::LiquidGlass::LiquidGlassTabBar>()) return control.GlassBrush();
			return nullptr;
		}
	}

	MaterialControlState::MaterialControlState()
	{
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		ButtonEnabled(database.GetBool(Category, "ButtonEnabled").value_or(true));
		ToggleButtonEnabled(database.GetBool(Category, "ToggleButtonEnabled").value_or(true));
		HyperlinkButtonEnabled(database.GetBool(Category, "HyperlinkButtonEnabled").value_or(true));
		CheckBoxEnabled(database.GetBool(Category, "CheckBoxEnabled").value_or(true));
		RadioButtonEnabled(database.GetBool(Category, "RadioButtonEnabled").value_or(true));
		SliderEnabled(database.GetBool(Category, "SliderEnabled").value_or(true));
		TextBoxEnabled(database.GetBool(Category, "TextBoxEnabled").value_or(true));
		SearchBoxEnabled(database.GetBool(Category, "SearchBoxEnabled").value_or(true));
		PasswordBoxEnabled(database.GetBool(Category, "PasswordBoxEnabled").value_or(true));
		ComboBoxEnabled(database.GetBool(Category, "ComboBoxEnabled").value_or(true));
		ToggleSwitchEnabled(database.GetBool(Category, "ToggleSwitchEnabled").value_or(true));
		CardEnabled(database.GetBool(Category, "CardEnabled").value_or(true));
		FloatingPanelEnabled(database.GetBool(Category, "FloatingPanelEnabled").value_or(true));
		TabBarEnabled(database.GetBool(Category, "TabBarEnabled").value_or(true));
		Strength(database.GetDouble(Category, "Strength").value_or(1.0));
		BlurScale(database.GetDouble(Category, "BlurScale").value_or(1.0));
		RefractionScale(database.GetDouble(Category, "RefractionScale").value_or(1.0));
		DispersionScale(database.GetDouble(Category, "DispersionScale").value_or(1.0));
		HighlightScale(database.GetDouble(Category, "HighlightScale").value_or(1.0));
		MagnificationScale(database.GetDouble(Category, "MagnificationScale").value_or(1.0));
		DispersionRange(std::clamp(database.GetDouble(Category, "DispersionRange").value_or(1.0), 0.0, 1.0));
		RefFresnelRange(std::clamp(database.GetDouble(Category, "RefFresnelRange").value_or(30.0), 0.0, 100.0));
		RefFresnelHardness(std::clamp(database.GetDouble(Category, "RefFresnelHardness").value_or(20.0), 0.0, 100.0));
		RefFresnelFactor(std::clamp(database.GetDouble(Category, "RefFresnelFactor").value_or(0.0), 0.0, 100.0));
		GlareRange(std::clamp(database.GetDouble(Category, "GlareRange").value_or(30.0), 0.0, 100.0));
		GlareHardness(std::clamp(database.GetDouble(Category, "GlareHardness").value_or(20.0), 0.0, 100.0));
		GlareFactor(std::clamp(database.GetDouble(Category, "GlareFactor").value_or(0.0), 0.0, 120.0));
		GlareConvergence(std::clamp(database.GetDouble(Category, "GlareConvergence").value_or(50.0), 0.0, 100.0));
		GlareOppositeFactor(std::clamp(database.GetDouble(Category, "GlareOppositeFactor").value_or(80.0), 0.0, 100.0));
		GlareAngle(std::clamp(database.GetDouble(Category, "GlareAngle").value_or(-45.0), -180.0, 180.0));
		Magnification(std::clamp(database.GetDouble(Category, "Magnification").value_or(1.0), 1.0, 3.0));
		BloomAmount(std::clamp(database.GetDouble(Category, "BloomAmount").value_or(1.0), 0.0, 1.0));
		Brightness(std::clamp(database.GetDouble(Category, "Brightness").value_or(0.0), -1.0, 1.0));
		Temperature(std::clamp(database.GetDouble(Category, "Temperature").value_or(0.0), -1.0, 1.0));
		Vibrance(std::clamp(database.GetDouble(Category, "Vibrance").value_or(0.0), 0.0, 1.0));
		m_loading = false;
		UpdateSelection();
	}

	winrt::OpenNet::UI::Xaml::Control::Theme::MaterialControlState MaterialControlState::Current()
	{
		// This is the existing settings resource, never a shared composition brush.
		static class_type state{ nullptr };
		if (state) return state;
		auto app = Application::Current();
		if (app)
		{
			auto resources = app.Resources();
			auto key = box_value(L"MaterialControlState");
			if (resources.HasKey(key)) state = resources.Lookup(key).try_as<class_type>();
			if (state) return state;
			for (auto const& dictionary : resources.MergedDictionaries())
			{
				if (!dictionary.HasKey(key)) continue;
				state = dictionary.Lookup(key).try_as<class_type>();
				if (state) return state;
			}
		}
		throw hresult_illegal_method_call(L"MaterialControlState resources must be initialized before compiled bindings.");
	}

	winrt::hstring MaterialControlState::FormatSettingValue(double value)
	{
		return winrt::hstring(std::format(L"{:.2f}", value));
	}

	bool MaterialControlState::IsTrue(Windows::Foundation::IReference<bool> const& value)
	{
		return value && value.Value();
	}

	Windows::Foundation::IInspectable MaterialControlState::BoxInt32(std::int32_t value)
	{
		return box_value(value);
	}

	DependencyProperty MaterialControlState::StyleIndexProperty()
	{
		static auto property = DependencyProperty::Register(L"StyleIndex", xaml_typename<std::int32_t>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	std::int32_t MaterialControlState::StyleIndex()
	{
		return unbox_value<std::int32_t>(GetValue(StyleIndexProperty()));
	}
	void MaterialControlState::StyleIndex(std::int32_t value)
	{
		SetValue(StyleIndexProperty(), box_value(value));
	}

	DependencyProperty MaterialControlState::ButtonEnabledProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"ButtonEnabled",
				xaml_typename<bool>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::ButtonEnabled()
	{
		return unbox_value<bool>(GetValue(ButtonEnabledProperty()));
	}
	void MaterialControlState::ButtonEnabled(bool value)
	{
		SetValue(ButtonEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::ButtonMaterialProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"ButtonMaterial",
				xaml_typename<std::int32_t>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::ButtonMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(ButtonMaterialProperty()));
	}

	DependencyProperty MaterialControlState::ToggleButtonEnabledProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"ToggleButtonEnabled",
				xaml_typename<bool>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::ToggleButtonEnabled()
	{
		return unbox_value<bool>(GetValue(ToggleButtonEnabledProperty()));
	}
	void MaterialControlState::ToggleButtonEnabled(bool value)
	{
		SetValue(ToggleButtonEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::ToggleButtonMaterialProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"ToggleButtonMaterial",
				xaml_typename<std::int32_t>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::ToggleButtonMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(ToggleButtonMaterialProperty()));
	}

	DependencyProperty MaterialControlState::HyperlinkButtonEnabledProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"HyperlinkButtonEnabled",
				xaml_typename<bool>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::HyperlinkButtonEnabled()
	{
		return unbox_value<bool>(GetValue(HyperlinkButtonEnabledProperty()));
	}
	void MaterialControlState::HyperlinkButtonEnabled(bool value)
	{
		SetValue(HyperlinkButtonEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::HyperlinkButtonMaterialProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"HyperlinkButtonMaterial",
				xaml_typename<std::int32_t>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::HyperlinkButtonMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(HyperlinkButtonMaterialProperty()));
	}

	DependencyProperty MaterialControlState::CheckBoxEnabledProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"CheckBoxEnabled",
				xaml_typename<bool>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::CheckBoxEnabled()
	{
		return unbox_value<bool>(GetValue(CheckBoxEnabledProperty()));
	}
	void MaterialControlState::CheckBoxEnabled(bool value)
	{
		SetValue(CheckBoxEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::CheckBoxMaterialProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"CheckBoxMaterial",
				xaml_typename<std::int32_t>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::CheckBoxMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(CheckBoxMaterialProperty()));
	}

	DependencyProperty MaterialControlState::RadioButtonEnabledProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"RadioButtonEnabled",
				xaml_typename<bool>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::RadioButtonEnabled()
	{
		return unbox_value<bool>(GetValue(RadioButtonEnabledProperty()));
	}
	void MaterialControlState::RadioButtonEnabled(bool value)
	{
		SetValue(RadioButtonEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::RadioButtonMaterialProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"RadioButtonMaterial",
				xaml_typename<std::int32_t>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::RadioButtonMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(RadioButtonMaterialProperty()));
	}

	DependencyProperty MaterialControlState::SliderEnabledProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"SliderEnabled",
				xaml_typename<bool>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::SliderEnabled()
	{
		return unbox_value<bool>(GetValue(SliderEnabledProperty()));
	}
	void MaterialControlState::SliderEnabled(bool value)
	{
		SetValue(SliderEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::SliderMaterialProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"SliderMaterial",
				xaml_typename<std::int32_t>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::SliderMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(SliderMaterialProperty()));
	}

	DependencyProperty MaterialControlState::TextBoxEnabledProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"TextBoxEnabled",
				xaml_typename<bool>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::TextBoxEnabled()
	{
		return unbox_value<bool>(GetValue(TextBoxEnabledProperty()));
	}
	DependencyProperty MaterialControlState::SearchBoxEnabledProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"SearchBoxEnabled",
				xaml_typename<bool>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::SearchBoxEnabled()
	{
		return unbox_value<bool>(GetValue(SearchBoxEnabledProperty()));
	}
	void MaterialControlState::SearchBoxEnabled(bool value)
	{
		SetValue(SearchBoxEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::SearchBoxMaterialProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"SearchBoxMaterial",
				xaml_typename<std::int32_t>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::SearchBoxMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(SearchBoxMaterialProperty()));
	}
	void MaterialControlState::TextBoxEnabled(bool value)
	{
		SetValue(TextBoxEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::TextBoxMaterialProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"TextBoxMaterial",
				xaml_typename<std::int32_t>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::TextBoxMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(TextBoxMaterialProperty()));
	}

	DependencyProperty MaterialControlState::PasswordBoxEnabledProperty()
	{
		static auto property =
			DependencyProperty::Register(
				L"PasswordBoxEnabled",
				xaml_typename<bool>(),
				xaml_typename<class_type>(),
				PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::PasswordBoxEnabled()
	{
		return unbox_value<bool>(GetValue(PasswordBoxEnabledProperty()));
	}
	void MaterialControlState::PasswordBoxEnabled(bool value)
	{
		SetValue(PasswordBoxEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::PasswordBoxMaterialProperty()
	{
		static auto property = DependencyProperty::Register(L"PasswordBoxMaterial", xaml_typename<std::int32_t>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::PasswordBoxMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(PasswordBoxMaterialProperty()));
	}

	DependencyProperty MaterialControlState::ComboBoxEnabledProperty()
	{
		static auto property = DependencyProperty::Register(L"ComboBoxEnabled", xaml_typename<bool>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::ComboBoxEnabled()
	{
		return unbox_value<bool>(GetValue(ComboBoxEnabledProperty()));
	}
	void MaterialControlState::ComboBoxEnabled(bool value)
	{
		SetValue(ComboBoxEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::ComboBoxMaterialProperty()
	{
		static auto property = DependencyProperty::Register(L"ComboBoxMaterial", xaml_typename<std::int32_t>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::ComboBoxMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(ComboBoxMaterialProperty()));
	}

	DependencyProperty MaterialControlState::ToggleSwitchEnabledProperty()
	{
		static auto property = DependencyProperty::Register(L"ToggleSwitchEnabled", xaml_typename<bool>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::ToggleSwitchEnabled()
	{
		return unbox_value<bool>(GetValue(ToggleSwitchEnabledProperty()));
	}
	void MaterialControlState::ToggleSwitchEnabled(bool value)
	{
		SetValue(ToggleSwitchEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::ToggleSwitchMaterialProperty()
	{
		static auto property = DependencyProperty::Register(L"ToggleSwitchMaterial", xaml_typename<std::int32_t>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::ToggleSwitchMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(ToggleSwitchMaterialProperty()));
	}

	DependencyProperty MaterialControlState::CardEnabledProperty()
	{
		static auto property = DependencyProperty::Register(L"CardEnabled", xaml_typename<bool>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::CardEnabled()
	{
		return unbox_value<bool>(GetValue(CardEnabledProperty()));
	}
	void MaterialControlState::CardEnabled(bool value)
	{
		SetValue(CardEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::CardMaterialProperty()
	{
		static auto property = DependencyProperty::Register(L"CardMaterial", xaml_typename<std::int32_t>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::CardMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(CardMaterialProperty()));
	}

	DependencyProperty MaterialControlState::FloatingPanelEnabledProperty()
	{
		static auto property = DependencyProperty::Register(L"FloatingPanelEnabled", xaml_typename<bool>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::FloatingPanelEnabled()
	{
		return unbox_value<bool>(GetValue(FloatingPanelEnabledProperty()));
	}
	void MaterialControlState::FloatingPanelEnabled(bool value)
	{
		SetValue(FloatingPanelEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::FloatingPanelMaterialProperty()
	{
		static auto property = DependencyProperty::Register(L"FloatingPanelMaterial", xaml_typename<std::int32_t>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::FloatingPanelMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(FloatingPanelMaterialProperty()));
	}

	DependencyProperty MaterialControlState::TabBarEnabledProperty()
	{
		static auto property = DependencyProperty::Register(L"TabBarEnabled", xaml_typename<bool>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(true), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	bool MaterialControlState::TabBarEnabled()
	{
		return unbox_value<bool>(GetValue(TabBarEnabledProperty()));
	}
	void MaterialControlState::TabBarEnabled(bool value)
	{
		SetValue(TabBarEnabledProperty(), box_value(value));
	}
	DependencyProperty MaterialControlState::TabBarMaterialProperty()
	{
		static auto property = DependencyProperty::Register(L"TabBarMaterial", xaml_typename<std::int32_t>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(0) });
		return property;
	}
	std::int32_t MaterialControlState::TabBarMaterial()
	{
		return unbox_value<std::int32_t>(GetValue(TabBarMaterialProperty()));
	}

	DependencyProperty MaterialControlState::StrengthProperty()
	{
		static auto property = DependencyProperty::Register(L"Strength", xaml_typename<double>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::Strength()
	{
		return unbox_value<double>(GetValue(StrengthProperty()));
	}
	void MaterialControlState::Strength(double value)
	{
		SetValue(StrengthProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 2.0) : 1.0));
	}

	DependencyProperty MaterialControlState::BlurScaleProperty()
	{
		static auto property = DependencyProperty::Register(L"BlurScale", xaml_typename<double>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::BlurScale()
	{
		return unbox_value<double>(GetValue(BlurScaleProperty()));
	}
	void MaterialControlState::BlurScale(double value)
	{
		SetValue(BlurScaleProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 2.0) : 1.0));
	}

	DependencyProperty MaterialControlState::RefractionScaleProperty()
	{
		static auto property = DependencyProperty::Register(L"RefractionScale", xaml_typename<double>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::RefractionScale()
	{
		return unbox_value<double>(GetValue(RefractionScaleProperty()));
	}
	void MaterialControlState::RefractionScale(double value)
	{
		SetValue(RefractionScaleProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 2.0) : 1.0));
	}

	DependencyProperty MaterialControlState::DispersionScaleProperty()
	{
		static auto property = DependencyProperty::Register(L"DispersionScale", xaml_typename<double>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::DispersionScale()
	{
		return unbox_value<double>(GetValue(DispersionScaleProperty()));
	}
	void MaterialControlState::DispersionScale(double value)
	{
		SetValue(DispersionScaleProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 2.0) : 1.0));
	}

	DependencyProperty MaterialControlState::HighlightScaleProperty()
	{
		static auto property = DependencyProperty::Register(L"HighlightScale", xaml_typename<double>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::HighlightScale()
	{
		return unbox_value<double>(GetValue(HighlightScaleProperty()));
	}
	void MaterialControlState::HighlightScale(double value)
	{
		SetValue(HighlightScaleProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 2.0) : 1.0));
	}

	DependencyProperty MaterialControlState::MagnificationScaleProperty()
	{
		static auto property = DependencyProperty::Register(L"MagnificationScale", xaml_typename<double>(), xaml_typename<class_type>(),
															PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::MagnificationScale()
	{
		return unbox_value<double>(GetValue(MagnificationScaleProperty()));
	}
	void MaterialControlState::MagnificationScale(double value)
	{
		SetValue(MagnificationScaleProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 2.0) : 1.0));
	}

	void MaterialControlState::UpdateSelection()
	{
		SetValue(ButtonMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && ButtonEnabled() ? 1 : 0));
		SetValue(ToggleButtonMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && ToggleButtonEnabled() ? 1 : 0));
		SetValue(HyperlinkButtonMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && HyperlinkButtonEnabled() ? 1 : 0));
		SetValue(CheckBoxMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && CheckBoxEnabled() ? 1 : 0));
		SetValue(RadioButtonMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && RadioButtonEnabled() ? 1 : 0));
		SetValue(SliderMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && SliderEnabled() ? 1 : 0));
		SetValue(TextBoxMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && TextBoxEnabled() ? 1 : 0));
		SetValue(SearchBoxMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && SearchBoxEnabled() ? 1 : 0));
		SetValue(PasswordBoxMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && PasswordBoxEnabled() ? 1 : 0));
		SetValue(ComboBoxMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && ComboBoxEnabled() ? 1 : 0));
		SetValue(ToggleSwitchMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && ToggleSwitchEnabled() ? 1 : 0));
		SetValue(CardMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && CardEnabled() ? 1 : 0));
		SetValue(FloatingPanelMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && FloatingPanelEnabled() ? 1 : 0));
		SetValue(TabBarMaterialProperty(), box_value(StyleIndex() == 1 && ControlAppearance::CanUseGlass() && TabBarEnabled() ? 1 : 0));
		ControlAppearance::Refresh();
	}

	void MaterialControlState::SettingChanged(DependencyObject const& sender, DependencyPropertyChangedEventArgs const& args)
	{
		auto self = get_self<MaterialControlState>(sender.as<class_type>());
		if (self->m_loading) return;
		if (args.Property() == StyleIndexProperty())
		{
			self->UpdateSelection(); self->ApplyManagedControls(); return;
		}
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		if (args.Property() == ButtonEnabledProperty()) database.SetBool(Category, "ButtonEnabled", self->ButtonEnabled());
		if (args.Property() == ToggleButtonEnabledProperty()) database.SetBool(Category, "ToggleButtonEnabled", self->ToggleButtonEnabled());
		if (args.Property() == HyperlinkButtonEnabledProperty()) database.SetBool(Category, "HyperlinkButtonEnabled", self->HyperlinkButtonEnabled());
		if (args.Property() == CheckBoxEnabledProperty()) database.SetBool(Category, "CheckBoxEnabled", self->CheckBoxEnabled());
		if (args.Property() == RadioButtonEnabledProperty()) database.SetBool(Category, "RadioButtonEnabled", self->RadioButtonEnabled());
		if (args.Property() == SliderEnabledProperty()) database.SetBool(Category, "SliderEnabled", self->SliderEnabled());
		if (args.Property() == TextBoxEnabledProperty()) database.SetBool(Category, "TextBoxEnabled", self->TextBoxEnabled());
		if (args.Property() == SearchBoxEnabledProperty()) database.SetBool(Category, "SearchBoxEnabled", self->SearchBoxEnabled());
		if (args.Property() == PasswordBoxEnabledProperty()) database.SetBool(Category, "PasswordBoxEnabled", self->PasswordBoxEnabled());
		if (args.Property() == ComboBoxEnabledProperty()) database.SetBool(Category, "ComboBoxEnabled", self->ComboBoxEnabled());
		if (args.Property() == ToggleSwitchEnabledProperty()) database.SetBool(Category, "ToggleSwitchEnabled", self->ToggleSwitchEnabled());
		if (args.Property() == CardEnabledProperty()) database.SetBool(Category, "CardEnabled", self->CardEnabled());
		if (args.Property() == FloatingPanelEnabledProperty()) database.SetBool(Category, "FloatingPanelEnabled", self->FloatingPanelEnabled());
		if (args.Property() == TabBarEnabledProperty()) database.SetBool(Category, "TabBarEnabled", self->TabBarEnabled());
		if (args.Property() == StrengthProperty())
		{
			double const value = self->Strength();
			if (!std::isfinite(value) || value < 0.0 || value > 2.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "Strength", value);
		}
		if (args.Property() == BlurScaleProperty())
		{
			double const value = self->BlurScale();
			if (!std::isfinite(value) || value < 0.0 || value > 2.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "BlurScale", value);
		}
		if (args.Property() == RefractionScaleProperty())
		{
			double const value = self->RefractionScale();
			if (!std::isfinite(value) || value < 0.0 || value > 2.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "RefractionScale", value);
		}
		if (args.Property() == DispersionScaleProperty())
		{
			double const value = self->DispersionScale();
			if (!std::isfinite(value) || value < 0.0 || value > 2.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "DispersionScale", value);
		}
		if (args.Property() == HighlightScaleProperty())
		{
			double const value = self->HighlightScale();
			if (!std::isfinite(value) || value < 0.0 || value > 2.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "HighlightScale", value);
		}
		if (args.Property() == MagnificationScaleProperty())
		{
			double const value = self->MagnificationScale();
			if (!std::isfinite(value) || value < 0.0 || value > 2.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "MagnificationScale", value);
		}
		if (args.Property() == DispersionRangeProperty())
		{
			double const value = self->DispersionRange();
			if (!std::isfinite(value) || value < 0.0 || value > 1.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "DispersionRange", value);
		}
		if (args.Property() == RefFresnelRangeProperty())
		{
			double const value = self->RefFresnelRange();
			if (!std::isfinite(value) || value < 0.0 || value > 100.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "RefFresnelRange", value);
		}
		if (args.Property() == RefFresnelHardnessProperty())
		{
			double const value = self->RefFresnelHardness();
			if (!std::isfinite(value) || value < 0.0 || value > 100.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "RefFresnelHardness", value);
		}
		if (args.Property() == RefFresnelFactorProperty())
		{
			double const value = self->RefFresnelFactor();
			if (!std::isfinite(value) || value < 0.0 || value > 100.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "RefFresnelFactor", value);
		}
		if (args.Property() == GlareRangeProperty())
		{
			double const value = self->GlareRange();
			if (!std::isfinite(value) || value < 0.0 || value > 100.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "GlareRange", value);
		}
		if (args.Property() == GlareHardnessProperty())
		{
			double const value = self->GlareHardness();
			if (!std::isfinite(value) || value < 0.0 || value > 100.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "GlareHardness", value);
		}
		if (args.Property() == GlareFactorProperty())
		{
			double const value = self->GlareFactor();
			if (!std::isfinite(value) || value < 0.0 || value > 120.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "GlareFactor", value);
		}
		if (args.Property() == GlareConvergenceProperty())
		{
			double const value = self->GlareConvergence();
			if (!std::isfinite(value) || value < 0.0 || value > 100.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "GlareConvergence", value);
		}
		if (args.Property() == GlareOppositeFactorProperty())
		{
			double const value = self->GlareOppositeFactor();
			if (!std::isfinite(value) || value < 0.0 || value > 100.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "GlareOppositeFactor", value);
		}
		if (args.Property() == GlareAngleProperty())
		{
			double const value = self->GlareAngle();
			if (!std::isfinite(value) || value < -180.0 || value > 180.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "GlareAngle", value);
		}
		if (args.Property() == MagnificationProperty())
		{
			double const value = self->Magnification();
			if (!std::isfinite(value) || value < 1.0 || value > 3.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "Magnification", value);
		}
		if (args.Property() == BloomAmountProperty())
		{
			double const value = self->BloomAmount();
			if (!std::isfinite(value) || value < 0.0 || value > 1.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "BloomAmount", value);
		}
		if (args.Property() == BrightnessProperty())
		{
			double const value = self->Brightness();
			if (!std::isfinite(value) || value < -1.0 || value > 1.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "Brightness", value);
		}
		if (args.Property() == TemperatureProperty())
		{
			double const value = self->Temperature();
			if (!std::isfinite(value) || value < -1.0 || value > 1.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "Temperature", value);
		}
		if (args.Property() == VibranceProperty())
		{
			double const value = self->Vibrance();
			if (!std::isfinite(value) || value < 0.0 || value > 1.0)
			{
				self->m_loading = true;
				self->SetValue(args.Property(), args.OldValue());
				self->m_loading = false;
				return;
			}
			database.SetDouble(Category, "Vibrance", value);
		}
		self->UpdateSelection();
		LiquidGlassProfile::SyncEnabled();
		self->ApplyManagedControls();
	}

	DependencyProperty MaterialControlState::DispersionRangeProperty()
	{
		static auto property = DependencyProperty::Register(
			L"DispersionRange", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::DispersionRange()
	{
		return unbox_value<double>(GetValue(DispersionRangeProperty()));
	}
	void MaterialControlState::DispersionRange(double value)
	{
		SetValue(DispersionRangeProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 1.0));
	}

	DependencyProperty MaterialControlState::RefFresnelRangeProperty()
	{
		static auto property = DependencyProperty::Register(
			L"RefFresnelRange", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(30.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::RefFresnelRange()
	{
		return unbox_value<double>(GetValue(RefFresnelRangeProperty()));
	}
	void MaterialControlState::RefFresnelRange(double value)
	{
		SetValue(RefFresnelRangeProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 100.0) : 30.0));
	}

	DependencyProperty MaterialControlState::RefFresnelHardnessProperty()
	{
		static auto property = DependencyProperty::Register(
			L"RefFresnelHardness", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(20.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::RefFresnelHardness()
	{
		return unbox_value<double>(GetValue(RefFresnelHardnessProperty()));
	}
	void MaterialControlState::RefFresnelHardness(double value)
	{
		SetValue(RefFresnelHardnessProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 100.0) : 20.0));
	}

	DependencyProperty MaterialControlState::RefFresnelFactorProperty()
	{
		static auto property = DependencyProperty::Register(
			L"RefFresnelFactor", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::RefFresnelFactor()
	{
		return unbox_value<double>(GetValue(RefFresnelFactorProperty()));
	}
	void MaterialControlState::RefFresnelFactor(double value)
	{
		SetValue(RefFresnelFactorProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 100.0) : 0.0));
	}

	DependencyProperty MaterialControlState::GlareRangeProperty()
	{
		static auto property = DependencyProperty::Register(
			L"GlareRange", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(30.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::GlareRange()
	{
		return unbox_value<double>(GetValue(GlareRangeProperty()));
	}
	void MaterialControlState::GlareRange(double value)
	{
		SetValue(GlareRangeProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 100.0) : 30.0));
	}

	DependencyProperty MaterialControlState::GlareHardnessProperty()
	{
		static auto property = DependencyProperty::Register(
			L"GlareHardness", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(20.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::GlareHardness()
	{
		return unbox_value<double>(GetValue(GlareHardnessProperty()));
	}
	void MaterialControlState::GlareHardness(double value)
	{
		SetValue(GlareHardnessProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 100.0) : 20.0));
	}

	DependencyProperty MaterialControlState::GlareFactorProperty()
	{
		static auto property = DependencyProperty::Register(
			L"GlareFactor", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::GlareFactor()
	{
		return unbox_value<double>(GetValue(GlareFactorProperty()));
	}
	void MaterialControlState::GlareFactor(double value)
	{
		SetValue(GlareFactorProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 120.0) : 0.0));
	}

	DependencyProperty MaterialControlState::GlareConvergenceProperty()
	{
		static auto property = DependencyProperty::Register(
			L"GlareConvergence", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(50.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::GlareConvergence()
	{
		return unbox_value<double>(GetValue(GlareConvergenceProperty()));
	}
	void MaterialControlState::GlareConvergence(double value)
	{
		SetValue(GlareConvergenceProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 100.0) : 50.0));
	}

	DependencyProperty MaterialControlState::GlareOppositeFactorProperty()
	{
		static auto property = DependencyProperty::Register(
			L"GlareOppositeFactor", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(80.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::GlareOppositeFactor()
	{
		return unbox_value<double>(GetValue(GlareOppositeFactorProperty()));
	}
	void MaterialControlState::GlareOppositeFactor(double value)
	{
		SetValue(GlareOppositeFactorProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 100.0) : 80.0));
	}

	DependencyProperty MaterialControlState::GlareAngleProperty()
	{
		static auto property = DependencyProperty::Register(
			L"GlareAngle", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(-45.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::GlareAngle()
	{
		return unbox_value<double>(GetValue(GlareAngleProperty()));
	}
	void MaterialControlState::GlareAngle(double value)
	{
		SetValue(GlareAngleProperty(), box_value(std::isfinite(value) ? std::clamp(value, -180.0, 180.0) : -45.0));
	}

	DependencyProperty MaterialControlState::MagnificationProperty()
	{
		static auto property = DependencyProperty::Register(
			L"Magnification", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::Magnification()
	{
		return unbox_value<double>(GetValue(MagnificationProperty()));
	}
	void MaterialControlState::Magnification(double value)
	{
		SetValue(MagnificationProperty(), box_value(std::isfinite(value) ? std::clamp(value, 1.0, 3.0) : 1.0));
	}

	DependencyProperty MaterialControlState::BloomAmountProperty()
	{
		static auto property = DependencyProperty::Register(
			L"BloomAmount", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::BloomAmount()
	{
		return unbox_value<double>(GetValue(BloomAmountProperty()));
	}
	void MaterialControlState::BloomAmount(double value)
	{
		SetValue(BloomAmountProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 1.0));
	}

	DependencyProperty MaterialControlState::BrightnessProperty()
	{
		static auto property = DependencyProperty::Register(
			L"Brightness", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::Brightness()
	{
		return unbox_value<double>(GetValue(BrightnessProperty()));
	}
	void MaterialControlState::Brightness(double value)
	{
		SetValue(BrightnessProperty(), box_value(std::isfinite(value) ? std::clamp(value, -1.0, 1.0) : 0.0));
	}

	DependencyProperty MaterialControlState::TemperatureProperty()
	{
		static auto property = DependencyProperty::Register(
			L"Temperature", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::Temperature()
	{
		return unbox_value<double>(GetValue(TemperatureProperty()));
	}
	void MaterialControlState::Temperature(double value)
	{
		SetValue(TemperatureProperty(), box_value(std::isfinite(value) ? std::clamp(value, -1.0, 1.0) : 0.0));
	}

	DependencyProperty MaterialControlState::VibranceProperty()
	{
		static auto property = DependencyProperty::Register(
			L"Vibrance", xaml_typename<double>(), xaml_typename<class_type>(),
			PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ SettingChanged } });
		return property;
	}
	double MaterialControlState::Vibrance()
	{
		return unbox_value<double>(GetValue(VibranceProperty()));
	}
	void MaterialControlState::Vibrance(double value)
	{
		SetValue(VibranceProperty(), box_value(std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0));
	}

	DependencyProperty MaterialControlState::SettingValueProperty()
	{
		static auto property = DependencyProperty::RegisterAttached(
			L"SettingValue", xaml_typename<double>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(1.0) });
		return property;
	}
	double MaterialControlState::GetSettingValue(DependencyObject const& target)
	{
		return unbox_value<double>(target.GetValue(SettingValueProperty()));
	}
	void MaterialControlState::SetSettingValue(DependencyObject const& target, double value)
	{
		target.SetValue(SettingValueProperty(), box_value(value));
	}

	DependencyProperty MaterialControlState::SettingMinimumProperty()
	{
		static auto property = DependencyProperty::RegisterAttached(
			L"SettingMinimum", xaml_typename<double>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(0.0) });
		return property;
	}
	double MaterialControlState::GetSettingMinimum(DependencyObject const& target)
	{
		return unbox_value<double>(target.GetValue(SettingMinimumProperty()));
	}
	void MaterialControlState::SetSettingMinimum(DependencyObject const& target, double value)
	{
		target.SetValue(SettingMinimumProperty(), box_value(value));
	}

	DependencyProperty MaterialControlState::SettingMaximumProperty()
	{
		static auto property = DependencyProperty::RegisterAttached(
			L"SettingMaximum", xaml_typename<double>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(2.0) });
		return property;
	}
	double MaterialControlState::GetSettingMaximum(DependencyObject const& target)
	{
		return unbox_value<double>(target.GetValue(SettingMaximumProperty()));
	}
	void MaterialControlState::SetSettingMaximum(DependencyObject const& target, double value)
	{
		target.SetValue(SettingMaximumProperty(), box_value(value));
	}

	DependencyProperty MaterialControlState::SettingStepProperty()
	{
		static auto property = DependencyProperty::RegisterAttached(
			L"SettingStep", xaml_typename<double>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(0.1) });
		return property;
	}
	double MaterialControlState::GetSettingStep(DependencyObject const& target)
	{
		return unbox_value<double>(target.GetValue(SettingStepProperty()));
	}
	void MaterialControlState::SetSettingStep(DependencyObject const& target, double value)
	{
		target.SetValue(SettingStepProperty(), box_value(value));
	}

	DependencyProperty MaterialControlState::SettingLargeStepProperty()
	{
		static auto property = DependencyProperty::RegisterAttached(
			L"SettingLargeStep", xaml_typename<double>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(1.0) });
		return property;
	}
	double MaterialControlState::GetSettingLargeStep(DependencyObject const& target)
	{
		return unbox_value<double>(target.GetValue(SettingLargeStepProperty()));
	}
	void MaterialControlState::SetSettingLargeStep(DependencyObject const& target, double value)
	{
		target.SetValue(SettingLargeStepProperty(), box_value(value));
	}

	DependencyProperty MaterialControlState::SettingIsOnProperty()
	{
		static auto property = DependencyProperty::RegisterAttached(
			L"SettingIsOn", xaml_typename<bool>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(false) });
		return property;
	}
	bool MaterialControlState::GetSettingIsOn(DependencyObject const& target)
	{
		return unbox_value<bool>(target.GetValue(SettingIsOnProperty()));
	}
	void MaterialControlState::SetSettingIsOn(DependencyObject const& target, bool value)
	{
		target.SetValue(SettingIsOnProperty(), box_value(value));
	}

	DependencyProperty MaterialControlState::IsManagedProperty()
	{
		static auto property = DependencyProperty::RegisterAttached(L"IsManaged", xaml_typename<bool>(), xaml_typename<class_type>(),
																	PropertyMetadata{ box_value(false), PropertyChangedCallback{ ManagedChanged } });
		return property;
	}
	bool MaterialControlState::GetIsManaged(DependencyObject const& target)
	{
		return unbox_value_or<bool>(target.GetValue(IsManagedProperty()), false);
	}
	void MaterialControlState::SetIsManaged(DependencyObject const& target, bool value)
	{
		target.SetValue(IsManagedProperty(), box_value(value));
	}

	FrameworkElement MaterialControlState::CreateTextBoxPresenter(Controls::TextBox const& native)
	{
		ControlAppearance::SetIsEnabled(native, true);
		return native;
	}


	FrameworkElement MaterialControlState::CreateRadioButtonPresenter(Controls::RadioButton const& native)
	{
		ControlAppearance::SetIsEnabled(native, true);
		return native;
	}

	void MaterialControlState::ManagedChanged(DependencyObject const& sender, DependencyPropertyChangedEventArgs const& args)
	{
		auto element = sender.try_as<FrameworkElement>();
		if (!element) return;
		static auto hooked = DependencyProperty::RegisterAttached(L"ManagedHooksInstalled", xaml_typename<bool>(), xaml_typename<class_type>(), PropertyMetadata{ box_value(false) });
		if (!unbox_value_or<bool>(element.GetValue(hooked), false))
		{
			element.Loaded(&MaterialControlState::ManagedLoaded);
			element.Unloaded(&MaterialControlState::ManagedUnloaded);
			element.SetValue(hooked, box_value(true));
		}
		if (!unbox_value<bool>(args.NewValue())) ManagedUnloaded(element, RoutedEventArgs{});
		else if (element.IsLoaded()) ManagedLoaded(element, RoutedEventArgs{});
	}

	void MaterialControlState::ManagedLoaded(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
	{
		auto element = sender.try_as<FrameworkElement>();
		if (!element || !GetIsManaged(element)) return;
		if (auto control = element.try_as<Controls::Control>()) control.ApplyTemplate();
		auto brush = GetBrush(element);
		if (!brush) return;

		std::erase_if(s_controlDefaults, [](auto const& item)
		{
			return !item.Brush.get();
		});
		auto baseline = std::find_if(s_controlDefaults.begin(), s_controlDefaults.end(), [&brush](auto const& item)
		{
			return item.Brush.get() == brush;
		});
		if (baseline == s_controlDefaults.end())
		{
			s_controlDefaults.push_back({ make_weak(brush), ::OpenNet::UI::LiquidGlassOptics::Capture(brush) });
			baseline = std::prev(s_controlDefaults.end());
		}
		auto entry = std::find_if(s_managed.begin(), s_managed.end(), [&element](auto const& item)
		{
			return item.Element.get() == element;
		});
		if (entry == s_managed.end()) s_managed.push_back({ make_weak(element), brush, baseline->Defaults });
		else if (entry->Brush != brush) *entry = { make_weak(element), brush, baseline->Defaults };
		for (auto const& dictionary : Application::Current().Resources().MergedDictionaries())
		{
			auto key = box_value(L"MaterialControlState");
			if (!dictionary.HasKey(key)) continue;
			if (auto state = dictionary.Lookup(key).try_as<class_type>()) get_self<MaterialControlState>(state)->ApplyManagedControl(element);
			break;
		}
	}

	void MaterialControlState::ManagedUnloaded(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
	{
		auto element = sender.try_as<FrameworkElement>();
		// No composition writes during unload/disconnection.
		std::erase_if(s_managed, [&element](auto const& item)
		{
			auto control = item.Element.get(); return !control || control == element;
		});
	}


	Controls::Primitives::ButtonBase MaterialControlState::CreateActionButton(Input::ICommand const& command, Windows::Foundation::IInspectable const& content, std::int32_t material)
	{
		Controls::Button button = material == 1 ? WinUI::LiquidGlass::LiquidGlassButton().as<Controls::Button>() : Controls::Button{};
		button.Command(command); button.Content(content);
		if (material == 1) SetIsManaged(button, true);
		return button;
	}

	void MaterialControlState::RefreshBrushes()
	{
		if (::OpenNet::XamlApplicationLifetime::Exiting.load() || s_brushRefreshQueued) return;
		s_brushRefreshQueued = true;
		try
		{
			auto state = Current();
			if (state.DispatcherQueue().TryEnqueue([weak = make_weak(state)]
			{
				try
				{
					if (::OpenNet::XamlApplicationLifetime::Exiting.load()) LiquidGlassProfile::RollbackChanges();
					else if (auto current = weak.get())
					{
						auto self = get_self<MaterialControlState>(current);
						LiquidGlassProfile::ValidatePendingChanges();
						bool stable = false;
						for (int attempt = 0; attempt < 3; ++attempt)
						{
							auto revision = LiquidGlassProfile::Revision();
							self->UpdateSelection();
							self->ApplyManagedControls();
							if (revision != LiquidGlassProfile::Revision()) continue;
							LiquidGlassProfile::CommitChanges();
							stable = true;
							break;
						}
						if (!stable) throw hresult_error(E_FAIL, L"Liquid glass parameter recovery did not stabilize.");
					}
					else LiquidGlassProfile::RollbackChanges();
				}
				catch (...)
				{
					// Nothing may escape DispatcherQueue into CoreMessaging fail-fast.
					LiquidGlassProfile::ReportCurrentFailure(L"Apply optical refresh");
					LiquidGlassProfile::RollbackChanges();
					try
					{
						if (!::OpenNet::XamlApplicationLifetime::Exiting.load())
							if (auto current = weak.get()) get_self<MaterialControlState>(current)->ApplyManagedControls();
					}
					catch (...)
					{
						LiquidGlassProfile::ReportCurrentFailure(L"Restore optical refresh");
					}
				}
				s_brushRefreshQueued = false;
			})) return;
		}
		catch (...)
		{
			LiquidGlassProfile::ReportCurrentFailure(L"Queue optical refresh");
		}
		s_brushRefreshQueued = false;
		LiquidGlassProfile::RollbackChanges();
	}

	void MaterialControlState::ApplyManagedControls()
	{
		if (::OpenNet::XamlApplicationLifetime::Exiting.load()) return;
		std::erase_if(s_managed, [](auto const& item)
		{
			return !item.Element.get();
		});
		for (auto const& item : s_managed)
			if (auto element = item.Element.get(); element && element.IsLoaded()) ApplyManagedControl(element);
	}

	void MaterialControlState::ApplyManagedControl(FrameworkElement const& element)
	{
		if (::OpenNet::XamlApplicationLifetime::Exiting.load()) return;
		auto item = std::find_if(s_managed.begin(), s_managed.end(), [&element](auto const& entry)
		{
			return entry.Element.get() == element;
		});
		if (item == s_managed.end()) return;
		auto kind = ::OpenNet::UI::LiquidGlassOptics::Canonical(ControlAppearance::GetSurface(element));
		auto libraryKind = LibraryKind(element);
		if (!libraryKind.empty()) kind = libraryKind;
		if (kind.empty()) kind = L"Card";
		if (libraryKind.empty())
		{
			::OpenNet::UI::LiquidGlassOptics::RestoreGeometry(item->Brush, item->Defaults);
		}
		// Optical defaults and overrides go through the same guarded native setters.
		// Library controls still own geometry, DPI and interaction motion.
		auto result = LiquidGlassProfile::Apply(kind, item->Brush, item->Defaults);
		if (libraryKind.empty())
		{
			// Keep automatic accent contrast/tint adaptation when no override exists.
			ControlAppearance::AdaptSurface(element);
			result = std::max(result, LiquidGlassProfile::Apply(kind, item->Brush, item->Defaults, true));
		}
		auto enabled = StyleIndex() == 1;
		if (kind == L"Card") enabled = enabled && CardEnabled();
		if (kind == L"FloatingPanel") enabled = enabled && FloatingPanelEnabled();
		if (kind == L"TabBar") enabled = enabled && TabBarEnabled();
		item->Brush.IsEnabled(result != LiquidGlassApplyResult::Unsupported
							  && (ControlAppearance::GetIsPreview(element) || enabled) && ControlAppearance::CanUseGlass());
	}
}
