#include "XamlWorkaround.h"
#include "MaterialControlState.h"
#if __has_include("UI/Xaml/Control/Theme/MaterialControlState.g.cpp")
#include "UI/Xaml/Control/Theme/MaterialControlState.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
    DependencyProperty MaterialControlState::StyleIndexProperty()
    {
        static DependencyProperty property = DependencyProperty::Register(
            L"StyleIndex",
            xaml_typename<std::int32_t>(),
            xaml_typename<OpenNet::UI::Xaml::Control::Theme::MaterialControlState>(),
            PropertyMetadata{ box_value(0) });
        return property;
    }

    std::int32_t MaterialControlState::StyleIndex()
    {
        return unbox_value<std::int32_t>(GetValue(StyleIndexProperty()));
    }

    void MaterialControlState::StyleIndex(std::int32_t value)
    {
        SetValue(StyleIndexProperty(), box_value(value));
    }
}
