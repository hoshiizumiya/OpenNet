#include "pch.h"
#include "OffsetAnimation.h"
#if __has_include("UI/Xaml/Media/Animations/OffsetAnimation.g.cpp")
#include "UI/Xaml/Media/Animations/OffsetAnimation.g.cpp"
#endif

using namespace winrt;

namespace winrt::OpenNet::UI::Xaml::Media::Animations::implementation
{
	OffsetAnimation::OffsetAnimation()
	{
	}

	Windows::Foundation::TimeSpan OffsetAnimation::OffsetDuration() const
	{
		return m_offsetDuration;
	}

	void OffsetAnimation::OffsetDuration(Windows::Foundation::TimeSpan const& value)
	{
		m_offsetDuration = value;
	}
}
