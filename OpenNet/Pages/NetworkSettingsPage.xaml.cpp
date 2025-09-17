#include "pch.h"
#include "Pages/NetworkSettingsPage.xaml.h"
#if __has_include("Pages/NetworkSettingsPage.g.cpp")
#include "Pages/NetworkSettingsPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::Pages::implementation
{
    NetworkSettingsPage::NetworkSettingsPage()
    {
        InitializeComponent();
    }
}
