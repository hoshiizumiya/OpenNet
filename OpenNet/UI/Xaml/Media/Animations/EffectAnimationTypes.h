#pragma once

#include "UI/Xaml/Media/Animations/EffectAnimationBase.g.h"
#include "UI/Xaml/Media/Animations/BlurEffectAnimation.g.h"
#include "UI/Xaml/Media/Animations/SaturationEffectAnimation.g.h"
#include "UI/Xaml/Media/Animations/OpacityEffectAnimation.g.h"
#include "UI/Xaml/Media/Animations/ExposureEffectAnimation.g.h"
#include "UI/Xaml/Media/Animations/HueRotationEffectAnimation.g.h"
#include "UI/Xaml/Media/Animations/SepiaEffectAnimation.g.h"
#include "UI/Xaml/Media/Animations/ColorEffectAnimation.g.h"

namespace winrt::OpenNet::UI::Xaml::Media::Animations::implementation
{
	// ---- EffectAnimationBase ----
	struct EffectAnimationBase : EffectAnimationBaseT<EffectAnimationBase>
	{
		EffectAnimationBase() = default;

		winrt::Windows::Foundation::TimeSpan Duration() const;
		void Duration(winrt::Windows::Foundation::TimeSpan value);
		static Microsoft::UI::Xaml::DependencyProperty DurationProperty();

		winrt::Windows::Foundation::TimeSpan Delay() const;
		void Delay(winrt::Windows::Foundation::TimeSpan value);
		static Microsoft::UI::Xaml::DependencyProperty DelayProperty();

		/// <summary>
		/// Helper: start a scalar keyframe animation on a composition brush property
		/// </summary>
		void StartScalarAnimation(
			winrt::Microsoft::UI::Composition::CompositionBrush const& brush,
			winrt::hstring const& propertyName,
			float from, float to);

		/// <summary>
		/// Helper: start a color keyframe animation on a composition brush property
		/// </summary>
		void StartColorAnimation(
			winrt::Microsoft::UI::Composition::CompositionBrush const& brush,
			winrt::hstring const& propertyName,
			winrt::Windows::UI::Color from, winrt::Windows::UI::Color to);

	private:
		static Microsoft::UI::Xaml::DependencyProperty s_durationProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_delayProperty;
	};

	// ---- Macro to reduce boilerplate for scalar effect animations ----
#define DECLARE_SCALAR_EFFECT_ANIMATION(ClassName, EffectType)                     \
	struct ClassName : ClassName##T<ClassName, EffectAnimationBase>                  \
	{                                                                                \
		ClassName() = default;                                                        \
		                                                                              \
		OpenNet::UI::Xaml::Media::Effects::EffectType Target() const;                 \
		void Target(OpenNet::UI::Xaml::Media::Effects::EffectType const& value);      \
		static Microsoft::UI::Xaml::DependencyProperty TargetProperty();              \
		                                                                              \
		double From() const;                                                          \
		void From(double value);                                                      \
		static Microsoft::UI::Xaml::DependencyProperty FromProperty();                \
		                                                                              \
		double To() const;                                                            \
		void To(double value);                                                        \
		static Microsoft::UI::Xaml::DependencyProperty ToProperty();                  \
		                                                                              \
	private:                                                                         \
		static Microsoft::UI::Xaml::DependencyProperty s_targetProperty;              \
		static Microsoft::UI::Xaml::DependencyProperty s_fromProperty;                \
		static Microsoft::UI::Xaml::DependencyProperty s_toProperty;                  \
	}

	DECLARE_SCALAR_EFFECT_ANIMATION(BlurEffectAnimation, BlurEffect);
	DECLARE_SCALAR_EFFECT_ANIMATION(SaturationEffectAnimation, SaturationEffect);
	DECLARE_SCALAR_EFFECT_ANIMATION(OpacityEffectAnimation, OpacityEffect);
	DECLARE_SCALAR_EFFECT_ANIMATION(ExposureEffectAnimation, ExposureEffect);
	DECLARE_SCALAR_EFFECT_ANIMATION(HueRotationEffectAnimation, HueRotationEffect);
	DECLARE_SCALAR_EFFECT_ANIMATION(SepiaEffectAnimation, SepiaEffect);

#undef DECLARE_SCALAR_EFFECT_ANIMATION

	// ---- ColorEffectAnimation (different: Color type instead of double) ----
	struct ColorEffectAnimation : ColorEffectAnimationT<ColorEffectAnimation, EffectAnimationBase>
	{
		ColorEffectAnimation() = default;

		OpenNet::UI::Xaml::Media::Effects::TintEffect Target() const;
		void Target(OpenNet::UI::Xaml::Media::Effects::TintEffect const& value);
		static Microsoft::UI::Xaml::DependencyProperty TargetProperty();

		winrt::Windows::UI::Color From() const;
		void From(winrt::Windows::UI::Color value);
		static Microsoft::UI::Xaml::DependencyProperty FromProperty();

		winrt::Windows::UI::Color To() const;
		void To(winrt::Windows::UI::Color value);
		static Microsoft::UI::Xaml::DependencyProperty ToProperty();

	private:
		static Microsoft::UI::Xaml::DependencyProperty s_targetProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_fromProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_toProperty;
	};
}

namespace winrt::OpenNet::UI::Xaml::Media::Animations::factory_implementation
{
	struct EffectAnimationBase : EffectAnimationBaseT<EffectAnimationBase, implementation::EffectAnimationBase> {};
	struct BlurEffectAnimation : BlurEffectAnimationT<BlurEffectAnimation, implementation::BlurEffectAnimation> {};
	struct SaturationEffectAnimation : SaturationEffectAnimationT<SaturationEffectAnimation, implementation::SaturationEffectAnimation> {};
	struct OpacityEffectAnimation : OpacityEffectAnimationT<OpacityEffectAnimation, implementation::OpacityEffectAnimation> {};
	struct ExposureEffectAnimation : ExposureEffectAnimationT<ExposureEffectAnimation, implementation::ExposureEffectAnimation> {};
	struct HueRotationEffectAnimation : HueRotationEffectAnimationT<HueRotationEffectAnimation, implementation::HueRotationEffectAnimation> {};
	struct SepiaEffectAnimation : SepiaEffectAnimationT<SepiaEffectAnimation, implementation::SepiaEffectAnimation> {};
	struct ColorEffectAnimation : ColorEffectAnimationT<ColorEffectAnimation, implementation::ColorEffectAnimation> {};
}
