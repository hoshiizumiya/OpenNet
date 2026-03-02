#pragma once

#include "UI/Xaml/Media/Effects/BlurEffect.g.h"
#include "UI/Xaml/Media/Effects/SaturationEffect.g.h"
#include "UI/Xaml/Media/Effects/OpacityEffect.g.h"
#include "UI/Xaml/Media/Effects/ExposureEffect.g.h"
#include "UI/Xaml/Media/Effects/HueRotationEffect.g.h"
#include "UI/Xaml/Media/Effects/SepiaEffect.g.h"
#include "UI/Xaml/Media/Effects/TintEffect.g.h"
#include "UI/Xaml/Media/Effects/GrayscaleEffect.g.h"
#include "UI/Xaml/Media/Effects/InvertEffect.g.h"
#include "UI/Xaml/Media/Effects/LuminanceToAlphaEffect.g.h"
#include "UI/Xaml/Media/Effects/ShadeEffect.g.h"
#include "UI/Xaml/Media/Effects/TemperatureAndTintEffect.g.h"
#include "UI/Xaml/Media/Effects/PipelineEffect.h"

namespace winrt::OpenNet::UI::Xaml::Media::Effects::implementation
{
	// ---- BlurEffect ----
	struct BlurEffect : BlurEffectT<BlurEffect, PipelineEffect>
	{
		BlurEffect() = default;

		double Amount() const { return m_amount; }
		void Amount(double value) { m_amount = value; }

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.Blur(static_cast<float>(m_amount));
		}

	private:
		double m_amount{ 0.0 };
	};

	// ---- SaturationEffect ----
	struct SaturationEffect : SaturationEffectT<SaturationEffect, PipelineEffect>
	{
		SaturationEffect() = default;

		double Value() const { return m_value; }
		void Value(double value) { m_value = value; }

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.Saturation(static_cast<float>(m_value));
		}

	private:
		double m_value{ 1.0 };
	};

	// ---- OpacityEffect ----
	struct OpacityEffect : OpacityEffectT<OpacityEffect, PipelineEffect>
	{
		OpacityEffect() = default;

		double Value() const { return m_value; }
		void Value(double value) { m_value = value; }

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.Opacity(static_cast<float>(m_value));
		}

	private:
		double m_value{ 1.0 };
	};

	// ---- ExposureEffect ----
	struct ExposureEffect : ExposureEffectT<ExposureEffect, PipelineEffect>
	{
		ExposureEffect() = default;

		double Amount() const { return m_amount; }
		void Amount(double value) { m_amount = value; }

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.Exposure(static_cast<float>(m_amount));
		}

	private:
		double m_amount{ 0.0 };
	};

	// ---- HueRotationEffect ----
	struct HueRotationEffect : HueRotationEffectT<HueRotationEffect, PipelineEffect>
	{
		HueRotationEffect() = default;

		double Angle() const { return m_angle; }
		void Angle(double value) { m_angle = value; }

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.HueRotation(static_cast<float>(m_angle));
		}

	private:
		double m_angle{ 0.0 };
	};

	// ---- SepiaEffect ----
	struct SepiaEffect : SepiaEffectT<SepiaEffect, PipelineEffect>
	{
		SepiaEffect() = default;

		double Intensity() const { return m_intensity; }
		void Intensity(double value) { m_intensity = value; }

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.Sepia(static_cast<float>(m_intensity));
		}

	private:
		double m_intensity{ 0.5 };
	};

	// ---- TintEffect ----
	struct TintEffect : TintEffectT<TintEffect, PipelineEffect>
	{
		TintEffect() = default;

		winrt::Windows::UI::Color Color() const { return m_color; }
		void Color(winrt::Windows::UI::Color value) { m_color = value; }

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.Tint(m_color);
		}

	private:
		winrt::Windows::UI::Color m_color{};
	};

	// ---- GrayscaleEffect ----
	struct GrayscaleEffect : GrayscaleEffectT<GrayscaleEffect, PipelineEffect>
	{
		GrayscaleEffect() = default;

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.Grayscale();
		}
	};

	// ---- InvertEffect ----
	struct InvertEffect : InvertEffectT<InvertEffect, PipelineEffect>
	{
		InvertEffect() = default;

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.Invert();
		}
	};

	// ---- LuminanceToAlphaEffect ----
	struct LuminanceToAlphaEffect : LuminanceToAlphaEffectT<LuminanceToAlphaEffect, PipelineEffect>
	{
		LuminanceToAlphaEffect() = default;

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.LuminanceToAlpha();
		}
	};

	// ---- ShadeEffect ----
	struct ShadeEffect : ShadeEffectT<ShadeEffect, PipelineEffect>
	{
		ShadeEffect() = default;

		winrt::Windows::UI::Color Color() const { return m_color; }
		void Color(winrt::Windows::UI::Color value) { m_color = value; }

		double Intensity() const { return m_intensity; }
		void Intensity(double value) { m_intensity = value; }

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.Shade(m_color, static_cast<float>(m_intensity));
		}

	private:
		winrt::Windows::UI::Color m_color{};
		double m_intensity{ 0.5 };
	};

	// ---- TemperatureAndTintEffect ----
	struct TemperatureAndTintEffect : TemperatureAndTintEffectT<TemperatureAndTintEffect, PipelineEffect>
	{
		TemperatureAndTintEffect() = default;

		double Temperature() const { return m_temperature; }
		void Temperature(double value) { m_temperature = value; }

		double Tint() const { return m_tint; }
		void Tint(double value) { m_tint = value; }

		::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder
			AppendToPipeline(::OpenNet::UI::Xaml::Media::Pipelines::PipelineBuilder builder) const override
		{
			return builder.TemperatureAndTint(static_cast<float>(m_temperature), static_cast<float>(m_tint));
		}

	private:
		double m_temperature{ 0.0 };
		double m_tint{ 0.0 };
	};
}

// Factory implementations
namespace winrt::OpenNet::UI::Xaml::Media::Effects::factory_implementation
{
	struct BlurEffect : BlurEffectT<BlurEffect, implementation::BlurEffect> {};
	struct SaturationEffect : SaturationEffectT<SaturationEffect, implementation::SaturationEffect> {};
	struct OpacityEffect : OpacityEffectT<OpacityEffect, implementation::OpacityEffect> {};
	struct ExposureEffect : ExposureEffectT<ExposureEffect, implementation::ExposureEffect> {};
	struct HueRotationEffect : HueRotationEffectT<HueRotationEffect, implementation::HueRotationEffect> {};
	struct SepiaEffect : SepiaEffectT<SepiaEffect, implementation::SepiaEffect> {};
	struct TintEffect : TintEffectT<TintEffect, implementation::TintEffect> {};
	struct GrayscaleEffect : GrayscaleEffectT<GrayscaleEffect, implementation::GrayscaleEffect> {};
	struct InvertEffect : InvertEffectT<InvertEffect, implementation::InvertEffect> {};
	struct LuminanceToAlphaEffect : LuminanceToAlphaEffectT<LuminanceToAlphaEffect, implementation::LuminanceToAlphaEffect> {};
	struct ShadeEffect : ShadeEffectT<ShadeEffect, implementation::ShadeEffect> {};
	struct TemperatureAndTintEffect : TemperatureAndTintEffectT<TemperatureAndTintEffect, implementation::TemperatureAndTintEffect> {};
}
