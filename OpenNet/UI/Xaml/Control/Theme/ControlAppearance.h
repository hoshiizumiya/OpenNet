#pragma once
#include "UI/Xaml/Control/Theme/ControlAppearance.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
    // A control owns its behavior and values; a template owns its optical surfaces.
    struct ControlAppearance : ControlAppearanceT<ControlAppearance>
    {
        ControlAppearance() { EnsureProperties(); }
        static void EnsureProperties();
        static winrt::Microsoft::UI::Xaml::DependencyProperty IsMaterialHostProperty();
        static bool GetIsMaterialHost(winrt::Microsoft::UI::Xaml::DependencyObject const& target);
        static void SetIsMaterialHost(winrt::Microsoft::UI::Xaml::DependencyObject const& target, bool value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty IsEnabledProperty();
        static bool GetIsEnabled(winrt::Microsoft::UI::Xaml::DependencyObject const& target);
        static void SetIsEnabled(winrt::Microsoft::UI::Xaml::DependencyObject const& target, bool value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty SurfaceProperty();
        static winrt::hstring GetSurface(winrt::Microsoft::UI::Xaml::DependencyObject const& target);
        static void SetSurface(winrt::Microsoft::UI::Xaml::DependencyObject const& target, winrt::hstring const& value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty BrushProperty();
        static winrt::Microsoft::UI::Xaml::Media::Brush GetBrush(winrt::Microsoft::UI::Xaml::DependencyObject const& target);
        static void SetBrush(winrt::Microsoft::UI::Xaml::DependencyObject const& target, winrt::Microsoft::UI::Xaml::Media::Brush const& value);
        static void Refresh();
        static void Shutdown() noexcept;
        static void AdaptSurface(winrt::Microsoft::UI::Xaml::FrameworkElement const& element);
        static bool CanUseGlass();
        static winrt::Microsoft::UI::Xaml::DependencyProperty IsPreviewProperty();
        static bool GetIsPreview(winrt::Microsoft::UI::Xaml::DependencyObject const& target);
        static void SetIsPreview(winrt::Microsoft::UI::Xaml::DependencyObject const& target, bool value);

    private:
        static void EnabledChanged(winrt::Microsoft::UI::Xaml::DependencyObject const&, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
        static void SurfaceChanged(winrt::Microsoft::UI::Xaml::DependencyObject const&, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
        static void Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        static void Unloaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        static void SurfaceLoaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
    };
}
namespace winrt::OpenNet::UI::Xaml::Control::Theme::factory_implementation
{
    struct ControlAppearance : ControlAppearanceT<ControlAppearance, implementation::ControlAppearance> {};
}
