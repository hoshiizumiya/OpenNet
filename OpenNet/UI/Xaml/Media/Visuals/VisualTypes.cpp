#include "pch.h"
#include "UI/Xaml/Media/Visuals/VisualTypes.h"

#if __has_include("UI/Xaml/Media/Visuals/AttachedVisualFactoryBase.g.cpp")
#include "UI/Xaml/Media/Visuals/AttachedVisualFactoryBase.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Visuals/PipelineVisualFactory.g.cpp")
#include "UI/Xaml/Media/Visuals/PipelineVisualFactory.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Visuals/UIElementExtensions.g.cpp")
#include "UI/Xaml/Media/Visuals/UIElementExtensions.g.cpp"
#endif

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Windows.Foundation.Numerics.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Hosting;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Windows::Foundation::Numerics;

namespace winrt::OpenNet::UI::Xaml::Media::Visuals::implementation
{
	// ---- PipelineVisualFactory ----

	PipelineVisualFactory::PipelineVisualFactory()
		: m_effects(winrt::single_threaded_vector<OpenNet::UI::Xaml::Media::Effects::PipelineEffect>())
	{
	}

	winrt::Windows::Foundation::Collections::IVector<OpenNet::UI::Xaml::Media::Effects::PipelineEffect>
		PipelineVisualFactory::Effects() const
	{
		return m_effects;
	}

	void PipelineVisualFactory::Effects(
		winrt::Windows::Foundation::Collections::IVector<OpenNet::UI::Xaml::Media::Effects::PipelineEffect> const& value)
	{
		m_effects = value;
	}

	SpriteVisual PipelineVisualFactory::CreateVisual(UIElement const& element)
	{
		auto visual = ElementCompositionPreview::GetElementVisual(element);
		auto compositor = visual.Compositor();
		auto spriteVisual = compositor.CreateSpriteVisual();

		// Build pipeline from effects
		auto pipeline = ::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder::FromBackdrop();

		if (m_effects)
		{
			for (uint32_t i = 0; i < m_effects.Size(); ++i)
			{
				auto effect = m_effects.GetAt(i);
				auto impl = winrt::get_self<OpenNet::UI::Xaml::Media::Effects::implementation::PipelineEffect>(effect);
				pipeline = impl->AppendToPipeline(pipeline);
			}
		}

		spriteVisual.Brush(pipeline.Build());
		return spriteVisual;
	}

	// ---- UIElementExtensions ----

	DependencyProperty UIElementExtensions::s_visualFactoryProperty =
		DependencyProperty::RegisterAttached(
			L"VisualFactory",
			xaml_typename<OpenNet::UI::Xaml::Media::Visuals::AttachedVisualFactoryBase>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Visuals::UIElementExtensions>(),
			PropertyMetadata{ nullptr, PropertyChangedCallback{ &UIElementExtensions::OnVisualFactoryPropertyChanged } });

	OpenNet::UI::Xaml::Media::Visuals::AttachedVisualFactoryBase UIElementExtensions::GetVisualFactory(
		UIElement const& element)
	{
		return element.GetValue(s_visualFactoryProperty).as<OpenNet::UI::Xaml::Media::Visuals::AttachedVisualFactoryBase>();
	}

	void UIElementExtensions::SetVisualFactory(
		UIElement const& element,
		OpenNet::UI::Xaml::Media::Visuals::AttachedVisualFactoryBase const& value)
	{
		element.SetValue(s_visualFactoryProperty, value);
	}

	DependencyProperty UIElementExtensions::VisualFactoryProperty()
	{
		return s_visualFactoryProperty;
	}

	void UIElementExtensions::OnVisualFactoryPropertyChanged(
		DependencyObject const& d,
		DependencyPropertyChangedEventArgs const& e)
	{
		auto element = d.as<UIElement>();
		if (!element) return;

		auto factory = e.NewValue().try_as<OpenNet::UI::Xaml::Media::Visuals::PipelineVisualFactory>();
		if (!factory) return;

		auto impl = winrt::get_self<PipelineVisualFactory>(factory);
		auto visual = impl->CreateVisual(element);
		visual.RelativeSizeAdjustment(float2{ 1.0f, 1.0f });

		ElementCompositionPreview::SetElementChildVisual(element, visual);
	}
}
