#pragma once

#include "UI/Xaml/Media/Effects/PipelineEffect.g.h"
#include "UI/Xaml/Media/Pipelines/PipelineBuilder.h"

namespace winrt::OpenNet::UI::Xaml::Media::Effects::implementation
{
	struct PipelineEffect : PipelineEffectT<PipelineEffect>
	{
		PipelineEffect() = default;

		bool IsAnimatable() const;
		void IsAnimatable(bool value);

		// Internal C++ method: subclasses override to append themselves to a pipeline
		virtual ::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const;

	protected:
		bool m_isAnimatable{ false };
	};
}

// No factory_implementation for unsealed base class PipelineEffect
