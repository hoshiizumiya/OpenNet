#pragma once
#include "UI/Xaml/Control/Theme/MaterialSettingsResources.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
    struct MaterialSettingsResources : MaterialSettingsResourcesT<MaterialSettingsResources>
    {
        MaterialSettingsResources();
    };
}
namespace winrt::OpenNet::UI::Xaml::Control::Theme::factory_implementation
{
    struct MaterialSettingsResources : MaterialSettingsResourcesT<MaterialSettingsResources, implementation::MaterialSettingsResources> {};
}
