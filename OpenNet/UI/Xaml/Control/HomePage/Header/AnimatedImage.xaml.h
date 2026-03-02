#pragma once

#include "UI/Xaml/Control/HomePage/Header/AnimatedImage.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
	struct AnimatedImage : AnimatedImageT<AnimatedImage>
	{
		AnimatedImage();

		winrt::Windows::Foundation::Uri ImageUrl() const;
		void ImageUrl(winrt::Windows::Foundation::Uri const& value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty ImageUrlProperty();

		double BlurAmount() const;
		void BlurAmount(double value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty BlurAmountProperty();

	private:
		void OnLoaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void OnUnloaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void OnSizeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);
		void SetupComposition();
		void SetupEffectPipeline();
		void UpdateImage();

		winrt::Microsoft::UI::Composition::Compositor m_compositor{ nullptr };
		winrt::Microsoft::UI::Composition::ContainerVisual m_container{ nullptr };
		winrt::Microsoft::UI::Composition::SpriteVisual m_currentSprite{ nullptr };
		winrt::Microsoft::UI::Composition::CompositionEffectFactory m_blurFactory{ nullptr };

		winrt::Microsoft::UI::Xaml::Media::LoadedImageSurface m_currentSurface{ nullptr };
		winrt::event_token m_loadedToken{};
		winrt::event_token m_unloadedToken{};
		winrt::event_token m_sizeChangedToken{};
		bool m_compositionReady{ false };
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::factory_implementation
{
	struct AnimatedImage : AnimatedImageT<AnimatedImage, implementation::AnimatedImage>
	{
	};
}
