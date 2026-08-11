// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.
// Adapted for OpenNet from the WinUI AcrylicBrush recipe.

#include "XamlWorkaround.h"
#include "UI/Xaml/Media/AcrylicBrush.h"

#if __has_include("UI/Xaml/Media/AcrylicBrush.g.cpp")
#include "UI/Xaml/Media/AcrylicBrush.g.cpp"
#endif

import winrt.Windows.Graphics.Effects;
import winrt.Windows.UI;
import winrt.Microsoft.Graphics.Canvas;
import winrt.Microsoft.Graphics.Canvas.Effects;
import winrt.Microsoft.UI.Composition;
import winrt.Microsoft.UI.Xaml.Media.Imaging;

using namespace std::chrono_literals;
using namespace winrt;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Microsoft::Graphics::Canvas::Effects;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Effects;

namespace
{
    constexpr float BlurRadius = 30.0f;
    constexpr float NoiseOpacity = 0.02f;
    constexpr wchar_t TintColorPropertyName[] = L"TintColor.Color";
    constexpr wchar_t LuminosityColorPropertyName[] = L"LuminosityColor.Color";
    constexpr wchar_t OpaqueFallbackColorPropertyName[] = L"OpaqueFallbackColor.Color";
    constexpr wchar_t NoiseOpacityPropertyName[] = L"NoiseOpacity.Opacity";
    constexpr wchar_t NoiseAssetUri[] = L"ms-appx:///Assets/Materials/NoiseAsset_256X256_PNG.png";

    struct Rgb
    {
        double r{};
        double g{};
        double b{};
    };

    struct Hsv
    {
        double h{};
        double s{};
        double v{};
    };

    Rgb ToRgb(Windows::UI::Color const& color)
    {
        return { color.R / 255.0, color.G / 255.0, color.B / 255.0 };
    }

    Hsv ToHsv(Rgb const& rgb)
    {
        double const maximum = std::max({ rgb.r, rgb.g, rgb.b });
        double const minimum = std::min({ rgb.r, rgb.g, rgb.b });
        double const delta = maximum - minimum;
        Hsv hsv{ 0.0, maximum == 0.0 ? 0.0 : delta / maximum, maximum };

        if (delta != 0.0)
        {
            if (maximum == rgb.r)
            {
                hsv.h = std::fmod((rgb.g - rgb.b) / delta, 6.0);
            }
            else if (maximum == rgb.g)
            {
                hsv.h = ((rgb.b - rgb.r) / delta) + 2.0;
            }
            else
            {
                hsv.h = ((rgb.r - rgb.g) / delta) + 4.0;
            }

            hsv.h *= 60.0;
            if (hsv.h < 0.0)
            {
                hsv.h += 360.0;
            }
        }

        return hsv;
    }

    Rgb ToRgb(Hsv const& hsv)
    {
        double const chroma = hsv.v * hsv.s;
        double const x = chroma * (1.0 - std::abs(std::fmod(hsv.h / 60.0, 2.0) - 1.0));
        double const match = hsv.v - chroma;
        Rgb rgb{};

        if (hsv.h < 60.0) rgb = { chroma, x, 0.0 };
        else if (hsv.h < 120.0) rgb = { x, chroma, 0.0 };
        else if (hsv.h < 180.0) rgb = { 0.0, chroma, x };
        else if (hsv.h < 240.0) rgb = { 0.0, x, chroma };
        else if (hsv.h < 300.0) rgb = { x, 0.0, chroma };
        else rgb = { chroma, 0.0, x };

        rgb.r += match;
        rgb.g += match;
        rgb.b += match;
        return rgb;
    }

    Windows::UI::Color ToColor(Rgb const& rgb, double alpha)
    {
        auto channel = [](double value)
        {
            return static_cast<uint8_t>(std::round(std::clamp(value, 0.0, 1.0) * 255.0));
        };
        return { channel(alpha), channel(rgb.r), channel(rgb.g), channel(rgb.b) };
    }

    double GetTintOpacityModifier(Windows::UI::Color const& tintColor)
    {
        constexpr double midpoint = 0.50;
        constexpr double whiteMaxOpacity = 0.45;
        constexpr double midpointMaxOpacity = 0.90;
        constexpr double blackMaxOpacity = 0.85;
        Hsv const hsv = ToHsv(ToRgb(tintColor));
        double opacityModifier = midpointMaxOpacity;

        if (hsv.v != midpoint)
        {
            double const lowestMaxOpacity = hsv.v > midpoint ? whiteMaxOpacity : blackMaxOpacity;
            double const maximumDeviation = midpoint;
            double maximumSuppression = midpointMaxOpacity - lowestMaxOpacity;
            if (hsv.s > 0.0)
            {
                maximumSuppression *= std::max(1.0 - (hsv.s * 2.0), 0.0);
            }
            double const normalizedDeviation = std::abs(hsv.v - midpoint) / maximumDeviation;
            opacityModifier -= maximumSuppression * normalizedDeviation;
        }

        return opacityModifier;
    }

    IGraphicsEffectSource AsEffectSource(auto const& effect)
    {
        return effect.template as<IGraphicsEffectSource>();
    }
}

namespace winrt::OpenNet::UI::Xaml::Media::implementation
{
    namespace Runtime = OpenNet::UI::Xaml::Media;

    void AcrylicBrush::OnBrushPropertyChanged(
        DependencyObject const& sender,
        DependencyPropertyChangedEventArgs const&)
    {
        if (auto brush = sender.try_as<Runtime::AcrylicBrush>())
        {
            get_self<AcrylicBrush>(brush)->RebuildBrush();
        }
    }

    namespace
    {
        PropertyChangedCallback BrushPropertyChangedCallback()
        {
            return PropertyChangedCallback{ [](DependencyObject const& sender, DependencyPropertyChangedEventArgs const& args)
            {
                AcrylicBrush::OnBrushPropertyChanged(sender, args);
            } };
        }
    }

    DependencyProperty AcrylicBrush::s_tintColorProperty = DependencyProperty::Register(
        L"TintColor", xaml_typename<Windows::UI::Color>(), xaml_typename<Runtime::AcrylicBrush>(),
        PropertyMetadata{ box_value(Windows::UI::Color{ 204, 255, 255, 255 }), BrushPropertyChangedCallback() });
    DependencyProperty AcrylicBrush::s_tintOpacityProperty = DependencyProperty::Register(
        L"TintOpacity", xaml_typename<double>(), xaml_typename<Runtime::AcrylicBrush>(),
        PropertyMetadata{ box_value(1.0), BrushPropertyChangedCallback() });
    DependencyProperty AcrylicBrush::s_tintLuminosityOpacityProperty = DependencyProperty::Register(
        L"TintLuminosityOpacity", xaml_typename<IReference<double>>(), xaml_typename<Runtime::AcrylicBrush>(),
        PropertyMetadata{ nullptr, BrushPropertyChangedCallback() });
    DependencyProperty AcrylicBrush::s_tintTransitionDurationProperty = DependencyProperty::Register(
        L"TintTransitionDuration", xaml_typename<TimeSpan>(), xaml_typename<Runtime::AcrylicBrush>(),
        PropertyMetadata{ box_value(TimeSpan{ 500ms }), BrushPropertyChangedCallback() });
    DependencyProperty AcrylicBrush::s_alwaysUseFallbackProperty = DependencyProperty::Register(
        L"AlwaysUseFallback", xaml_typename<bool>(), xaml_typename<Runtime::AcrylicBrush>(),
        PropertyMetadata{ box_value(false), BrushPropertyChangedCallback() });
    DependencyProperty AcrylicBrush::s_isNoiseEnabledProperty = DependencyProperty::Register(
        L"IsNoiseEnabled", xaml_typename<bool>(), xaml_typename<Runtime::AcrylicBrush>(),
        PropertyMetadata{ box_value(true), BrushPropertyChangedCallback() });

    Windows::UI::Color AcrylicBrush::TintColor() const { return unbox_value<Windows::UI::Color>(GetValue(s_tintColorProperty)); }
    void AcrylicBrush::TintColor(Windows::UI::Color const& value) { SetValue(s_tintColorProperty, box_value(value)); }
    DependencyProperty AcrylicBrush::TintColorProperty() { return s_tintColorProperty; }

    double AcrylicBrush::TintOpacity() const { return unbox_value<double>(GetValue(s_tintOpacityProperty)); }
    void AcrylicBrush::TintOpacity(double value) { SetValue(s_tintOpacityProperty, box_value(std::clamp(value, 0.0, 1.0))); }
    DependencyProperty AcrylicBrush::TintOpacityProperty() { return s_tintOpacityProperty; }

    IReference<double> AcrylicBrush::TintLuminosityOpacity() const { return GetValue(s_tintLuminosityOpacityProperty).try_as<IReference<double>>(); }
    void AcrylicBrush::TintLuminosityOpacity(IReference<double> const& value) { SetValue(s_tintLuminosityOpacityProperty, value); }
    DependencyProperty AcrylicBrush::TintLuminosityOpacityProperty() { return s_tintLuminosityOpacityProperty; }

    TimeSpan AcrylicBrush::TintTransitionDuration() const { return unbox_value<TimeSpan>(GetValue(s_tintTransitionDurationProperty)); }
    void AcrylicBrush::TintTransitionDuration(TimeSpan const& value) { SetValue(s_tintTransitionDurationProperty, box_value(value)); }
    DependencyProperty AcrylicBrush::TintTransitionDurationProperty() { return s_tintTransitionDurationProperty; }

    bool AcrylicBrush::AlwaysUseFallback() const { return unbox_value<bool>(GetValue(s_alwaysUseFallbackProperty)); }
    void AcrylicBrush::AlwaysUseFallback(bool value) { SetValue(s_alwaysUseFallbackProperty, box_value(value)); }
    DependencyProperty AcrylicBrush::AlwaysUseFallbackProperty() { return s_alwaysUseFallbackProperty; }

    bool AcrylicBrush::IsNoiseEnabled() const { return unbox_value<bool>(GetValue(s_isNoiseEnabledProperty)); }
    void AcrylicBrush::IsNoiseEnabled(bool value) { SetValue(s_isNoiseEnabledProperty, box_value(value)); }
    DependencyProperty AcrylicBrush::IsNoiseEnabledProperty() { return s_isNoiseEnabledProperty; }

    void AcrylicBrush::OnConnected()
    {
        m_isConnected = true;
        if (m_fallbackColorChangedToken == 0)
        {
            m_fallbackColorChangedToken = RegisterPropertyChangedCallback(
                XamlCompositionBrushBase::FallbackColorProperty(),
                { this, &AcrylicBrush::OnFallbackColorChanged });
        }
        RebuildBrush();
    }

    void AcrylicBrush::OnDisconnected()
    {
        m_isConnected = false;
        if (m_fallbackColorChangedToken != 0)
        {
            UnregisterPropertyChangedCallback(XamlCompositionBrushBase::FallbackColorProperty(), m_fallbackColorChangedToken);
            m_fallbackColorChangedToken = 0;
        }
        ReleaseCompositionResources();
    }

    void AcrylicBrush::OnFallbackColorChanged(DependencyObject const&, DependencyProperty const&)
    {
        RebuildBrush();
    }

    Windows::UI::Color AcrylicBrush::GetEffectiveTintColor() const
    {
        auto color = TintColor();
        double opacity = TintOpacity();
        if (!TintLuminosityOpacity())
        {
            opacity *= GetTintOpacityModifier(color);
        }
        color.A = static_cast<uint8_t>(std::round(color.A * opacity));
        return color;
    }

    Windows::UI::Color AcrylicBrush::GetEffectiveLuminosityColor() const
    {
        auto tintColor = TintColor();
        tintColor.A = static_cast<uint8_t>(std::round(tintColor.A * TintOpacity()));
        Rgb const tintRgb = ToRgb(tintColor);

        if (auto opacity = TintLuminosityOpacity())
        {
            return ToColor(tintRgb, std::clamp(opacity.Value(), 0.0, 1.0));
        }

        Hsv hsv = ToHsv(tintRgb);
        hsv.v = std::clamp(hsv.v, 0.125, 0.965);
        double const mappedOpacity = std::min(((tintColor.A / 255.0) * (1.03 - 0.15)) + 0.15, 1.0);
        return ToColor(ToRgb(hsv), mappedOpacity);
    }

    void AcrylicBrush::ReleaseCompositionResources()
    {
        CompositionBrush(nullptr);
        if (m_brush)
        {
            m_brush.Close();
            m_brush = nullptr;
        }
        if (m_noiseBrush)
        {
            m_noiseBrush.Close();
            m_noiseBrush = nullptr;
        }
        if (m_noiseSurface)
        {
            m_noiseSurface.Close();
            m_noiseSurface = nullptr;
        }
    }

    void AcrylicBrush::RebuildBrush()
    {
        if (!m_isConnected)
        {
            return;
        }

        ReleaseCompositionResources();
        auto const compositor = CompositionTarget::GetCompositorForCurrentThread();
        auto const fallbackColor = FallbackColor();

        try
        {
            if (AlwaysUseFallback())
            {
                m_brush = compositor.CreateColorBrush(fallbackColor);
                CompositionBrush(m_brush);
                return;
            }

            auto const tintColor = GetEffectiveTintColor();
            auto const luminosityColor = GetEffectiveLuminosityColor();

            ColorSourceEffect tintEffect;
            tintEffect.Name(L"TintColor");
            tintEffect.Color(tintColor);
            IGraphicsEffectSource tintOutput = AsEffectSource(tintEffect);

            if (tintColor.A != 255)
            {
                ColorSourceEffect opaqueFallbackEffect;
                opaqueFallbackEffect.Name(L"OpaqueFallbackColor");
                auto opaqueFallbackColor = fallbackColor;
                opaqueFallbackColor.A = 255;
                opaqueFallbackEffect.Color(opaqueFallbackColor);

                CompositionEffectSourceParameter backdropParameter{ L"Backdrop" };
                CompositeEffect backdropComposite;
                backdropComposite.Mode(CanvasComposite::SourceOver);
                backdropComposite.Sources().Append(AsEffectSource(opaqueFallbackEffect));
                backdropComposite.Sources().Append(backdropParameter);

                GaussianBlurEffect blurEffect;
                blurEffect.Name(L"Blur");
                blurEffect.BlurAmount(BlurRadius);
                blurEffect.BorderMode(EffectBorderMode::Hard);
                blurEffect.Source(AsEffectSource(backdropComposite));

                ColorSourceEffect luminosityEffect;
                luminosityEffect.Name(L"LuminosityColor");
                luminosityEffect.Color(luminosityColor);

                BlendEffect luminosityBlend;
                luminosityBlend.Mode(BlendEffectMode::Luminosity);
                luminosityBlend.Background(AsEffectSource(blurEffect));
                luminosityBlend.Foreground(AsEffectSource(luminosityEffect));

                BlendEffect colorBlend;
                colorBlend.Mode(BlendEffectMode::Color);
                colorBlend.Background(AsEffectSource(luminosityBlend));
                colorBlend.Foreground(AsEffectSource(tintEffect));
                tintOutput = AsEffectSource(colorBlend);
            }

            BorderEffect noiseBorder;
            noiseBorder.ExtendX(CanvasEdgeBehavior::Wrap);
            noiseBorder.ExtendY(CanvasEdgeBehavior::Wrap);
            noiseBorder.Source(CompositionEffectSourceParameter{ L"Noise" });

            OpacityEffect noiseOpacity;
            noiseOpacity.Name(L"NoiseOpacity");
            noiseOpacity.Opacity(IsNoiseEnabled() ? NoiseOpacity : 0.0f);
            noiseOpacity.Source(AsEffectSource(noiseBorder));

            CompositeEffect output;
            output.Mode(CanvasComposite::SourceOver);
            output.Sources().Append(tintOutput);
            output.Sources().Append(AsEffectSource(noiseOpacity));

            std::vector<hstring> animatableProperties{
                TintColorPropertyName,
                LuminosityColorPropertyName,
                OpaqueFallbackColorPropertyName,
                NoiseOpacityPropertyName,
            };
            auto factory = compositor.CreateEffectFactory(output, animatableProperties);
            auto effectBrush = factory.CreateBrush();

            if (tintColor.A != 255)
            {
                effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());
            }

            if (IsNoiseEnabled())
            {
                m_noiseSurface = LoadedImageSurface::StartLoadFromUri(Uri{ NoiseAssetUri });
                m_noiseBrush = compositor.CreateSurfaceBrush(m_noiseSurface);
                m_noiseBrush.Stretch(CompositionStretch::None);
                effectBrush.SetSourceParameter(L"Noise", m_noiseBrush);
            }
            else
            {
                effectBrush.SetSourceParameter(L"Noise", compositor.CreateColorBrush(Windows::UI::Color{}));
            }

            effectBrush.Properties().InsertColor(TintColorPropertyName, tintColor);
            effectBrush.Properties().InsertScalar(NoiseOpacityPropertyName, IsNoiseEnabled() ? NoiseOpacity : 0.0f);
            if (tintColor.A != 255)
            {
                effectBrush.Properties().InsertColor(LuminosityColorPropertyName, luminosityColor);
                auto opaqueFallbackColor = fallbackColor;
                opaqueFallbackColor.A = 255;
                effectBrush.Properties().InsertColor(OpaqueFallbackColorPropertyName, opaqueFallbackColor);
            }

            m_brush = effectBrush;
            CompositionBrush(m_brush);
        }
        catch (...)
        {
            ReleaseCompositionResources();
            m_brush = compositor.CreateColorBrush(fallbackColor);
            CompositionBrush(m_brush);
        }
    }
}
