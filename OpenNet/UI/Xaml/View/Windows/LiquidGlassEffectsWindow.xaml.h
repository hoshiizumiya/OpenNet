#pragma once
#include "UI/Xaml/View/Windows/LiquidGlassEffectsWindow.g.h"
import OpenNet.Helpers.WindowExBase;
import winrt.OpenNet.UI.Xaml.Control.Theme;
namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
    struct LiquidGlassEffectsWindow : LiquidGlassEffectsWindowT<LiquidGlassEffectsWindow>, WindowExBase<LiquidGlassEffectsWindow>
    {
        LiquidGlassEffectsWindow();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassProfile> Profiles();
        winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassProfile SelectedProfile() const { return m_profile; }
        void TypeChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void ResetClicked(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        
    private:
        bool m_loading{ true };
        winrt::OpenNet::UI::Xaml::Control::Theme::LiquidGlassProfile m_profile{ nullptr };
    };
}
namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
    struct LiquidGlassEffectsWindow : LiquidGlassEffectsWindowT<LiquidGlassEffectsWindow, implementation::LiquidGlassEffectsWindow> {};
}
