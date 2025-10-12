#include "pch.h"
#include "AboutPage.xaml.h"
#if __has_include("Pages/SettingsPages/AboutPage.g.cpp")
#include "Pages/SettingsPages/AboutPage.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
	AboutPage::AboutPage()
	{
		InitializeComponent();

	}

}
