#include "pch.h"
#include "UI/Xaml/Media/Pipelines/PipelineBrush.h"
#include "UI/Xaml/Media/Pipelines/PipelineBrush.g.cpp"

namespace winrt::OpenNet::UI::Xaml::Media::Pipelines::implementation
{
	namespace MUX = Microsoft::UI::Xaml;

	// Static DependencyProperty registration
	MUX::DependencyProperty PipelineBrush::s_sourceProperty =
		MUX::DependencyProperty::Register(
			L"Source",
			winrt::xaml_typename<OpenNet::UI::Xaml::Media::Pipelines::IPipelineEffect>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Media::Pipelines::PipelineBrush>(),
			MUX::PropertyMetadata{ nullptr });

	OpenNet::UI::Xaml::Media::Pipelines::IPipelineEffect PipelineBrush::Source() const
	{
		return GetValue(s_sourceProperty).try_as<OpenNet::UI::Xaml::Media::Pipelines::IPipelineEffect>();
	}

	void PipelineBrush::Source(OpenNet::UI::Xaml::Media::Pipelines::IPipelineEffect const& value)
	{
		SetValue(s_sourceProperty, value);
	}

	MUX::DependencyProperty PipelineBrush::SourceProperty()
	{
		return s_sourceProperty;
	}

	void PipelineBrush::SetPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder const& builder)
	{
		m_builder = builder;
		if (CompositionBrush())
		{
			RebuildBrush();
		}
	}

	void PipelineBrush::OnConnected()
	{
		RebuildBrush();
	}

	void PipelineBrush::OnDisconnected()
	{
		auto brush = CompositionBrush();
		if (brush)
		{
			CompositionBrush(nullptr);
		}
	}

	void PipelineBrush::RebuildBrush()
	{
		if (m_builder.has_value())
		{
			try
			{
				CompositionBrush(m_builder->Build());
			}
			catch (...)
			{
				// Silently fail on effect build errors
			}
		}
	}
}
