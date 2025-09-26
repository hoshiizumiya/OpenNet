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
		// Create and attach the view-model
		m_viewModel = winrt::make<winrt::OpenNet::ViewModels::implementation::TasksViewModel>();
		this->DataContext(m_viewModel); // optional, for {Binding} consumers and designer
	}
}
