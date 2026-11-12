#include "WindowsPlatform.h"
#include "XamlWorkaround.h"
#include "MaterialAppearance.h"
#include "ControlAppearance.h"
#include "MaterialControlState.h"
#include "LiquidGlassProfile.h"
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Hosting;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.WinUI.LiquidGlass;
import OpenNet.XamlApplicationLifetime;

namespace OpenNet::UI
{
	using namespace winrt;
	using namespace Microsoft::UI::Xaml;
	namespace W = WinUI::LiquidGlass;
	namespace T = winrt::OpenNet::UI::Xaml::Control::Theme::implementation;
	namespace
	{
		struct State
		{
			weak_ref<Controls::Control> Control;
			Style OriginalStyle{ nullptr };
			Media::Brush GlassBackground{ nullptr };
			bool HasLocalStyle{};
			bool CallerSetsBackground{};
			std::array<double, 6> Motion{};
			std::optional<bool> MotionGlass;
			std::int64_t BackgroundToken{};
			std::shared_ptr<bool> Applying{ std::make_shared<bool>(false) };
		};
		std::vector<State> states;

		bool StyleSetsBackground(Style const& style)
		{
			for (auto current = style; current; current = current.BasedOn())
				for (auto const& setter : current.Setters())
					if (auto value = setter.try_as<Setter>(); value && value.Property() == Controls::Control::BackgroundProperty()) return true;
			return false;
		}

		void ApplyMotion(FrameworkElement const& element, State& state, bool glass)
		{
			if (state.MotionGlass == glass) return;
			auto const motion = glass ? state.Motion : std::array<double, 6>{ 1, 1, 1, 1, 0, 0 };
			W::LiquidGlassInteraction::SetRestScale(element, motion[0]);
			W::LiquidGlassInteraction::SetPointerOverScale(element, motion[1]);
			W::LiquidGlassInteraction::SetPressedScale(element, motion[2]);
			W::LiquidGlassInteraction::SetFocusedScale(element, motion[3]);
			W::LiquidGlassInteraction::SetElasticity(element, motion[4]);
			W::LiquidGlassInteraction::SetPointerDisplacement(element, motion[5]);
			if (!glass || state.MotionGlass == false)
			{
				auto visual = Hosting::ElementCompositionPreview::GetElementVisual(element);
				visual.StopAnimation(L"Scale"); visual.StopAnimation(L"Translation");
				auto scale = static_cast<float>(glass ? motion[0] : 1.0);
				Windows::Foundation::Numerics::float3 value{ scale, scale, 1.0f };
				visual.Scale(value); element.Scale(value); element.Translation({ 0, 0, 0 });
			}
			state.MotionGlass = glass;
		}
	}

	hstring MaterialAppearance::Kind(FrameworkElement const& element)
	{
		if (element.try_as<W::LiquidGlassToggleSwitch>()) return L"ToggleSwitch";
		if (element.try_as<W::LiquidGlassButton>()) return L"Button";
		if (element.try_as<W::LiquidGlassToggleButton>()) return L"ToggleButton";
		if (element.try_as<W::LiquidGlassHyperlinkButton>()) return L"HyperlinkButton";
		if (element.try_as<W::LiquidGlassCheckBox>()) return L"CheckBox";
		if (element.try_as<W::LiquidGlassRadioButton>()) return L"RadioButton";
		if (element.try_as<W::LiquidGlassTextBox>()) return L"TextBox";
		if (element.try_as<W::LiquidGlassComboBox>()) return L"ComboBox";
		if (element.try_as<W::LiquidGlassCard>()) return L"Card";
		if (element.try_as<W::LiquidGlassFloatingPanel>()) return L"FloatingPanel";
		if (element.try_as<W::LiquidGlassTabBar>()) return L"TabBar";
		return {};
	}

	bool MaterialAppearance::IsGlass(FrameworkElement const& element)
	{
		if (!T::ControlAppearance::CanUseGlass()) return false;
		if (T::ControlAppearance::GetIsPreview(element)) return true;
		auto kind = Kind(element);
		if (kind.empty())
		{
			if (element.try_as<W::LiquidGlassSlider>()) kind = L"Slider";
			else if (element.try_as<W::LiquidGlassPasswordBox>()) kind = L"PasswordBox";
			else if (element.try_as<W::LiquidGlassSearchBox>()) kind = L"SearchBox";
		}
		auto state = T::MaterialControlState::Current();
		if (state.StyleIndex() != 1) return false;
		if (kind == L"Button") return state.ButtonEnabled();
		if (kind == L"ToggleButton") return state.ToggleButtonEnabled();
		if (kind == L"HyperlinkButton") return state.HyperlinkButtonEnabled();
		if (kind == L"CheckBox") return state.CheckBoxEnabled();
		if (kind == L"RadioButton") return state.RadioButtonEnabled();
		if (kind == L"TextBox") return state.TextBoxEnabled();
		if (kind == L"ComboBox") return state.ComboBoxEnabled();
		if (kind == L"ToggleSwitch") return state.ToggleSwitchEnabled();
		if (kind == L"Slider") return state.SliderEnabled();
		if (kind == L"PasswordBox") return state.PasswordBoxEnabled();
		if (kind == L"SearchBox") return state.SearchBoxEnabled();
		if (kind == L"Card") return state.CardEnabled();
		if (kind == L"FloatingPanel") return state.FloatingPanelEnabled();
		if (kind == L"TabBar") return state.TabBarEnabled();
		return false;
	}

	void MaterialAppearance::Release(FrameworkElement const& element)
	{
		if (T::ControlAppearance::IsShuttingDown()) return;
		if (auto control = element.try_as<Controls::Control>())
			for (auto& state : states) if (state.Control.get() == control && state.BackgroundToken)
			{
				control.UnregisterPropertyChangedCallback(Controls::Control::BackgroundProperty(), state.BackgroundToken);
				state.BackgroundToken = 0;
			}
		std::erase_if(states, [](auto const& item)
		{
			return !item.Control.get();
		});
	}

	void MaterialAppearance::Track(FrameworkElement const& element)
	{
		if (T::ControlAppearance::IsShuttingDown() || Kind(element).empty()) return;
		auto control = element.try_as<Controls::Control>();
		if (!control) return;
		try
		{
			std::erase_if(states, [](auto const& item)
			{
				return !item.Control.get();
			});
			auto found = std::find_if(states.begin(), states.end(), [&](auto const& item)
			{
				return item.Control.get() == control;
			});
			if (found == states.end())
			{
				State state;
				state.Control = make_weak(control); state.OriginalStyle = control.Style();
				state.HasLocalStyle = control.ReadLocalValue(FrameworkElement::StyleProperty()) != DependencyProperty::UnsetValue();
				state.CallerSetsBackground = state.HasLocalStyle && StyleSetsBackground(state.OriginalStyle);
				if (auto brush = control.ReadLocalValue(Controls::Control::BackgroundProperty()).try_as<WinUI::Composition::Hlsl::LiquidGlassBrush>()) state.GlassBackground = brush;
				state.Motion = { W::LiquidGlassInteraction::GetRestScale(element), W::LiquidGlassInteraction::GetPointerOverScale(element),
					W::LiquidGlassInteraction::GetPressedScale(element), W::LiquidGlassInteraction::GetFocusedScale(element),
					W::LiquidGlassInteraction::GetElasticity(element), W::LiquidGlassInteraction::GetPointerDisplacement(element) };
				states.push_back(std::move(state)); found = std::prev(states.end());
			}
			// Work on a copy: assigning Style can synchronously load another control.
			auto state = *found;
			if (*state.Applying) return;
			struct ApplyGuard
			{
				std::shared_ptr<bool> Flag;
				ApplyGuard(std::shared_ptr<bool> flag) : Flag(std::move(flag))
				{
					*Flag = true;
				}
				~ApplyGuard()
				{
					*Flag = false;
				}
			} guard{ state.Applying };
			auto glass = IsGlass(element);
			if (glass)
			{
				if (control.Style() != state.OriginalStyle)
				{
					if (state.OriginalStyle) control.Style(state.OriginalStyle);
					else control.ClearValue(FrameworkElement::StyleProperty());
				}
				auto local = control.ReadLocalValue(Controls::Control::BackgroundProperty());
				if (state.CallerSetsBackground)
				{
					if (local.try_as<WinUI::Composition::Hlsl::LiquidGlassBrush>()) control.ClearValue(Controls::Control::BackgroundProperty());
				}
				else if (state.GlassBackground && (local == DependencyProperty::UnsetValue() || local.try_as<WinUI::Composition::Hlsl::LiquidGlassBrush>()))
					control.Background(state.GlassBackground);
			}
			else
			{
				if (!state.HasLocalStyle)
				{
					auto key = box_value(L"MaterialNative" + Kind(element) + L"Style");
					auto resources = Application::Current().Resources();
					if (resources.HasKey(key)) if (auto style = resources.Lookup(key).try_as<Style>(); style && style != control.Style()) control.Style(style);
				}
				if (auto brush = control.ReadLocalValue(Controls::Control::BackgroundProperty()).try_as<WinUI::Composition::Hlsl::LiquidGlassBrush>())
				{
					if (!state.GlassBackground) state.GlassBackground = brush;
					control.ClearValue(Controls::Control::BackgroundProperty());
				}
				if (!state.BackgroundToken)
					state.BackgroundToken = control.RegisterPropertyChangedCallback(Controls::Control::BackgroundProperty(), [](auto const& sender, auto const&)
				{
					if (T::ControlAppearance::IsShuttingDown()) return;
					auto changed = sender.template as<Controls::Control>();
					if (!IsGlass(changed) && changed.ReadLocalValue(Controls::Control::BackgroundProperty()).template try_as<WinUI::Composition::Hlsl::LiquidGlassBrush>()) changed.ClearValue(Controls::Control::BackgroundProperty());
				});
			}
			ApplyMotion(element, state, glass);
			found = std::find_if(states.begin(), states.end(), [&](auto const& item)
			{
				return item.Control.get() == control;
			});
			if (found != states.end()) *found = std::move(state);
		}
		catch (...)
		{
			T::LiquidGlassProfile::ReportCurrentFailure(L"switch control appearance");
		}
	}
}
