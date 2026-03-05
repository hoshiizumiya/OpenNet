#include "pch.h"
#include "ContactsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/ContactsPage.g.cpp")
#include "UI/Xaml/View/Pages/ContactsPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	ContactsPage::ContactsPage()
	{
		InitializeComponent();
	}
}
