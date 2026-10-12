#pragma once
import std;
import winrt.WinUI.Composition.Hlsl;
import winrt.WinUI.LiquidGlass;

namespace OpenNet::UI::LiquidGlassOptics
{
    using Brush = winrt::WinUI::Composition::Hlsl::LiquidGlassBrush;
    struct Descriptor
    {
        wchar_t const* Key;
        wchar_t const* Label;
        double Minimum, Maximum, Step;
        double (*Read)(Brush const&);
        void (*Write)(Brush const&, double);
    };
    inline std::array<Descriptor, 30> const Descriptors{{
        { L"BlurRadius", L"Blur radius", 0.0, 32.0, 0.1, [](Brush const& b) { return b.BlurRadius(); }, [](Brush const& b, double v) { b.BlurRadius(v); } },
        { L"RefractionStrength", L"Refraction", 0.0, 64.0, 0.5, [](Brush const& b) { return b.RefractionStrength(); }, [](Brush const& b, double v) { b.RefractionStrength(v); } },
        { L"DispersionStrength", L"Chromatic dispersion", 0.0, 8.0, 0.05, [](Brush const& b) { return b.DispersionStrength(); }, [](Brush const& b, double v) { b.DispersionStrength(v); } },
        { L"HighlightStrength", L"Highlights", 0.0, 2.0, 0.05, [](Brush const& b) { return b.HighlightStrength(); }, [](Brush const& b, double v) { b.HighlightStrength(v); } },
        { L"HighlightSharpness", L"Highlight sharpness", 0.1, 4.0, 0.1, [](Brush const& b) { return b.HighlightSharpness(); }, [](Brush const& b, double v) { b.HighlightSharpness(v); } },
        { L"SpecularSaturation", L"Specular saturation", 0.0, 10.0, 0.1, [](Brush const& b) { return b.SpecularSaturation(); }, [](Brush const& b, double v) { b.SpecularSaturation(v); } },
        { L"SpecularWidth", L"Specular width", 0.1, 4.0, 0.1, [](Brush const& b) { return b.SpecularWidth(); }, [](Brush const& b, double v) { b.SpecularWidth(v); } },
        { L"RefractiveIndex", L"Refractive index", 1.0, 2.0, 0.01, [](Brush const& b) { return b.RefractiveIndex(); }, [](Brush const& b, double v) { b.RefractiveIndex(v); } },
        { L"TintOpacity", L"Tint opacity", 0.0, 1.0, 0.01, [](Brush const& b) { return b.TintOpacity(); }, [](Brush const& b, double v) { b.TintOpacity(v); } },
        { L"InnerShadowStrength", L"Inner shadow", 0.0, 1.0, 0.01, [](Brush const& b) { return b.InnerShadowStrength(); }, [](Brush const& b, double v) { b.InnerShadowStrength(v); } },
        { L"Saturation", L"Saturation", 0.0, 2.0, 0.05, [](Brush const& b) { return b.Saturation(); }, [](Brush const& b, double v) { b.Saturation(v); } },
        { L"Contrast", L"Contrast", 0.0, 2.0, 0.05, [](Brush const& b) { return b.Contrast(); }, [](Brush const& b, double v) { b.Contrast(v); } },
        { L"Exposure", L"Exposure", -2.0, 2.0, 0.05, [](Brush const& b) { return b.Exposure(); }, [](Brush const& b, double v) { b.Exposure(v); } },
        { L"MaterialOpacity", L"Material opacity", 0.1, 1.0, 0.01, [](Brush const& b) { return b.MaterialOpacity(); }, [](Brush const& b, double v) { b.MaterialOpacity(v); } },
        { L"DispersionRange", L"Dispersion range", 0.0, 1.0, 0.05, [](Brush const& b) { return b.DispersionRange(); }, [](Brush const& b, double v) { b.DispersionRange(v); } },
        { L"RefFresnelRange", L"Fresnel range", 0.0, 100.0, 1, [](Brush const& b) { return b.RefFresnelRange(); }, [](Brush const& b, double v) { b.RefFresnelRange(v); } },
        { L"RefFresnelHardness", L"Fresnel hardness", 0.0, 100.0, 1, [](Brush const& b) { return b.RefFresnelHardness(); }, [](Brush const& b, double v) { b.RefFresnelHardness(v); } },
        { L"RefFresnelFactor", L"Fresnel intensity", 0.0, 100.0, 1, [](Brush const& b) { return b.RefFresnelFactor(); }, [](Brush const& b, double v) { b.RefFresnelFactor(v); } },
        { L"GlareRange", L"Glare range", 0.0, 100.0, 1, [](Brush const& b) { return b.GlareRange(); }, [](Brush const& b, double v) { b.GlareRange(v); } },
        { L"GlareHardness", L"Glare hardness", 0.0, 100.0, 1, [](Brush const& b) { return b.GlareHardness(); }, [](Brush const& b, double v) { b.GlareHardness(v); } },
        { L"GlareFactor", L"Glare intensity", 0.0, 120.0, 1, [](Brush const& b) { return b.GlareFactor(); }, [](Brush const& b, double v) { b.GlareFactor(v); } },
        { L"GlareConvergence", L"Glare convergence", 0.0, 100.0, 1, [](Brush const& b) { return b.GlareConvergence(); }, [](Brush const& b, double v) { b.GlareConvergence(v); } },
        { L"GlareOppositeFactor", L"Opposite glare", 0.0, 100.0, 1, [](Brush const& b) { return b.GlareOppositeFactor(); }, [](Brush const& b, double v) { b.GlareOppositeFactor(v); } },
        { L"GlareAngle", L"Glare angle", -180.0, 180.0, 1, [](Brush const& b) { return b.GlareAngle(); }, [](Brush const& b, double v) { b.GlareAngle(v); } },
        { L"Magnification", L"Magnification", 1.0, 3.0, 0.05, [](Brush const& b) { return b.Magnification(); }, [](Brush const& b, double v) { b.Magnification(v); } },
        { L"MagnificationStrength", L"Magnification strength", 0.0, 64.0, 0.5, [](Brush const& b) { return b.MagnificationStrength(); }, [](Brush const& b, double v) { b.MagnificationStrength(v); } },
        { L"BloomAmount", L"Bloom", 0.0, 1.0, 0.05, [](Brush const& b) { return b.BloomAmount(); }, [](Brush const& b, double v) { b.BloomAmount(v); } },
        { L"Brightness", L"Brightness", -1.0, 1.0, 0.05, [](Brush const& b) { return b.Brightness(); }, [](Brush const& b, double v) { b.Brightness(v); } },
        { L"Temperature", L"Temperature", -1.0, 1.0, 0.05, [](Brush const& b) { return b.Temperature(); }, [](Brush const& b, double v) { b.Temperature(v); } },
        { L"Vibrance", L"Vibrance", 0.0, 1.0, 0.05, [](Brush const& b) { return b.Vibrance(); }, [](Brush const& b, double v) { b.Vibrance(v); } },
    }};
    inline constexpr auto Count = std::tuple_size_v<decltype(Descriptors)>;
    using Snapshot = std::array<double, Count + 3>;
    inline Snapshot Capture(Brush const& brush)
    {
        Snapshot result{};
        for (std::size_t i = 0; i < Descriptors.size(); ++i) result[i] = Descriptors[i].Read(brush);
        result[Count] = brush.BezelWidth(); result[Count + 1] = brush.GlassThickness(); result[Count + 2] = brush.CornerRadius();
        return result;
    }
    inline void Restore(Brush const& brush, Snapshot const& defaults)
    {
        for (std::size_t i = 0; i < Descriptors.size(); ++i) Descriptors[i].Write(brush, defaults[i]);
        brush.BezelWidth(defaults[Count]); brush.GlassThickness(defaults[Count + 1]); brush.CornerRadius(defaults[Count + 2]);
    }
    inline winrt::hstring Canonical(winrt::hstring const& kind)
    {
        if (kind == L"AutoSuggestBox") return L"SearchBox";
        if (kind == L"ToggleSwitchOff") return L"ToggleSwitch";
        return kind;
    }
    inline Brush Defaults(winrt::hstring const& kind)
    {
        using Preset = winrt::WinUI::LiquidGlass::LiquidGlassPreset;
        auto preset = Preset::Button;
        if (kind == L"Card") preset = Preset::Panel;
        else if (kind == L"FloatingPanel") preset = Preset::FloatingPanel;
        else if (kind == L"TabBar") preset = Preset::TabBar;
        else if (kind == L"SearchBox") preset = Preset::SearchBox;
        else if (kind == L"TextBox" || kind == L"PasswordBox") preset = Preset::Input;
        else if (kind == L"ComboBox" || kind == L"CheckBox" || kind == L"RadioButton") preset = Preset::Choice;
        else if (kind == L"Slider") preset = Preset::SliderThumb;
        else if (kind == L"ToggleSwitch") preset = Preset::ToggleSwitchKnob;
        auto brush = winrt::WinUI::LiquidGlass::LiquidGlassPresets::CreateBrush(preset);
        return brush;
    }
}
