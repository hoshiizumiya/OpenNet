#pragma once

#include "UI/Xaml/Behavior/Action/ShowTeachingTipAction.g.h"

namespace winrt::OpenNet::UI::Xaml::Behavior::Action::implementation
{
	struct ShowTeachingTipAction : ShowTeachingTipActionT<ShowTeachingTipAction>
    {
        ShowTeachingTipAction();

		winrt::Windows::Foundation::IInspectable Execute(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& parameter);
		static winrt::Microsoft::UI::Xaml::DependencyProperty TeachingTipProperty();

        winrt::Microsoft::UI::Xaml::Controls::TeachingTip TeachingTip();
        void TeachingTip(winrt::Microsoft::UI::Xaml::Controls::TeachingTip const& value);
    };
}

namespace winrt::OpenNet::UI::Xaml::Behavior::Action::factory_implementation
{
    struct ShowTeachingTipAction : ShowTeachingTipActionT<ShowTeachingTipAction, implementation::ShowTeachingTipAction>
    {
    };
}
