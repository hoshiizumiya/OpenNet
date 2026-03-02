#include "pch.h"
#include "UI/Xaml/Media/Effects/PipelineEffect.h"
#include "UI/Xaml/Media/Effects/PipelineEffect.g.cpp"

namespace winrt::OpenNet::UI::Xaml::Media::Effects::implementation
{
	bool PipelineEffect::IsAnimatable() const
	{
		return m_isAnimatable;
	}

	void PipelineEffect::IsAnimatable(bool value)
	{
		m_isAnimatable = value;
	}

	::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
		PipelineEffect::AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const
	{
		// Base implementation: identity (return the builder unchanged)
		return builder;
	}
}
