#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/AdvancedOptionItem.g.h"
#include <winrt/Windows.UI.Text.h>

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    struct AdvancedOptionItem : AdvancedOptionItemT<AdvancedOptionItem>
    {
        AdvancedOptionItem() = default;
        AdvancedOptionItem(winrt::hstring optionName, winrt::hstring optionValue, bool isBoldOption);

        winrt::hstring Name() const { return m_name; }
        void Name(winrt::hstring const& value) { m_name = value; }

        winrt::hstring Value() const { return m_value; }
        void Value(winrt::hstring const& value) { m_value = value; }

        winrt::hstring OriginalValue() const { return m_originalValue; }
        void OriginalValue(winrt::hstring const& value) { m_originalValue = value; }

        winrt::Windows::UI::Text::FontWeight FontWeight() const;

        bool IsBold() const { return m_isBold; }
        void IsBold(bool value) { m_isBold = value; }

    private:
        winrt::hstring m_name;
        winrt::hstring m_value;
        winrt::hstring m_originalValue;
        bool m_isBold{ false };
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
    struct AdvancedOptionItem : AdvancedOptionItemT<AdvancedOptionItem, implementation::AdvancedOptionItem>
    {
    };
}
