#include "pch.h"
#include "ServersPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/ServersPage.g.cpp")
#include "UI/Xaml/View/Pages/ServersPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;


namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	ServersPage::ServersPage()
	{
		InitializeComponent();
	}
}
