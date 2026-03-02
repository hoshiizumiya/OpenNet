#include "pch.h"
#include "Pages/HomePage.xaml.h"
#if __has_include("Pages/HomePage.g.cpp")
#include "Pages/HomePage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::Pages::implementation
{
	HomePage::HomePage()
	{
		InitializeComponent();
	}
}
