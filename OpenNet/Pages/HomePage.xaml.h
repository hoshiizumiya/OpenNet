#pragma once

#include "Pages/HomePage.g.h"

namespace winrt::OpenNet::Pages::implementation
{
	struct HomePage : HomePageT<HomePage>
	{
		HomePage();
	};
}

namespace winrt::OpenNet::Pages::factory_implementation
{
	struct HomePage : HomePageT<HomePage, implementation::HomePage>
	{
	};
}
