#include "pch.h"
#include "AdvancedOptionItem.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/AdvancedOptionItem.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/AdvancedOptionItem.g.cpp"
#endif

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
    AdvancedOptionItem::AdvancedOptionItem(winrt::hstring optionName, winrt::hstring optionValue, bool isBoldOption)
        : m_name(optionName), m_value(optionValue), m_originalValue(optionValue), m_isBold(isBoldOption)
    {
    }

    winrt::Windows::UI::Text::FontWeight AdvancedOptionItem::FontWeight() const
    {
        if (m_isBold)
        {
            return winrt::Windows::UI::Text::FontWeights::Bold();
        }
        return winrt::Windows::UI::Text::FontWeights::Normal();
    }
}
