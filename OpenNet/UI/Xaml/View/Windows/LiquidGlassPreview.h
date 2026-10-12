#pragma once
#include "UI/Xaml/Control/Theme/ControlAppearance.h"
#include "UI/Xaml/Control/Theme/MaterialControlState.h"
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Shapes;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Microsoft.UI.Xaml.Controls.Primitives;

namespace OpenNet::UI
{
    // One real native sample control; the same appearance pipeline as the application.
    inline winrt::Microsoft::UI::Xaml::UIElement GlassPreview(winrt::hstring const& kind, bool forceGlass)
    {
        using namespace winrt;
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        using Appearance = winrt::OpenNet::UI::Xaml::Control::Theme::implementation::ControlAppearance;
        Grid scene;
        scene.MinHeight(300);
        Media::LinearGradientBrush background;
        Media::GradientStop first; first.Color({255, 36, 82, 126}); first.Offset(0);
        Media::GradientStop last; last.Color({255, 89, 41, 104}); last.Offset(1);
        background.GradientStops().Append(first); background.GradientStops().Append(last);
        background.StartPoint({0, 0}); background.EndPoint({1, 1});
        scene.Background(background);
        auto updateTheme = [weak = make_weak(scene), first, last](auto const&, auto const&)
        {
            if (auto current = weak.get())
            {
                auto dark = current.ActualTheme() == ElementTheme::Dark;
                first.Color(dark ? Windows::UI::Color{255, 36, 82, 126} : Windows::UI::Color{255, 224, 242, 250});
                last.Color(dark ? Windows::UI::Color{255, 89, 41, 104} : Windows::UI::Color{255, 244, 229, 246});
            }
        };
        scene.Loaded(updateTheme);
        scene.ActualThemeChanged(updateTheme);
        Canvas shapes;
        shapes.IsHitTestVisible(false);
        Shapes::Ellipse blue; blue.Width(180); blue.Height(180); blue.Fill(Media::SolidColorBrush{ Windows::UI::Color{255, 39, 188, 208} });
        Canvas::SetLeft(blue, 10); Canvas::SetTop(blue, 45); shapes.Children().Append(blue);
        Shapes::Ellipse pink; pink.Width(145); pink.Height(145); pink.Fill(Media::SolidColorBrush{ Windows::UI::Color{255, 232, 91, 142} });
        Canvas::SetLeft(pink, 210); Canvas::SetTop(pink, 130); shapes.Children().Append(pink);
        Shapes::Rectangle stripe; stripe.Width(380); stripe.Height(28); stripe.Fill(Media::SolidColorBrush{ Windows::UI::Color{255, 246, 199, 79} });
        Canvas::SetLeft(stripe, 0); Canvas::SetTop(stripe, 130); shapes.Children().Append(stripe);
        scene.Children().Append(shapes);

        auto build = [kind, forceGlass](bool glass) -> FrameworkElement
        {
            namespace W = WinUI::LiquidGlass;
            Control sample{ nullptr };
            if (kind == L"Button") { Button c = glass ? W::LiquidGlassButton().as<Button>() : Button{}; c.Content(box_value(L"Preview button")); sample = c; }
            else if (kind == L"ToggleButton") { Primitives::ToggleButton c = glass ? W::LiquidGlassToggleButton().as<Primitives::ToggleButton>() : Primitives::ToggleButton{}; c.Content(box_value(L"Toggle me")); sample = c; }
            else if (kind == L"HyperlinkButton") { HyperlinkButton c = glass ? W::LiquidGlassHyperlinkButton().as<HyperlinkButton>() : HyperlinkButton{}; c.Content(box_value(L"Preview link")); sample = c; }
            else if (kind == L"CheckBox") { CheckBox c = glass ? W::LiquidGlassCheckBox().as<CheckBox>() : CheckBox{}; c.Content(box_value(L"Check option")); sample = c; }
            else if (kind == L"RadioButton") { RadioButton c = glass ? W::LiquidGlassRadioButton().as<RadioButton>() : RadioButton{}; c.Content(box_value(L"Select option")); sample = c; }
            else if (kind == L"Slider") { Slider c = glass ? W::LiquidGlassSlider().as<Slider>() : Slider{}; c.Width(270); c.Minimum(0); c.Maximum(100); c.Value(45); sample = c; }
            else if (kind == L"TextBox") { TextBox c = glass ? W::LiquidGlassTextBox().as<TextBox>() : TextBox{}; c.Width(260); c.PlaceholderText(L"Type to preview focus"); sample = c; }
            else if (kind == L"ComboBox") { ComboBox c = glass ? W::LiquidGlassComboBox().as<ComboBox>() : ComboBox{}; c.Width(240); c.Items().Append(box_value(L"First option")); c.Items().Append(box_value(L"Second option")); c.SelectedIndex(0); sample = c; }
            else if (kind == L"ToggleSwitch")
            {
                if (glass) { W::LiquidGlassToggleSwitch c; c.IsOn(true); c.OnContent(box_value(L"On")); c.OffContent(box_value(L"Off")); sample = c; }
                else { ToggleSwitch c; c.IsOn(true); sample = c; }
            }
            else if (kind == L"SearchBox")
            {
                if (glass) { W::LiquidGlassSearchBox c; c.Width(260); c.PlaceholderText(L"Search preview"); sample = c; }
                else { AutoSuggestBox c; c.Width(260); c.PlaceholderText(L"Search preview"); sample = c; }
            }
            else if (kind == L"PasswordBox")
            {
                if (glass) { W::LiquidGlassPasswordBox c; c.Width(260); c.PlaceholderText(L"Password preview"); sample = c; }
                else { PasswordBox c; c.Width(260); c.PlaceholderText(L"Password preview"); sample = c; }
            }
            else if (kind == L"TabBar" && glass)
            {
                W::LiquidGlassTabBar c;
                c.Items().Append(box_value(L"Overview")); c.Items().Append(box_value(L"Details")); c.Items().Append(box_value(L"Activity"));
                c.SelectedIndex(0); c.Width(330); sample = c;
            }
            else
            {
                ContentControl c;
                if (glass && kind == L"Card") c = W::LiquidGlassCard();
                if (glass && kind == L"FloatingPanel") c = W::LiquidGlassFloatingPanel();
                TextBlock text; text.Text(L"Glass surface\nContent remains readable"); text.TextWrapping(TextWrapping::Wrap);
                c.Content(text); c.Padding({24,24,24,24}); c.MaxWidth(330); sample = c;
            }
            sample.HorizontalAlignment(HorizontalAlignment::Center); sample.VerticalAlignment(VerticalAlignment::Center);
            if (glass)
            {
                Appearance::SetIsPreview(sample, forceGlass);
                winrt::OpenNet::UI::Xaml::Control::Theme::implementation::MaterialControlState::SetIsManaged(sample, true);
            }
            return sample;
        };
        if (forceGlass) scene.Children().Append(build(true));
        else
        {
            WinUI3Package::SwitchPresenter host;
            WinUI3Package::Case nativeCase, glassCase;
            nativeCase.Value(box_value(0)); nativeCase.Content(build(false));
            glassCase.Value(box_value(1)); glassCase.Content(build(true));
            host.SwitchCases().Append(nativeCase); host.SwitchCases().Append(glassCase);
            Data::Binding selection; selection.Source(winrt::OpenNet::UI::Xaml::Control::Theme::implementation::MaterialControlState::Current());
            selection.Path(PropertyPath{ kind + L"Material" }); selection.Mode(Data::BindingMode::OneWay);
            host.SetBinding(WinUI3Package::SwitchPresenter::ValueProperty(), selection);
            Appearance::SetIsMaterialHost(host, true);
            scene.Children().Append(host);
        }
        return scene;
    }
}
