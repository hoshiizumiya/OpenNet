#pragma once

#include "UI/Xaml/Media/Animations/Implicit.g.h"

namespace winrt::OpenNet::UI::Xaml::Media::Animations::implementation
{
	struct Implicit : ImplicitT<Implicit>
	{
		Implicit();

		static winrt::Microsoft::UI::Xaml::DependencyProperty AnimationsProperty();
		static winrt::OpenNet::UI::Xaml::Media::Animations::ImplicitAnimationSet GetAnimations(winrt::Microsoft::UI::Xaml::DependencyObject const& obj);
		static void SetAnimations(winrt::Microsoft::UI::Xaml::DependencyObject const& obj, winrt::OpenNet::UI::Xaml::Media::Animations::ImplicitAnimationSet const& value);
	};
}

namespace winrt::OpenNet::UI::Xaml::Media::Animations::factory_implementation
{
	struct Implicit : winrt::OpenNet::UI::Xaml::Media::Animations::factory_implementation::ImplicitT<Implicit, implementation::Implicit>
	{
	};
}
