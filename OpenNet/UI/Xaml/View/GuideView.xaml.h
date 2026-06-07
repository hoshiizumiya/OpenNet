#pragma once

#include "mvvm_framework/view.h"

#include "ViewModels/Guide/GuideViewModel.h"
#include "UI/Xaml/View/GuideView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	struct GuideView : GuideViewT<GuideView>, ::mvvm::view<GuideView, winrt::OpenNet::ViewModels::Guide::implementation::GuideViewModel::class_type>
	{
		GuideView();


	};
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
	struct GuideView : GuideViewT<GuideView, implementation::GuideView>
	{
	};
}
