#pragma once

#include "UI/Xaml/Control/Card/HorizontalCard.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::Card::implementation
{
    struct HorizontalCard : HorizontalCardT<HorizontalCard>
    {
        HorizontalCard();

        // DependencyProperty accessors
		static winrt::Microsoft::UI::Xaml::DependencyProperty LeftProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty RightProperty();

		// Properties
		winrt::Microsoft::UI::Xaml::UIElement Left();
		void Left(winrt::Microsoft::UI::Xaml::UIElement const& value);
		winrt::Microsoft::UI::Xaml::UIElement Right();
		void Right(winrt::Microsoft::UI::Xaml::UIElement const& value);
    };
}

namespace winrt::OpenNet::UI::Xaml::Control::Card::factory_implementation
{
    struct HorizontalCard : HorizontalCardT<HorizontalCard, implementation::HorizontalCard>
    {
    };
}
