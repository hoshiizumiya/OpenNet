#include "pch.h"
#include "FontCustomizePage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/FontCustomizePage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/FontCustomizePage.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	FontCustomizePage::FontCustomizePage()
	{
		InitializeComponent();
	}
}
