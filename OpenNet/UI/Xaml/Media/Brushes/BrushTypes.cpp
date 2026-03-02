#include "pch.h"
#include "UI/Xaml/Media/Brushes/BrushTypes.h"

#if __has_include("UI/Xaml/Media/Brushes/BackdropBlurBrush.g.cpp")
#include "UI/Xaml/Media/Brushes/BackdropBlurBrush.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Brushes/BackdropSaturationBrush.g.cpp")
#include "UI/Xaml/Media/Brushes/BackdropSaturationBrush.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Brushes/PipelineAcrylicBrush.g.cpp")
#include "UI/Xaml/Media/Brushes/PipelineAcrylicBrush.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Brushes/BackdropInvertBrush.g.cpp")
#include "UI/Xaml/Media/Brushes/BackdropInvertBrush.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Brushes/BackdropSepiaBrush.g.cpp")
#include "UI/Xaml/Media/Brushes/BackdropSepiaBrush.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Brushes/BackdropGammaTransferBrush.g.cpp")
#include "UI/Xaml/Media/Brushes/BackdropGammaTransferBrush.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Brushes/ImageBlendBrush.g.cpp")
#include "UI/Xaml/Media/Brushes/ImageBlendBrush.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Brushes/AcrylicBrush.g.cpp")
#include "UI/Xaml/Media/Brushes/AcrylicBrush.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Brushes/TilesBrush.g.cpp")
#include "UI/Xaml/Media/Brushes/TilesBrush.g.cpp"
#endif

#include <winrt/Windows.UI.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Composition;
using ::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder;

namespace winrt::OpenNet::UI::Xaml::Media::Brushes::implementation
{
	// =====================================================================
	// BackdropBlurBrush
	// =====================================================================

	DependencyProperty BackdropBlurBrush::s_amountProperty =
		DependencyProperty::Register(
			L"Amount",
			xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::BackdropBlurBrush>(),
			PropertyMetadata{ box_value(8.0) });

	double BackdropBlurBrush::Amount() const
	{
		return unbox_value<double>(GetValue(s_amountProperty));
	}

	void BackdropBlurBrush::Amount(double value)
	{
		SetValue(s_amountProperty, box_value(value));
	}

	DependencyProperty BackdropBlurBrush::AmountProperty()
	{
		return s_amountProperty;
	}

	void BackdropBlurBrush::OnConnected()
	{
		BuildBrush();
	}

	void BackdropBlurBrush::OnDisconnected()
	{
		CompositionBrush(nullptr);
	}

	void BackdropBlurBrush::BuildBrush()
	{
		try
		{
			auto pipeline = PipelineBuilder::FromBackdrop()
				.Blur(static_cast<float>(Amount()));
			CompositionBrush(pipeline.Build());
		}
		catch (...) {}
	}

	// =====================================================================
	// BackdropSaturationBrush
	// =====================================================================

	DependencyProperty BackdropSaturationBrush::s_saturationProperty =
		DependencyProperty::Register(
			L"Saturation",
			xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::BackdropSaturationBrush>(),
			PropertyMetadata{ box_value(0.5) });

	double BackdropSaturationBrush::Saturation() const
	{
		return unbox_value<double>(GetValue(s_saturationProperty));
	}

	void BackdropSaturationBrush::Saturation(double value)
	{
		SetValue(s_saturationProperty, box_value(value));
	}

	DependencyProperty BackdropSaturationBrush::SaturationProperty()
	{
		return s_saturationProperty;
	}

	void BackdropSaturationBrush::OnConnected()
	{
		BuildBrush();
	}

	void BackdropSaturationBrush::OnDisconnected()
	{
		CompositionBrush(nullptr);
	}

	void BackdropSaturationBrush::BuildBrush()
	{
		try
		{
			auto pipeline = PipelineBuilder::FromBackdrop()
				.Saturation(static_cast<float>(Saturation()));
			CompositionBrush(pipeline.Build());
		}
		catch (...) {}
	}

	// =====================================================================
	// PipelineAcrylicBrush
	// =====================================================================

	DependencyProperty PipelineAcrylicBrush::s_blurAmountProperty =
		DependencyProperty::Register(
			L"BlurAmount",
			xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::PipelineAcrylicBrush>(),
			PropertyMetadata{ box_value(30.0) });

	DependencyProperty PipelineAcrylicBrush::s_tintColorProperty =
		DependencyProperty::Register(
			L"TintColor",
			xaml_typename<winrt::Windows::UI::Color>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::PipelineAcrylicBrush>(),
			PropertyMetadata{ box_value(winrt::Windows::UI::Color{ 255, 255, 255, 255 }) });

	DependencyProperty PipelineAcrylicBrush::s_tintOpacityProperty =
		DependencyProperty::Register(
			L"TintOpacity",
			xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::PipelineAcrylicBrush>(),
			PropertyMetadata{ box_value(0.3) });

	double PipelineAcrylicBrush::BlurAmount() const
	{
		return unbox_value<double>(GetValue(s_blurAmountProperty));
	}

	void PipelineAcrylicBrush::BlurAmount(double value)
	{
		SetValue(s_blurAmountProperty, box_value(value));
	}

	DependencyProperty PipelineAcrylicBrush::BlurAmountProperty()
	{
		return s_blurAmountProperty;
	}

	winrt::Windows::UI::Color PipelineAcrylicBrush::TintColor() const
	{
		return unbox_value<winrt::Windows::UI::Color>(GetValue(s_tintColorProperty));
	}

	void PipelineAcrylicBrush::TintColor(winrt::Windows::UI::Color value)
	{
		SetValue(s_tintColorProperty, box_value(value));
	}

	DependencyProperty PipelineAcrylicBrush::TintColorProperty()
	{
		return s_tintColorProperty;
	}

	double PipelineAcrylicBrush::TintOpacity() const
	{
		return unbox_value<double>(GetValue(s_tintOpacityProperty));
	}

	void PipelineAcrylicBrush::TintOpacity(double value)
	{
		SetValue(s_tintOpacityProperty, box_value(value));
	}

	DependencyProperty PipelineAcrylicBrush::TintOpacityProperty()
	{
		return s_tintOpacityProperty;
	}

	void PipelineAcrylicBrush::OnConnected()
	{
		BuildBrush();
	}

	void PipelineAcrylicBrush::OnDisconnected()
	{
		CompositionBrush(nullptr);
	}

	void PipelineAcrylicBrush::BuildBrush()
	{
		try
		{
			auto pipeline = PipelineBuilder::FromBackdropAcrylic(
				TintColor(),
				static_cast<float>(TintOpacity()),
				static_cast<float>(BlurAmount()));
			CompositionBrush(pipeline.Build());
		}
		catch (...) {}
	}

	// =====================================================================
	// BackdropInvertBrush
	// =====================================================================

	void BackdropInvertBrush::OnConnected()
	{
		try
		{
			CompositionBrush(PipelineBuilder::FromBackdrop().Invert().Build());
		}
		catch (...) {}
	}

	void BackdropInvertBrush::OnDisconnected()
	{
		CompositionBrush(nullptr);
	}

	// =====================================================================
	// BackdropSepiaBrush
	// =====================================================================

	DependencyProperty BackdropSepiaBrush::s_intensityProperty =
		DependencyProperty::Register(
			L"Intensity",
			xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::BackdropSepiaBrush>(),
			PropertyMetadata{ box_value(0.5) });

	double BackdropSepiaBrush::Intensity() const
	{
		return unbox_value<double>(GetValue(s_intensityProperty));
	}

	void BackdropSepiaBrush::Intensity(double value)
	{
		SetValue(s_intensityProperty, box_value(value));
	}

	DependencyProperty BackdropSepiaBrush::IntensityProperty()
	{
		return s_intensityProperty;
	}

	void BackdropSepiaBrush::OnConnected()
	{
		BuildBrush();
	}

	void BackdropSepiaBrush::OnDisconnected()
	{
		CompositionBrush(nullptr);
	}

	void BackdropSepiaBrush::BuildBrush()
	{
		try
		{
			CompositionBrush(PipelineBuilder::FromBackdrop().Sepia(static_cast<float>(Intensity())).Build());
		}
		catch (...) {}
	}

	// =====================================================================
	// BackdropGammaTransferBrush
	// =====================================================================

	DependencyProperty BackdropGammaTransferBrush::s_redExponentProperty =
		DependencyProperty::Register(L"RedExponent", xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::BackdropGammaTransferBrush>(),
			PropertyMetadata{ box_value(1.0) });

	DependencyProperty BackdropGammaTransferBrush::s_greenExponentProperty =
		DependencyProperty::Register(L"GreenExponent", xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::BackdropGammaTransferBrush>(),
			PropertyMetadata{ box_value(1.0) });

	DependencyProperty BackdropGammaTransferBrush::s_blueExponentProperty =
		DependencyProperty::Register(L"BlueExponent", xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::BackdropGammaTransferBrush>(),
			PropertyMetadata{ box_value(1.0) });

	double BackdropGammaTransferBrush::RedExponent() const { return unbox_value<double>(GetValue(s_redExponentProperty)); }
	void BackdropGammaTransferBrush::RedExponent(double v) { SetValue(s_redExponentProperty, box_value(v)); }
	DependencyProperty BackdropGammaTransferBrush::RedExponentProperty() { return s_redExponentProperty; }

	double BackdropGammaTransferBrush::GreenExponent() const { return unbox_value<double>(GetValue(s_greenExponentProperty)); }
	void BackdropGammaTransferBrush::GreenExponent(double v) { SetValue(s_greenExponentProperty, box_value(v)); }
	DependencyProperty BackdropGammaTransferBrush::GreenExponentProperty() { return s_greenExponentProperty; }

	double BackdropGammaTransferBrush::BlueExponent() const { return unbox_value<double>(GetValue(s_blueExponentProperty)); }
	void BackdropGammaTransferBrush::BlueExponent(double v) { SetValue(s_blueExponentProperty, box_value(v)); }
	DependencyProperty BackdropGammaTransferBrush::BlueExponentProperty() { return s_blueExponentProperty; }

	void BackdropGammaTransferBrush::OnConnected()
	{
		BuildBrush();
	}

	void BackdropGammaTransferBrush::OnDisconnected()
	{
		CompositionBrush(nullptr);
	}

	void BackdropGammaTransferBrush::BuildBrush()
	{
		try
		{
			// Use Exposure effect as a perceptual gamma approximation via PipelineBuilder
			// (Win2D GammaTransferEffect is available directly if needed)
			auto compositor = CompositionTarget::GetCompositorForCurrentThread();

			winrt::Microsoft::Graphics::Canvas::Effects::GammaTransferEffect gammaEffect;
			gammaEffect.RedExponent(static_cast<float>(RedExponent()));
			gammaEffect.GreenExponent(static_cast<float>(GreenExponent()));
			gammaEffect.BlueExponent(static_cast<float>(BlueExponent()));
			gammaEffect.RedAmplitude(1.0f);
			gammaEffect.GreenAmplitude(1.0f);
			gammaEffect.BlueAmplitude(1.0f);
			gammaEffect.RedOffset(0.0f);
			gammaEffect.GreenOffset(0.0f);
			gammaEffect.BlueOffset(0.0f);
			gammaEffect.Source(winrt::Microsoft::UI::Composition::CompositionEffectSourceParameter(L"backdrop"));

			auto factory = compositor.CreateEffectFactory(
				gammaEffect.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>());
			auto effectBrush = factory.CreateBrush();
			effectBrush.SetSourceParameter(L"backdrop", compositor.CreateBackdropBrush());
			CompositionBrush(effectBrush);
		}
		catch (...) {}
	}

	// =====================================================================
	// ImageBlendBrush
	// =====================================================================

	DependencyProperty ImageBlendBrush::s_sourceProperty =
		DependencyProperty::Register(
			L"Source",
			xaml_typename<winrt::Microsoft::UI::Xaml::Media::ImageSource>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::ImageBlendBrush>(),
			PropertyMetadata{ nullptr });

	DependencyProperty ImageBlendBrush::s_stretchProperty =
		DependencyProperty::Register(
			L"Stretch",
			xaml_typename<winrt::Microsoft::UI::Xaml::Media::Stretch>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::ImageBlendBrush>(),
			PropertyMetadata{ box_value(winrt::Microsoft::UI::Xaml::Media::Stretch::None) });

	DependencyProperty ImageBlendBrush::s_modeProperty =
		DependencyProperty::Register(
			L"Mode",
			xaml_typename<OpenNet::UI::Xaml::Media::ImageBlendMode>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::ImageBlendBrush>(),
			PropertyMetadata{ box_value(OpenNet::UI::Xaml::Media::ImageBlendMode::Multiply) });

	winrt::Microsoft::UI::Xaml::Media::ImageSource ImageBlendBrush::Source() const
	{
		return GetValue(s_sourceProperty).as<winrt::Microsoft::UI::Xaml::Media::ImageSource>();
	}

	void ImageBlendBrush::Source(winrt::Microsoft::UI::Xaml::Media::ImageSource const& value)
	{
		SetValue(s_sourceProperty, value);
	}

	DependencyProperty ImageBlendBrush::SourceProperty() { return s_sourceProperty; }

	winrt::Microsoft::UI::Xaml::Media::Stretch ImageBlendBrush::Stretch() const
	{
		return unbox_value<winrt::Microsoft::UI::Xaml::Media::Stretch>(GetValue(s_stretchProperty));
	}

	void ImageBlendBrush::Stretch(winrt::Microsoft::UI::Xaml::Media::Stretch value)
	{
		SetValue(s_stretchProperty, box_value(value));
	}

	DependencyProperty ImageBlendBrush::StretchProperty() { return s_stretchProperty; }

	winrt::OpenNet::UI::Xaml::Media::ImageBlendMode ImageBlendBrush::Mode() const
	{
		return unbox_value<OpenNet::UI::Xaml::Media::ImageBlendMode>(GetValue(s_modeProperty));
	}

	void ImageBlendBrush::Mode(winrt::OpenNet::UI::Xaml::Media::ImageBlendMode value)
	{
		SetValue(s_modeProperty, box_value(value));
	}

	DependencyProperty ImageBlendBrush::ModeProperty() { return s_modeProperty; }

	CompositionStretch ImageBlendBrush::CompositionStretchFromStretch(winrt::Microsoft::UI::Xaml::Media::Stretch value)
	{
		switch (value)
		{
		case winrt::Microsoft::UI::Xaml::Media::Stretch::Fill:
			return CompositionStretch::Fill;
		case winrt::Microsoft::UI::Xaml::Media::Stretch::Uniform:
			return CompositionStretch::Uniform;
		case winrt::Microsoft::UI::Xaml::Media::Stretch::UniformToFill:
			return CompositionStretch::UniformToFill;
		default:
			return CompositionStretch::None;
		}
	}

	void ImageBlendBrush::OnConnected()
	{
		try
		{
			auto compositor = CompositionTarget::GetCompositorForCurrentThread();

			auto source = Source();
			if (!source) return;

			auto bitmapImage = source.try_as<winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage>();
			if (!bitmapImage) return;

			m_surface = winrt::Microsoft::UI::Xaml::Media::LoadedImageSurface::StartLoadFromUri(bitmapImage.UriSource());
			m_surfaceBrush = compositor.CreateSurfaceBrush(m_surface);
			m_surfaceBrush.Stretch(CompositionStretchFromStretch(Stretch()));

			auto backdrop = compositor.CreateBackdropBrush();

			winrt::Microsoft::Graphics::Canvas::Effects::BlendEffect blendEffect;
			blendEffect.Mode(static_cast<winrt::Microsoft::Graphics::Canvas::Effects::BlendEffectMode>(
				static_cast<int32_t>(Mode())));
			blendEffect.Background(CompositionEffectSourceParameter(L"backdrop"));
			blendEffect.Foreground(CompositionEffectSourceParameter(L"image"));

			auto factory = compositor.CreateEffectFactory(
				blendEffect.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>());
			auto effectBrush = factory.CreateBrush();

			effectBrush.SetSourceParameter(L"backdrop", backdrop);
			effectBrush.SetSourceParameter(L"image", m_surfaceBrush);

			CompositionBrush(effectBrush);
		}
		catch (...) {}
	}

	void ImageBlendBrush::OnDisconnected()
	{
		if (auto brush = CompositionBrush())
		{
			brush.Close();
			CompositionBrush(nullptr);
		}
		if (m_surfaceBrush)
		{
			m_surfaceBrush.Close();
			m_surfaceBrush = nullptr;
		}
		if (m_surface)
		{
			m_surface.Close();
			m_surface = nullptr;
		}
	}

	// =====================================================================
	// AcrylicBrush (enhanced with EffectSetter support)
	// =====================================================================

	DependencyProperty AcrylicBrush::s_blurAmountProperty =
		DependencyProperty::Register(
			L"BlurAmount",
			xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::AcrylicBrush>(),
			PropertyMetadata{ box_value(30.0) });

	DependencyProperty AcrylicBrush::s_tintColorProperty =
		DependencyProperty::Register(
			L"TintColor",
			xaml_typename<winrt::Windows::UI::Color>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::AcrylicBrush>(),
			PropertyMetadata{ box_value(winrt::Windows::UI::Color{ 255, 255, 255, 255 }) });

	DependencyProperty AcrylicBrush::s_tintOpacityProperty =
		DependencyProperty::Register(
			L"TintOpacity",
			xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::AcrylicBrush>(),
			PropertyMetadata{ box_value(0.5) });

	DependencyProperty AcrylicBrush::s_textureUriProperty =
		DependencyProperty::Register(
			L"TextureUri",
			xaml_typename<winrt::Windows::Foundation::Uri>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::AcrylicBrush>(),
			PropertyMetadata{ nullptr });

	double AcrylicBrush::BlurAmount() const { return unbox_value<double>(GetValue(s_blurAmountProperty)); }
	void AcrylicBrush::BlurAmount(double value) { SetValue(s_blurAmountProperty, box_value(std::max(value, 0.0))); }
	DependencyProperty AcrylicBrush::BlurAmountProperty() { return s_blurAmountProperty; }

	winrt::Windows::UI::Color AcrylicBrush::TintColor() const { return unbox_value<winrt::Windows::UI::Color>(GetValue(s_tintColorProperty)); }
	void AcrylicBrush::TintColor(winrt::Windows::UI::Color value) { SetValue(s_tintColorProperty, box_value(value)); }
	DependencyProperty AcrylicBrush::TintColorProperty() { return s_tintColorProperty; }

	double AcrylicBrush::TintOpacity() const { return unbox_value<double>(GetValue(s_tintOpacityProperty)); }
	void AcrylicBrush::TintOpacity(double value) { SetValue(s_tintOpacityProperty, box_value(std::clamp(value, 0.0, 1.0))); }
	DependencyProperty AcrylicBrush::TintOpacityProperty() { return s_tintOpacityProperty; }

	winrt::Windows::Foundation::Uri AcrylicBrush::TextureUri() const
	{
		auto val = GetValue(s_textureUriProperty);
		return val ? val.as<winrt::Windows::Foundation::Uri>() : nullptr;
	}
	void AcrylicBrush::TextureUri(winrt::Windows::Foundation::Uri const& value) { SetValue(s_textureUriProperty, value); }
	DependencyProperty AcrylicBrush::TextureUriProperty() { return s_textureUriProperty; }

	void AcrylicBrush::OnConnected()
	{
		BuildBrush();
	}

	void AcrylicBrush::OnDisconnected()
	{
		CompositionBrush(nullptr);
		m_blurSetter = nullptr;
		m_tintColorSetter = nullptr;
		m_tintOpacitySetter = nullptr;
	}

	void AcrylicBrush::BuildBrush()
	{
		try
		{
			// Build with setters for dynamic property updates
			auto pipeline = PipelineBuilder::FromBackdrop()
				.Blur(static_cast<float>(BlurAmount()), m_blurSetter)
				.Tint(TintColor(), m_tintColorSetter)
				.Opacity(static_cast<float>(TintOpacity()), m_tintOpacitySetter);

			CompositionBrush(pipeline.Build());
		}
		catch (...) {}
	}

	// =====================================================================
	// TilesBrush
	// =====================================================================

	DependencyProperty TilesBrush::s_textureUriProperty =
		DependencyProperty::Register(
			L"TextureUri",
			xaml_typename<winrt::Windows::Foundation::Uri>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::TilesBrush>(),
			PropertyMetadata{ nullptr });

	DependencyProperty TilesBrush::s_dpiModeProperty =
		DependencyProperty::Register(
			L"DpiMode",
			xaml_typename<OpenNet::UI::Xaml::Media::DpiMode>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Brushes::TilesBrush>(),
			PropertyMetadata{ box_value(OpenNet::UI::Xaml::Media::DpiMode::DisplayDpiWith96AsLowerBound) });

	winrt::Windows::Foundation::Uri TilesBrush::TextureUri() const
	{
		auto val = GetValue(s_textureUriProperty);
		return val ? val.as<winrt::Windows::Foundation::Uri>() : nullptr;
	}
	void TilesBrush::TextureUri(winrt::Windows::Foundation::Uri const& value){SetValue(s_textureUriProperty, value);}
	DependencyProperty TilesBrush::TextureUriProperty(){return s_textureUriProperty;}

	winrt::OpenNet::UI::Xaml::Media::DpiMode TilesBrush::DpiMode() const
	{
		return unbox_value<OpenNet::UI::Xaml::Media::DpiMode>(GetValue(s_dpiModeProperty));
	}
	void TilesBrush::DpiMode(winrt::OpenNet::UI::Xaml::Media::DpiMode value){SetValue(s_dpiModeProperty, box_value(value));}
	DependencyProperty TilesBrush::DpiModeProperty(){return s_dpiModeProperty;}

	void TilesBrush::OnConnected()
	{
		BuildBrush();
	}

	void TilesBrush::OnDisconnected()
	{
		CompositionBrush(nullptr);
	}

	void TilesBrush::BuildBrush()
	{
		try
		{
			auto uri = TextureUri();
			if (!uri)
			{
				// No texture, use a transparent fallback
				CompositionBrush(PipelineBuilder::FromColor(winrt::Windows::UI::Color{ 0, 0, 0, 0 }).Build());
				return;
			}

			// Load the image and create a tiled surface brush directly
			auto compositor = CompositionTarget::GetCompositorForCurrentThread();
			auto surface = winrt::Microsoft::UI::Xaml::Media::LoadedImageSurface::StartLoadFromUri(uri);
			auto surfaceBrush = compositor.CreateSurfaceBrush(surface);
			surfaceBrush.Stretch(CompositionStretch::None);

			// Enable tiling via BorderEffect
			winrt::Microsoft::Graphics::Canvas::Effects::BorderEffect borderEffect;
			borderEffect.Source(CompositionEffectSourceParameter(L"source"));
			borderEffect.ExtendX(winrt::Microsoft::Graphics::Canvas::CanvasEdgeBehavior::Wrap);
			borderEffect.ExtendY(winrt::Microsoft::Graphics::Canvas::CanvasEdgeBehavior::Wrap);

			auto factory = compositor.CreateEffectFactory(
				borderEffect.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>());
			auto effectBrush = factory.CreateBrush();
			effectBrush.SetSourceParameter(L"source", surfaceBrush);

			CompositionBrush(effectBrush);
		}
		catch (...) {}
	}
}
