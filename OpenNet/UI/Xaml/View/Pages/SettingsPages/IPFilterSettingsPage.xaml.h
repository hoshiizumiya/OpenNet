#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    struct IPFilterSettingsPage : IPFilterSettingsPageT<IPFilterSettingsPage>
    {
        IPFilterSettingsPage();

        void OnEnableToggled(winrt::Windows::Foundation::IInspectable const& sender,
                             winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OnAddRuleClick(winrt::Windows::Foundation::IInspectable const& sender,
                            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnImportClick(winrt::Windows::Foundation::IInspectable const& sender,
                                             winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        winrt::fire_and_forget OnClearAllClick(winrt::Windows::Foundation::IInspectable const& sender,
                                               winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        winrt::fire_and_forget LoadState();
        void RefreshRuleCount();
        void ShowStatus(winrt::hstring const& message, winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity);

        bool m_loading{ false };
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
    struct IPFilterSettingsPage : IPFilterSettingsPageT<IPFilterSettingsPage, implementation::IPFilterSettingsPage>
    {};
}
