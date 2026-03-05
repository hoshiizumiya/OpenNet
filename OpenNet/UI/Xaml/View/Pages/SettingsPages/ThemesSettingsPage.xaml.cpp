#include "pch.h"
#include "ThemesSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/ThemesSettingsPage.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	ThemesSettingsPage::ThemesSettingsPage()
	{
		InitializeComponent();
	}

}
