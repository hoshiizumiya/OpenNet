// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License. See LICENSE in the project root for license information.
// Adapted for OpenNet from the WinUI AcrylicBrush recipe.

#pragma once

#include "UI/Xaml/Media/AcrylicBrush.g.h"

namespace winrt::OpenNet::UI::Xaml::Media::implementation
{
    struct AcrylicBrush : AcrylicBrushT<AcrylicBrush>
    {
        AcrylicBrush() = default;

        winrt::Windows::UI::Color TintColor() const;
        void TintColor(winrt::Windows::UI::Color const& value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty TintColorProperty();

        double TintOpacity() const;
        void TintOpacity(double value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty TintOpacityProperty();

        winrt::Windows::Foundation::IReference<double> TintLuminosityOpacity() const;
        void TintLuminosityOpacity(winrt::Windows::Foundation::IReference<double> const& value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty TintLuminosityOpacityProperty();

        winrt::Windows::Foundation::TimeSpan TintTransitionDuration() const;
        void TintTransitionDuration(winrt::Windows::Foundation::TimeSpan const& value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty TintTransitionDurationProperty();

        bool AlwaysUseFallback() const;
        void AlwaysUseFallback(bool value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty AlwaysUseFallbackProperty();

        bool IsNoiseEnabled() const;
        void IsNoiseEnabled(bool value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty IsNoiseEnabledProperty();

        void OnConnected();
        void OnDisconnected();

        // Public only so the dependency-property callback thunk can invoke it;
        // this is not projected as part of the WinRT API.
        static void OnBrushPropertyChanged(
            winrt::Microsoft::UI::Xaml::DependencyObject const& sender,
            winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);

    private:
        void OnFallbackColorChanged(
            winrt::Microsoft::UI::Xaml::DependencyObject const& sender,
            winrt::Microsoft::UI::Xaml::DependencyProperty const& property);

        void RebuildBrush();
        void ReleaseCompositionResources();
        winrt::Windows::UI::Color GetEffectiveTintColor() const;
        winrt::Windows::UI::Color GetEffectiveLuminosityColor() const;

        static winrt::Microsoft::UI::Xaml::DependencyProperty s_tintColorProperty;
        static winrt::Microsoft::UI::Xaml::DependencyProperty s_tintOpacityProperty;
        static winrt::Microsoft::UI::Xaml::DependencyProperty s_tintLuminosityOpacityProperty;
        static winrt::Microsoft::UI::Xaml::DependencyProperty s_tintTransitionDurationProperty;
        static winrt::Microsoft::UI::Xaml::DependencyProperty s_alwaysUseFallbackProperty;
        static winrt::Microsoft::UI::Xaml::DependencyProperty s_isNoiseEnabledProperty;

        bool m_isConnected{};
        int64_t m_fallbackColorChangedToken{};
        winrt::Microsoft::UI::Composition::CompositionBrush m_brush{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::LoadedImageSurface m_noiseSurface{ nullptr };
        winrt::Microsoft::UI::Composition::CompositionSurfaceBrush m_noiseBrush{ nullptr };
    };
}

namespace winrt::OpenNet::UI::Xaml::Media::factory_implementation
{
    struct AcrylicBrush : AcrylicBrushT<AcrylicBrush, implementation::AcrylicBrush>
    {
    };
}
