#pragma once

#include "UI/Xaml/Media/Brushes/BackdropBlurBrush.g.h"
#include "UI/Xaml/Media/Brushes/BackdropSaturationBrush.g.h"
#include "UI/Xaml/Media/Brushes/PipelineAcrylicBrush.g.h"
#include "UI/Xaml/Media/Brushes/BackdropInvertBrush.g.h"
#include "UI/Xaml/Media/Brushes/BackdropSepiaBrush.g.h"
#include "UI/Xaml/Media/Brushes/BackdropGammaTransferBrush.g.h"
#include "UI/Xaml/Media/Brushes/ImageBlendBrush.g.h"
#include "UI/Xaml/Media/Brushes/AcrylicBrush.g.h"
#include "UI/Xaml/Media/Brushes/TilesBrush.g.h"
#include "UI/Xaml/Media/Pipelines/PipelineBuilder.h"

namespace winrt::OpenNet::UI::Xaml::Media::Brushes::implementation
{
	// ---- BackdropBlurBrush ----
	struct BackdropBlurBrush : BackdropBlurBrushT<BackdropBlurBrush>
	{
		BackdropBlurBrush() = default;

		double Amount() const;
		void Amount(double value);
		static Microsoft::UI::Xaml::DependencyProperty AmountProperty();

		void OnConnected();
		void OnDisconnected();

	private:
		void BuildBrush();
		static Microsoft::UI::Xaml::DependencyProperty s_amountProperty;
	};

	// ---- BackdropSaturationBrush ----
	struct BackdropSaturationBrush : BackdropSaturationBrushT<BackdropSaturationBrush>
	{
		BackdropSaturationBrush() = default;

		double Saturation() const;
		void Saturation(double value);
		static Microsoft::UI::Xaml::DependencyProperty SaturationProperty();

		void OnConnected();
		void OnDisconnected();

	private:
		void BuildBrush();
		static Microsoft::UI::Xaml::DependencyProperty s_saturationProperty;
	};

	// ---- PipelineAcrylicBrush ----
	struct PipelineAcrylicBrush : PipelineAcrylicBrushT<PipelineAcrylicBrush>
	{
		PipelineAcrylicBrush() = default;

		double BlurAmount() const;
		void BlurAmount(double value);
		static Microsoft::UI::Xaml::DependencyProperty BlurAmountProperty();

		winrt::Windows::UI::Color TintColor() const;
		void TintColor(winrt::Windows::UI::Color value);
		static Microsoft::UI::Xaml::DependencyProperty TintColorProperty();

		double TintOpacity() const;
		void TintOpacity(double value);
		static Microsoft::UI::Xaml::DependencyProperty TintOpacityProperty();

		void OnConnected();
		void OnDisconnected();

	private:
		void BuildBrush();
		static Microsoft::UI::Xaml::DependencyProperty s_blurAmountProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_tintColorProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_tintOpacityProperty;
	};

	// ---- BackdropInvertBrush ----
	struct BackdropInvertBrush : BackdropInvertBrushT<BackdropInvertBrush>
	{
		BackdropInvertBrush() = default;
		void OnConnected();
		void OnDisconnected();
	};

	// ---- BackdropSepiaBrush ----
	struct BackdropSepiaBrush : BackdropSepiaBrushT<BackdropSepiaBrush>
	{
		BackdropSepiaBrush() = default;
		double Intensity() const;
		void Intensity(double value);
		static Microsoft::UI::Xaml::DependencyProperty IntensityProperty();
		void OnConnected();
		void OnDisconnected();
	private:
		void BuildBrush();
		static Microsoft::UI::Xaml::DependencyProperty s_intensityProperty;
	};

	// ---- BackdropGammaTransferBrush ----
	struct BackdropGammaTransferBrush : BackdropGammaTransferBrushT<BackdropGammaTransferBrush>
	{
		BackdropGammaTransferBrush() = default;
		double RedExponent() const;
		void RedExponent(double value);
		static Microsoft::UI::Xaml::DependencyProperty RedExponentProperty();
		double GreenExponent() const;
		void GreenExponent(double value);
		static Microsoft::UI::Xaml::DependencyProperty GreenExponentProperty();
		double BlueExponent() const;
		void BlueExponent(double value);
		static Microsoft::UI::Xaml::DependencyProperty BlueExponentProperty();
		void OnConnected();
		void OnDisconnected();
	private:
		void BuildBrush();
		static Microsoft::UI::Xaml::DependencyProperty s_redExponentProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_greenExponentProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_blueExponentProperty;
	};

	// ---- ImageBlendBrush ----
	struct ImageBlendBrush : ImageBlendBrushT<ImageBlendBrush>
	{
		ImageBlendBrush() = default;

		winrt::Microsoft::UI::Xaml::Media::ImageSource Source() const;
		void Source(winrt::Microsoft::UI::Xaml::Media::ImageSource const& value);
		static Microsoft::UI::Xaml::DependencyProperty SourceProperty();

		winrt::Microsoft::UI::Xaml::Media::Stretch Stretch() const;
		void Stretch(winrt::Microsoft::UI::Xaml::Media::Stretch value);
		static Microsoft::UI::Xaml::DependencyProperty StretchProperty();

		winrt::OpenNet::UI::Xaml::Media::ImageBlendMode Mode() const;
		void Mode(winrt::OpenNet::UI::Xaml::Media::ImageBlendMode value);
		static Microsoft::UI::Xaml::DependencyProperty ModeProperty();

		void OnConnected();
		void OnDisconnected();

	private:
		winrt::Microsoft::UI::Xaml::Media::LoadedImageSurface m_surface{ nullptr };
		winrt::Microsoft::UI::Composition::CompositionSurfaceBrush m_surfaceBrush{ nullptr };
		static Microsoft::UI::Xaml::DependencyProperty s_sourceProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_stretchProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_modeProperty;

		static winrt::Microsoft::UI::Composition::CompositionStretch CompositionStretchFromStretch(
			winrt::Microsoft::UI::Xaml::Media::Stretch value);
	};

	// ---- AcrylicBrush ----
	struct AcrylicBrush : AcrylicBrushT<AcrylicBrush>
	{
		AcrylicBrush() = default;

		double BlurAmount() const;
		void BlurAmount(double value);
		static Microsoft::UI::Xaml::DependencyProperty BlurAmountProperty();

		winrt::Windows::UI::Color TintColor() const;
		void TintColor(winrt::Windows::UI::Color value);
		static Microsoft::UI::Xaml::DependencyProperty TintColorProperty();

		double TintOpacity() const;
		void TintOpacity(double value);
		static Microsoft::UI::Xaml::DependencyProperty TintOpacityProperty();

		winrt::Windows::Foundation::Uri TextureUri() const;
		void TextureUri(winrt::Windows::Foundation::Uri const& value);
		static Microsoft::UI::Xaml::DependencyProperty TextureUriProperty();

		void OnConnected();
		void OnDisconnected();

	private:
		void BuildBrush();
		::OpenNet::UI::Xaml::Media::Pipelines::EffectSetter<float> m_blurSetter;
		::OpenNet::UI::Xaml::Media::Pipelines::EffectSetter<winrt::Windows::UI::Color> m_tintColorSetter;
		::OpenNet::UI::Xaml::Media::Pipelines::EffectSetter<float> m_tintOpacitySetter;
		static Microsoft::UI::Xaml::DependencyProperty s_blurAmountProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_tintColorProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_tintOpacityProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_textureUriProperty;
	};

	// ---- TilesBrush ----
	struct TilesBrush : TilesBrushT<TilesBrush>
	{
		TilesBrush() = default;

		winrt::Windows::Foundation::Uri TextureUri() const;
		void TextureUri(winrt::Windows::Foundation::Uri const& value);
		static Microsoft::UI::Xaml::DependencyProperty TextureUriProperty();

		winrt::OpenNet::UI::Xaml::Media::DpiMode DpiMode() const;
		void DpiMode(winrt::OpenNet::UI::Xaml::Media::DpiMode value);
		static Microsoft::UI::Xaml::DependencyProperty DpiModeProperty();

		void OnConnected();
		void OnDisconnected();

	private:
		void BuildBrush();
		static Microsoft::UI::Xaml::DependencyProperty s_textureUriProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_dpiModeProperty;
	};
}

namespace winrt::OpenNet::UI::Xaml::Media::Brushes::factory_implementation
{
	struct BackdropBlurBrush : BackdropBlurBrushT<BackdropBlurBrush, implementation::BackdropBlurBrush> {};
	struct BackdropSaturationBrush : BackdropSaturationBrushT<BackdropSaturationBrush, implementation::BackdropSaturationBrush> {};
	struct PipelineAcrylicBrush : PipelineAcrylicBrushT<PipelineAcrylicBrush, implementation::PipelineAcrylicBrush> {};
	struct BackdropInvertBrush : BackdropInvertBrushT<BackdropInvertBrush, implementation::BackdropInvertBrush> {};
	struct BackdropSepiaBrush : BackdropSepiaBrushT<BackdropSepiaBrush, implementation::BackdropSepiaBrush> {};
	struct BackdropGammaTransferBrush : BackdropGammaTransferBrushT<BackdropGammaTransferBrush, implementation::BackdropGammaTransferBrush> {};
	struct ImageBlendBrush : ImageBlendBrushT<ImageBlendBrush, implementation::ImageBlendBrush> {};
	struct AcrylicBrush : AcrylicBrushT<AcrylicBrush, implementation::AcrylicBrush> {};
	struct TilesBrush : TilesBrushT<TilesBrush, implementation::TilesBrush> {};
}
