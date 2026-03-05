#include "pch.h"
#include "UI/Xaml/View/Pages/HomePage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/HomePage.g.cpp")
#include "UI/Xaml/View/Pages/HomePage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	HomePage::HomePage()
	{
		InitializeComponent();
	}
}
