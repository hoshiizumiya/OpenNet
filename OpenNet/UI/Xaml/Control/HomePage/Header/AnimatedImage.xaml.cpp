#include "pch.h"
#include "AnimatedImage.xaml.h"
#if __has_include("UI/Xaml/Control/HomePage/Header/AnimatedImage.g.cpp")
#include "UI/Xaml/Control/HomePage/Header/AnimatedImage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.Graphics.Canvas.Effects.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Hosting;
using namespace winrt::Microsoft::UI::Composition;

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
	static DependencyProperty s_imageUrlProperty{ nullptr };
	static DependencyProperty s_blurAmountProperty{ nullptr };

	AnimatedImage::AnimatedImage()
	{
		InitializeComponent();

		m_loadedToken = Loaded({ this, &AnimatedImage::OnLoaded });
		m_unloadedToken = Unloaded({ this, &AnimatedImage::OnUnloaded });
		m_sizeChangedToken = SizeChanged({ this, &AnimatedImage::OnSizeChanged });
	}

	void AnimatedImage::OnLoaded(IInspectable const&, RoutedEventArgs const&)
	{
		SetupComposition();
	}

	void AnimatedImage::OnUnloaded(IInspectable const&, RoutedEventArgs const&)
	{
		m_compositionReady = false;

		// Remove all children to release sprites and break batch.Completed references
		if (m_container)
		{
			try { m_container.Children().RemoveAll(); } catch (...) {}
		}

		m_currentSprite = nullptr;
		m_container = nullptr;
		m_blurFactory = nullptr;
		m_currentSurface = nullptr;
		m_compositor = nullptr;
	}

	void AnimatedImage::OnSizeChanged(IInspectable const&, SizeChangedEventArgs const&)
	{
		if (m_container)
		{
			m_container.Size({ static_cast<float>(ActualWidth()), static_cast<float>(ActualHeight()) });
		}
	}

	void AnimatedImage::SetupComposition()
	{
		auto visual = ElementCompositionPreview::GetElementVisual(RootGrid());
		m_compositor = visual.Compositor();

		// Reset state in case this is a re-load (Loaded fired again)
		if (m_container)
		{
			try { m_container.Children().RemoveAll(); } catch (...) {}
		}
		m_currentSprite = nullptr;
		m_currentSurface = nullptr;

		m_container = m_compositor.CreateContainerVisual();
		m_container.RelativeSizeAdjustment({ 1.0f, 1.0f });
		m_container.Clip(m_compositor.CreateInsetClip());

		ElementCompositionPreview::SetElementChildVisual(RootGrid(), m_container);

		// FIX: Set compositionReady BEFORE SetupEffectPipeline so blur effect is created
		m_compositionReady = true;
		SetupEffectPipeline();
		UpdateImage();
	}

	void AnimatedImage::SetupEffectPipeline()
	{
		// Guard: compositor is only valid after OnLoaded -> SetupComposition()
		if (!m_compositionReady || !m_compositor) return;

		auto blurAmount = static_cast<float>(BlurAmount());
		if (blurAmount > 0.0f)
		{
			Microsoft::Graphics::Canvas::Effects::GaussianBlurEffect blurEffect;
			blurEffect.BlurAmount(blurAmount);
			blurEffect.BorderMode(Microsoft::Graphics::Canvas::Effects::EffectBorderMode::Hard);
			blurEffect.Source(CompositionEffectSourceParameter(L"source"));

			m_blurFactory = m_compositor.CreateEffectFactory(
				blurEffect.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>());
		}
		else
		{
			m_blurFactory = nullptr;
		}
	}

	void AnimatedImage::UpdateImage()
	{
		if (!m_compositionReady || !m_compositor || !m_container || !ImageUrl()) return;

		auto surface = Microsoft::UI::Xaml::Media::LoadedImageSurface::StartLoadFromUri(ImageUrl());
		m_currentSurface = surface;

		auto surfaceBrush = m_compositor.CreateSurfaceBrush(surface);
		surfaceBrush.Stretch(CompositionStretch::UniformToFill);
		surfaceBrush.HorizontalAlignmentRatio(0.5f);
		surfaceBrush.VerticalAlignmentRatio(0.5f);

		CompositionBrush finalBrush{ nullptr };
		if (m_blurFactory)
		{
			auto effectBrush = m_blurFactory.CreateBrush();
			effectBrush.SetSourceParameter(L"source", surfaceBrush);
			finalBrush = effectBrush;
		}
		else
		{
			finalBrush = surfaceBrush;
		}

		auto newSprite = m_compositor.CreateSpriteVisual();
		newSprite.RelativeSizeAdjustment({ 1.0f, 1.0f });
		newSprite.Brush(finalBrush);

		m_container.Children().InsertAtTop(newSprite);

		// Cross-fade: animate the old sprite out
		if (m_currentSprite)
		{
			auto oldSprite = m_currentSprite;

			try
			{
				auto fadeOut = m_compositor.CreateScalarKeyFrameAnimation();
				fadeOut.InsertKeyFrame(1.0f, 0.0f);
				fadeOut.Duration(std::chrono::milliseconds(800));

				auto batch = m_compositor.CreateScopedBatch(CompositionBatchTypes::Animation);
				oldSprite.StartAnimation(L"Opacity", fadeOut);
				batch.End();

				// Capture container by copy to safely remove oldSprite after fade-out
				auto container = m_container;
				batch.Completed([container, oldSprite](auto&&, auto&&)
				{
					if (container)
					{
						try { container.Children().Remove(oldSprite); } catch (...) {}
					}
				});
			}
			catch (...)
			{
				// If animation fails (disposed objects), just remove the old sprite directly
				try { m_container.Children().Remove(oldSprite); } catch (...) {}
			}
		}

		m_currentSprite = newSprite;
	}

	// ---- DependencyProperty registrations ----

	DependencyProperty AnimatedImage::ImageUrlProperty()
	{
		if (!s_imageUrlProperty)
		{
			s_imageUrlProperty = DependencyProperty::Register(
				L"ImageUrl",
				winrt::xaml_typename<winrt::Windows::Foundation::Uri>(),
				winrt::xaml_typename<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::AnimatedImage>(),
				PropertyMetadata{ nullptr,
					PropertyChangedCallback{ [](DependencyObject const& d, DependencyPropertyChangedEventArgs const&)
					{
						if (auto img = d.try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::AnimatedImage>())
						{
							get_self<AnimatedImage>(img)->UpdateImage();
						}
					}}
				});
		}
		return s_imageUrlProperty;
	}

	winrt::Windows::Foundation::Uri AnimatedImage::ImageUrl() const
	{
		return GetValue(ImageUrlProperty()).try_as<winrt::Windows::Foundation::Uri>();
	}
	void AnimatedImage::ImageUrl(winrt::Windows::Foundation::Uri const& value)
	{
		SetValue(ImageUrlProperty(), value);
	}

	DependencyProperty AnimatedImage::BlurAmountProperty()
	{
		if (!s_blurAmountProperty)
		{
			s_blurAmountProperty = DependencyProperty::Register(
				L"BlurAmount",
				winrt::xaml_typename<double>(),
				winrt::xaml_typename<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::AnimatedImage>(),
				PropertyMetadata{ box_value(0.0),
					PropertyChangedCallback{ [](DependencyObject const& d, DependencyPropertyChangedEventArgs const&)
					{
						if (auto img = d.try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::AnimatedImage>())
						{
							auto self = get_self<AnimatedImage>(img);
							self->SetupEffectPipeline();
							self->UpdateImage();
						}
					}}
				});
		}
		return s_blurAmountProperty;
	}

	double AnimatedImage::BlurAmount() const
	{
		return unbox_value_or<double>(GetValue(BlurAmountProperty()), 0.0);
	}
	void AnimatedImage::BlurAmount(double value)
	{
		SetValue(BlurAmountProperty(), box_value(value));
	}
}
