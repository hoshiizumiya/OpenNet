#pragma once
#include "pch.h"
#include "Helpers/NavItemIconHelper.g.h"

namespace winrt::OpenNet::Helpers::implementation
{
    struct NavItemIconHelper : NavItemIconHelperT<NavItemIconHelper>
    {
        NavItemIconHelper() = default;

        static winrt::Microsoft::UI::Xaml::DependencyProperty SelectedIconProperty();
        static winrt::Microsoft::UI::Xaml::DependencyProperty ShowNotificationDotProperty();
        static winrt::Microsoft::UI::Xaml::DependencyProperty UnselectedIconProperty();
        static winrt::Microsoft::UI::Xaml::DependencyProperty StaticIconVisibilityProperty();

        // 获取和设置附加属性的方法
        static winrt::Windows::Foundation::IInspectable GetSelectedIcon(winrt::Microsoft::UI::Xaml::DependencyObject const& obj);
        static void SetSelectedIcon(winrt::Microsoft::UI::Xaml::DependencyObject const& obj, winrt::Windows::Foundation::IInspectable const& value);
        static bool GetShowNotificationDot(winrt::Microsoft::UI::Xaml::DependencyObject const& obj);
        static void SetShowNotificationDot(winrt::Microsoft::UI::Xaml::DependencyObject const& obj, bool value);
        static winrt::Windows::Foundation::IInspectable GetUnselectedIcon(winrt::Microsoft::UI::Xaml::DependencyObject const& obj);
        static void SetUnselectedIcon(winrt::Microsoft::UI::Xaml::DependencyObject const& obj, winrt::Windows::Foundation::IInspectable const& value);
        static winrt::Microsoft::UI::Xaml::Visibility GetStaticIconVisibility(winrt::Microsoft::UI::Xaml::DependencyObject const& obj);
        static void SetStaticIconVisibility(winrt::Microsoft::UI::Xaml::DependencyObject const& obj, winrt::Microsoft::UI::Xaml::Visibility const& value);

    private:
        static winrt::Microsoft::UI::Xaml::DependencyProperty m_SelectedIconProperty;
        static winrt::Microsoft::UI::Xaml::DependencyProperty m_ShowNotificationDotProperty;
        static winrt::Microsoft::UI::Xaml::DependencyProperty m_UnselectedIconProperty;
        static winrt::Microsoft::UI::Xaml::DependencyProperty m_StaticIconVisibilityProperty;
    };
}

namespace winrt::OpenNet::Helpers::factory_implementation
{
    struct NavItemIconHelper : NavItemIconHelperT<NavItemIconHelper, implementation::NavItemIconHelper> {};
}
