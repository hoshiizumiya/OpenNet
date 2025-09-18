#pragma once

#include "Pages/HomePage.g.h"

namespace winrt::OpenNet::Pages::implementation
{
	struct HomePage : HomePageT<HomePage>
	{
		HomePage();

		// ViewModel property (for x:Bind)
		winrt::OpenNet::ViewModels::MainViewModel ViewModel() const noexcept { return m_viewModel; }

		// Receive navigation parameter
		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

	private:
		winrt::OpenNet::ViewModels::MainViewModel m_viewModel{ nullptr };
	};
}

namespace winrt::OpenNet::Pages::factory_implementation
{
	struct HomePage : HomePageT<HomePage, implementation::HomePage>
	{
	};
}
