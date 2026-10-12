#include "XamlWorkaround.h"
#include "MaterialSettingsResources.xaml.h"
#if __has_include("UI/Xaml/Control/Theme/MaterialSettingsResources.g.cpp")
#include "UI/Xaml/Control/Theme/MaterialSettingsResources.g.cpp"
#endif

namespace winrt::OpenNet::UI::Xaml::Control::Theme::implementation
{
    MaterialSettingsResources::MaterialSettingsResources()
    {
        InitializeComponent();
    }
}
