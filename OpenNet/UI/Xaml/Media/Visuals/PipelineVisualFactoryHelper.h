#pragma once
// PipelineVisualFactoryHelper - C++/WinRT port of CommunityToolkit.WinUI.Media.Visuals.PipelineVisualFactory
// Creates SpriteVisuals from PipelineBuilder pipelines and attaches them to UIElements.
// NOTE: This is a pure C++ helper class. The WinRT runtimeclass is defined in Visuals.idl / VisualTypes.h.

#include "UI/Xaml/Media/Pipelines/PipelineBuilder.h"

namespace OpenNet::UI::Xaml::Media::Visuals
{
	namespace MUC = winrt::Microsoft::UI::Composition;
	namespace MUX = winrt::Microsoft::UI::Xaml;

	/// <summary>
	/// Helper factory for creating composition visuals from a PipelineBuilder pipeline.
	/// </summary>
	class PipelineVisualFactoryHelper
	{
	public:
		PipelineVisualFactoryHelper() = default;

		/// <summary>
		/// Creates a PipelineVisualFactoryHelper from a PipelineBuilder
		/// </summary>
		explicit PipelineVisualFactoryHelper(Pipelines::PipelineBuilder builder)
			: m_builder(std::move(builder))
		{
		}

		/// <summary>
		/// Sets the pipeline builder.
		/// </summary>
		void SetPipeline(Pipelines::PipelineBuilder builder)
		{
			m_builder = std::move(builder);
		}

		/// <summary>
		/// Gets the pipeline builder.
		/// </summary>
		Pipelines::PipelineBuilder const& Pipeline() const { return m_builder; }

		/// <summary>
		/// Builds and attaches the effect visual as a child visual of the target element.
		/// The visual is auto-sized to the reference element (or target if no reference).
		/// </summary>
		MUC::SpriteVisual Attach(
			MUX::UIElement const& target,
			MUX::UIElement const* reference = nullptr) const
		{
			return m_builder.Attach(target, reference);
		}

		/// <summary>
		/// Builds the CompositionBrush without attaching to any element.
		/// </summary>
		MUC::CompositionBrush Build() const
		{
			return m_builder.Build();
		}

	private:
		Pipelines::PipelineBuilder m_builder;
	};

} // namespace OpenNet::UI::Xaml::Media::Visuals
