#include "pch.h"
#include "TasksPage.xaml.h"
#if __has_include("Pages/TasksPage.g.cpp")
#include "Pages/TasksPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::Pages::implementation
{
	TasksPage::TasksPage()
	{
		InitializeComponent();
	}
}
