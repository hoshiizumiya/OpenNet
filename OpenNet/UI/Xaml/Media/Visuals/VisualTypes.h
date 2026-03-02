#pragma once

#include "UI/Xaml/Media/Visuals/AttachedVisualFactoryBase.g.h"
#include "UI/Xaml/Media/Visuals/PipelineVisualFactory.g.h"
#include "UI/Xaml/Media/Visuals/UIElementExtensions.g.h"
#include "UI/Xaml/Media/Pipelines/PipelineBuilder.h"
#include "UI/Xaml/Media/Effects/PipelineEffect.h"

namespace winrt::OpenNet::UI::Xaml::Media::Visuals::implementation
{
	// ---- AttachedVisualFactoryBase ----
	struct AttachedVisualFactoryBase : AttachedVisualFactoryBaseT<AttachedVisualFactoryBase>
	{
		AttachedVisualFactoryBase() = default;
	};

	// ---- PipelineVisualFactory ----
	struct PipelineVisualFactory : PipelineVisualFactoryT<PipelineVisualFactory, AttachedVisualFactoryBase>
	{
		PipelineVisualFactory();

		winrt::Windows::Foundation::Collections::IVector<OpenNet::UI::Xaml::Media::Effects::PipelineEffect> Effects() const;
		void Effects(winrt::Windows::Foundation::Collections::IVector<OpenNet::UI::Xaml::Media::Effects::PipelineEffect> const& value);

		// Internal: Build the pipeline and create a SpriteVisual
		winrt::Microsoft::UI::Composition::SpriteVisual CreateVisual(
			winrt::Microsoft::UI::Xaml::UIElement const& element);

	private:
		winrt::Windows::Foundation::Collections::IVector<OpenNet::UI::Xaml::Media::Effects::PipelineEffect> m_effects{ nullptr };
	};

	// ---- UIElementExtensions ----
	struct UIElementExtensions : UIElementExtensionsT<UIElementExtensions>
	{
		UIElementExtensions() = default;

		static OpenNet::UI::Xaml::Media::Visuals::AttachedVisualFactoryBase GetVisualFactory(
			winrt::Microsoft::UI::Xaml::UIElement const& element);
		static void SetVisualFactory(
			winrt::Microsoft::UI::Xaml::UIElement const& element,
			OpenNet::UI::Xaml::Media::Visuals::AttachedVisualFactoryBase const& value);
		static Microsoft::UI::Xaml::DependencyProperty VisualFactoryProperty();

	private:
		static void OnVisualFactoryPropertyChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& d,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

		static Microsoft::UI::Xaml::DependencyProperty s_visualFactoryProperty;
	};
}

namespace winrt::OpenNet::UI::Xaml::Media::Visuals::factory_implementation
{
	struct PipelineVisualFactory : PipelineVisualFactoryT<PipelineVisualFactory, implementation::PipelineVisualFactory> {};
	struct UIElementExtensions : UIElementExtensionsT<UIElementExtensions, implementation::UIElementExtensions> {};
}
