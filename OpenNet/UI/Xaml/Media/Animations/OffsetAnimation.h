#pragma once

#include "UI/Xaml/Media/Animations/OffsetAnimation.g.h"

namespace winrt::OpenNet::UI::Xaml::Media::Animations::implementation
{
	struct OffsetAnimation : OffsetAnimationT<OffsetAnimation>
	{
		OffsetAnimation();

		Windows::Foundation::TimeSpan OffsetDuration() const;
		void OffsetDuration(Windows::Foundation::TimeSpan const& value);

	private:
		Windows::Foundation::TimeSpan m_offsetDuration{};
	};
}

namespace winrt::OpenNet::UI::Xaml::Media::Animations::factory_implementation
{
	struct OffsetAnimation : winrt::OpenNet::UI::Xaml::Media::Animations::factory_implementation::OffsetAnimationT<OffsetAnimation, implementation::OffsetAnimation>
	{
	};
}
