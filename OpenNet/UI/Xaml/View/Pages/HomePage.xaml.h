#pragma once

#include "UI/Xaml/View/Pages/HomePage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct HomePage : HomePageT<HomePage>
	{
		HomePage();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct HomePage : HomePageT<HomePage, implementation::HomePage>
	{
	};
}
