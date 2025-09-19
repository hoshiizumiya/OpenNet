#include "pch.h"
#include "ContactsPage.xaml.h"
#if __has_include("Pages/ContactsPage.g.cpp")
#include "Pages/ContactsPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::Pages::implementation
{
	ContactsPage::ContactsPage()
	{
		InitializeComponent();
	}
}
