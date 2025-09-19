#include "pch.h"
#include "ServersPage.xaml.h"
#if __has_include("Pages/ServersPage.g.cpp")
#include "Pages/ServersPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;


namespace winrt::OpenNet::Pages::implementation
{
	ServersPage::ServersPage()
	{
		InitializeComponent();
	}
}
