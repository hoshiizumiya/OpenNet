#include "pch.h"
#include "TitleCard.h"
#if __has_include("UI/Xaml/Control/Card/TitleCard.g.cpp")
#include "UI/Xaml/Control/Card/TitleCard.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::Control::Card::implementation
{
    TitleCard::TitleCard()
    {
        DefaultStyleKey(winrt::box_value(winrt::xaml_typename<class_type>()));
    }

    Microsoft::UI::Xaml::DependencyProperty TitleCard::TitleProperty()
    {
        static Microsoft::UI::Xaml::DependencyProperty s_property =
            Microsoft::UI::Xaml::DependencyProperty::Register(
                L"Title",
                winrt::xaml_typename<hstring>(),
                winrt::xaml_typename<OpenNet::UI::Xaml::Control::Card::TitleCard>(),
                Microsoft::UI::Xaml::PropertyMetadata{ nullptr, Microsoft::UI::Xaml::PropertyChangedCallback{ &TitleCard::OnVisualPropertyChanged } });

        return s_property;
    }

    Microsoft::UI::Xaml::DependencyProperty TitleCard::TitleContentProperty()
    {
        static Microsoft::UI::Xaml::DependencyProperty s_property =
            Microsoft::UI::Xaml::DependencyProperty::Register(
                L"TitleContent",
                winrt::xaml_typename<Windows::Foundation::IInspectable>(),
                winrt::xaml_typename<OpenNet::UI::Xaml::Control::Card::TitleCard>(),
                Microsoft::UI::Xaml::PropertyMetadata{ nullptr, Microsoft::UI::Xaml::PropertyChangedCallback{ &TitleCard::OnVisualPropertyChanged } });

        return s_property;
    }

    Microsoft::UI::Xaml::DependencyProperty TitleCard::ContentProperty()
    {
        static Microsoft::UI::Xaml::DependencyProperty s_property =
            Microsoft::UI::Xaml::DependencyProperty::Register(
                L"Content",
                winrt::xaml_typename<Windows::Foundation::IInspectable>(),
                winrt::xaml_typename<OpenNet::UI::Xaml::Control::Card::TitleCard>(),
                Microsoft::UI::Xaml::PropertyMetadata{ nullptr });

        return s_property;
    }

    Microsoft::UI::Xaml::DependencyProperty TitleCard::DividerVisibilityProperty()
    {
        static Microsoft::UI::Xaml::DependencyProperty s_property =
            Microsoft::UI::Xaml::DependencyProperty::Register(
                L"DividerVisibility",
                winrt::xaml_typename<Microsoft::UI::Xaml::Visibility>(),
                winrt::xaml_typename<OpenNet::UI::Xaml::Control::Card::TitleCard>(),
                Microsoft::UI::Xaml::PropertyMetadata{ winrt::box_value(Microsoft::UI::Xaml::Visibility::Visible) });

        return s_property;
    }

    hstring TitleCard::Title()
    {
        return winrt::unbox_value_or<hstring>(GetValue(TitleProperty()), L"");
    }

    void TitleCard::Title(hstring const& value)
    {
        SetValue(TitleProperty(), winrt::box_value(value));
    }

    Windows::Foundation::IInspectable TitleCard::TitleContent()
    {
        return GetValue(TitleContentProperty());
    }

    void TitleCard::TitleContent(Windows::Foundation::IInspectable const& value)
    {
        SetValue(TitleContentProperty(), value);
    }

    Windows::Foundation::IInspectable TitleCard::Content()
    {
        return GetValue(ContentProperty());
    }

    void TitleCard::Content(Windows::Foundation::IInspectable const& value)
    {
        SetValue(ContentProperty(), value);
    }

    Microsoft::UI::Xaml::Visibility TitleCard::DividerVisibility()
    {
        return winrt::unbox_value<Microsoft::UI::Xaml::Visibility>(GetValue(DividerVisibilityProperty()));
    }

    void TitleCard::DividerVisibility(Microsoft::UI::Xaml::Visibility const& value)
    {
        SetValue(DividerVisibilityProperty(), winrt::box_value(value));
    }

    void TitleCard::OnApplyTemplate()
    {
        TitleCardT<TitleCard>::OnApplyTemplate();
        SetVisualStates();
    }

    void TitleCard::OnVisualPropertyChanged(
        Microsoft::UI::Xaml::DependencyObject const& d,
        Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&)
    {
        if (auto card = d.try_as<OpenNet::UI::Xaml::Control::Card::TitleCard>())
        {
            if (auto impl = winrt::get_self<implementation::TitleCard>(card))
            {
                impl->SetVisualStates();
            }
        }
    }

    void TitleCard::SetVisualStates()
    {
        bool const titleEmpty = Title().empty();
        bool const titleContentNull = (TitleContent() == nullptr);

        if (titleEmpty && titleContentNull)
        {
            Microsoft::UI::Xaml::VisualStateManager::GoToState(*this, L"TitleGridCollapsed", true);
            DividerVisibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        }
        else
        {
            Microsoft::UI::Xaml::VisualStateManager::GoToState(*this, L"TitleGridVisible", true);
            DividerVisibility(Microsoft::UI::Xaml::Visibility::Visible);
        }
    }
}
