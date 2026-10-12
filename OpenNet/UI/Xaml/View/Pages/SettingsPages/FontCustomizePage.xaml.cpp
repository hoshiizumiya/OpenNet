#include "XamlWorkaround.h"
#include "FontCustomizePage.xaml.h"
#include "UI/Xaml/Control/Theme/Typography.h"
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
	void FontCustomizePage::Reset_Click(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		winrt::OpenNet::UI::Xaml::Control::Theme::implementation::Typography::Current().Scale(1.0);
	}
}
