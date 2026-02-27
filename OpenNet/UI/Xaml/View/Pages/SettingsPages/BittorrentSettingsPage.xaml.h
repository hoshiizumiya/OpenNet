#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.g.h"
#include "AdvancedOptionItem.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/OpenNet.UI.Xaml.Behaviors.h>

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    struct BittorrentSettingsPage : BittorrentSettingsPageT<BittorrentSettingsPage>
    {
        BittorrentSettingsPage();

        // Properties
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> AdvancedOptions() const;

        winrt::hstring SearchFilter() const { return m_searchFilter; }
        void SearchFilter(winrt::hstring const& value);

        // Commands
        void ApplySettings();
        void ResetToDefaults();

        // Event Handlers
        void SearchBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender, 
            winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& args);
        void ApplyButton_Click(winrt::Windows::Foundation::IInspectable const& sender, 
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ResetButton_Click(winrt::Windows::Foundation::IInspectable const& sender, 
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void LoadOptions();
        void FilterOptions();

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_options;
        std::vector<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::AdvancedOptionItem> m_allOptions;
        winrt::hstring m_searchFilter;
        winrt::OpenNet::UI::Xaml::Behaviors::StickyHeaderBehavior m_stickyBehavior{ nullptr };
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
    struct BittorrentSettingsPage : BittorrentSettingsPageT<BittorrentSettingsPage, implementation::BittorrentSettingsPage>
    {
    };
}
