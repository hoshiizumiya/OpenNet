#pragma once

#include "UI/Xaml/View/ItemsCardPopupView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	struct ItemsCardPopupView : ItemsCardPopupViewT<ItemsCardPopupView>
	{
	};
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
	struct ItemsCardPopupView : ItemsCardPopupViewT<ItemsCardPopupView, implementation::ItemsCardPopupView>
	{
	};
}
