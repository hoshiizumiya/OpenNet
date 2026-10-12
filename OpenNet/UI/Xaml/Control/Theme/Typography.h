#pragma once
#include "UI/Xaml/Control/Theme/Typography.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
    struct Typography : Typography_base<Typography>
    {
        Typography();
        static void EnsureProperties();
        static winrt::OpenNet::UI::Xaml::Control::Theme::Typography Current();
        static winrt::Microsoft::UI::Xaml::DependencyProperty ScaleProperty();
        double Scale();
        void Scale(double value);
        static winrt::hstring FormatScale(double value);
        static winrt::Microsoft::UI::Xaml::DependencyProperty BaseFontSizeProperty();
        static double GetBaseFontSize(winrt::Microsoft::UI::Xaml::DependencyObject const& target);
        static void SetBaseFontSize(winrt::Microsoft::UI::Xaml::DependencyObject const& target, double value);
        static void Track(winrt::Microsoft::UI::Xaml::FrameworkElement const& element, double baseSize);
    private:
        static void ScaleChanged(winrt::Microsoft::UI::Xaml::DependencyObject const&, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
        static void BaseSizeChanged(winrt::Microsoft::UI::Xaml::DependencyObject const&, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
        static void Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
    };
}
namespace winrt::OpenNet::UI::Xaml::Control::Theme::factory_implementation
{
    struct Typography : TypographyT<Typography, implementation::Typography> {};
}
