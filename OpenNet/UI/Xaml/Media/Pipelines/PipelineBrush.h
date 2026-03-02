#pragma once

#include "UI/Xaml/Media/Pipelines/PipelineBrush.g.h"
#include "UI/Xaml/Media/Pipelines/PipelineBuilder.h"

namespace winrt::OpenNet::UI::Xaml::Media::Pipelines::implementation
{
	struct PipelineBrush : PipelineBrushT<PipelineBrush>
	{
		PipelineBrush() = default;

		// --- Source DependencyProperty (for XAML usage) ---
		OpenNet::UI::Xaml::Media::Pipelines::IPipelineEffect Source() const;
		void Source(OpenNet::UI::Xaml::Media::Pipelines::IPipelineEffect const& value);
		static Microsoft::UI::Xaml::DependencyProperty SourceProperty();

		// --- Code-only: set an already-built PipelineBuilder ---
		void SetPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder const& builder);

		// --- XamlCompositionBrushBase overrides ---
		void OnConnected();
		void OnDisconnected();

	private:
		void RebuildBrush();

		static Microsoft::UI::Xaml::DependencyProperty s_sourceProperty;
		std::optional<::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder> m_builder;
	};
}

namespace winrt::OpenNet::UI::Xaml::Media::Pipelines::factory_implementation
{
	struct PipelineBrush : PipelineBrushT<PipelineBrush, implementation::PipelineBrush>
	{
	};
}
