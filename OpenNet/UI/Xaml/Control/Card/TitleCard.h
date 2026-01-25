#pragma once

#include "UI/Xaml/Control/Card/TitleCard.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::Card::implementation
{
    struct TitleCard : TitleCardT<TitleCard>
    {
        TitleCard();

        // DependencyProperty accessors (C++/WinRT pattern)
        static winrt::Microsoft::UI::Xaml::DependencyProperty TitleProperty();
        static winrt::Microsoft::UI::Xaml::DependencyProperty TitleContentProperty();
        static winrt::Microsoft::UI::Xaml::DependencyProperty ContentProperty();
        static winrt::Microsoft::UI::Xaml::DependencyProperty DividerVisibilityProperty();

        // Properties
        winrt::hstring Title();
        void Title(winrt::hstring const& value);

        winrt::Windows::Foundation::IInspectable TitleContent();
        void TitleContent(winrt::Windows::Foundation::IInspectable const& value);

        winrt::Windows::Foundation::IInspectable Content();
        void Content(winrt::Windows::Foundation::IInspectable const& value);

        winrt::Microsoft::UI::Xaml::Visibility DividerVisibility();
        void DividerVisibility(winrt::Microsoft::UI::Xaml::Visibility const& value);

        // Template
        void OnApplyTemplate();

    private:
        static void OnVisualPropertyChanged(
            winrt::Microsoft::UI::Xaml::DependencyObject const& d,
            winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

        void SetVisualStates();
    };
}

namespace winrt::OpenNet::UI::Xaml::Control::Card::factory_implementation
{
    struct TitleCard : TitleCardT<TitleCard, implementation::TitleCard>
    {
    };
}
