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

	void HomePage::OnNavigatedTo(Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		// Expecting parameter to be OpenNet.ViewModels.MainViewModel
		if (auto param = e.Parameter())
		{
			if (auto vm = param.try_as<winrt::OpenNet::ViewModels::MainViewModel>())
			{
				m_viewModel = vm;
				try
				{
					if (Bindings)
					{
						Bindings->Update();
					}
				}
				catch (...)
				{
				}
			}
		}

		HomePageT<HomePage>::OnNavigatedTo(e);
	}
}
