#include "pch.h"
#include "UI/Xaml/Media/Animations/EffectAnimationTypes.h"
#include "UI/Xaml/Media/Effects/PipelineEffect.h"

#if __has_include("UI/Xaml/Media/Animations/EffectAnimationBase.g.cpp")
#include "UI/Xaml/Media/Animations/EffectAnimationBase.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Animations/BlurEffectAnimation.g.cpp")
#include "UI/Xaml/Media/Animations/BlurEffectAnimation.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Animations/SaturationEffectAnimation.g.cpp")
#include "UI/Xaml/Media/Animations/SaturationEffectAnimation.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Animations/OpacityEffectAnimation.g.cpp")
#include "UI/Xaml/Media/Animations/OpacityEffectAnimation.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Animations/ExposureEffectAnimation.g.cpp")
#include "UI/Xaml/Media/Animations/ExposureEffectAnimation.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Animations/HueRotationEffectAnimation.g.cpp")
#include "UI/Xaml/Media/Animations/HueRotationEffectAnimation.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Animations/SepiaEffectAnimation.g.cpp")
#include "UI/Xaml/Media/Animations/SepiaEffectAnimation.g.cpp"
#endif
#if __has_include("UI/Xaml/Media/Animations/ColorEffectAnimation.g.cpp")
#include "UI/Xaml/Media/Animations/ColorEffectAnimation.g.cpp"
#endif

#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Composition.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Composition;
using namespace std::chrono_literals;

namespace winrt::OpenNet::UI::Xaml::Media::Animations::implementation
{
	// =====================================================================
	// EffectAnimationBase
	// =====================================================================

	DependencyProperty EffectAnimationBase::s_durationProperty =
		DependencyProperty::Register(L"Duration", xaml_typename<winrt::Windows::Foundation::TimeSpan>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Animations::EffectAnimationBase>(),
			PropertyMetadata{ box_value(winrt::Windows::Foundation::TimeSpan{ 4000000LL }) }); // 400ms

	DependencyProperty EffectAnimationBase::s_delayProperty =
		DependencyProperty::Register(L"Delay", xaml_typename<winrt::Windows::Foundation::TimeSpan>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Animations::EffectAnimationBase>(),
			PropertyMetadata{ box_value(winrt::Windows::Foundation::TimeSpan{ 0LL }) });

	winrt::Windows::Foundation::TimeSpan EffectAnimationBase::Duration() const
	{
		return unbox_value<winrt::Windows::Foundation::TimeSpan>(GetValue(s_durationProperty));
	}
	void EffectAnimationBase::Duration(winrt::Windows::Foundation::TimeSpan value)
	{
		SetValue(s_durationProperty, box_value(value));
	}
	DependencyProperty EffectAnimationBase::DurationProperty() { return s_durationProperty; }

	winrt::Windows::Foundation::TimeSpan EffectAnimationBase::Delay() const
	{
		return unbox_value<winrt::Windows::Foundation::TimeSpan>(GetValue(s_delayProperty));
	}
	void EffectAnimationBase::Delay(winrt::Windows::Foundation::TimeSpan value)
	{
		SetValue(s_delayProperty, box_value(value));
	}
	DependencyProperty EffectAnimationBase::DelayProperty() { return s_delayProperty; }

	void EffectAnimationBase::StartScalarAnimation(
		CompositionBrush const& brush,
		hstring const& propertyName,
		float from, float to)
	{
		if (!brush) return;
		auto compositor = brush.Compositor();

		auto animation = compositor.CreateScalarKeyFrameAnimation();
		animation.Duration(Duration());
		animation.DelayTime(Delay());

		if (from != to)
		{
			animation.InsertKeyFrame(0.0f, from);
		}
		animation.InsertKeyFrame(1.0f, to);

		brush.StartAnimation(propertyName, animation);
	}

	void EffectAnimationBase::StartColorAnimation(
		CompositionBrush const& brush,
		hstring const& propertyName,
		winrt::Windows::UI::Color from, winrt::Windows::UI::Color to)
	{
		if (!brush) return;
		auto compositor = brush.Compositor();

		auto animation = compositor.CreateColorKeyFrameAnimation();
		animation.Duration(Duration());
		animation.DelayTime(Delay());
		animation.InsertKeyFrame(0.0f, from);
		animation.InsertKeyFrame(1.0f, to);

		brush.StartAnimation(propertyName, animation);
	}

	// =====================================================================
	// Macro for scalar effect animation implementations
	// =====================================================================

#define IMPLEMENT_SCALAR_EFFECT_ANIMATION(ClassName, EffectType, EffectXamlType)           \
	DependencyProperty ClassName::s_targetProperty =                                        \
		DependencyProperty::Register(L"Target",                                              \
			xaml_typename<OpenNet::UI::Xaml::Media::Effects::EffectType>(),                    \
			xaml_typename<OpenNet::UI::Xaml::Media::Animations::ClassName>(),                  \
			PropertyMetadata{ nullptr });                                                     \
	DependencyProperty ClassName::s_fromProperty =                                          \
		DependencyProperty::Register(L"From", xaml_typename<double>(),                       \
			xaml_typename<OpenNet::UI::Xaml::Media::Animations::ClassName>(),                  \
			PropertyMetadata{ box_value(0.0) });                                              \
	DependencyProperty ClassName::s_toProperty =                                            \
		DependencyProperty::Register(L"To", xaml_typename<double>(),                         \
			xaml_typename<OpenNet::UI::Xaml::Media::Animations::ClassName>(),                  \
			PropertyMetadata{ box_value(0.0) });                                              \
	                                                                                         \
	OpenNet::UI::Xaml::Media::Effects::EffectType ClassName::Target() const                  \
	{                                                                                        \
		auto val = GetValue(s_targetProperty);                                                \
		return val ? val.as<OpenNet::UI::Xaml::Media::Effects::EffectType>() : nullptr;       \
	}                                                                                        \
	void ClassName::Target(OpenNet::UI::Xaml::Media::Effects::EffectType const& value)       \
	{                                                                                        \
		SetValue(s_targetProperty, value);                                                    \
	}                                                                                        \
	DependencyProperty ClassName::TargetProperty() { return s_targetProperty; }              \
	                                                                                         \
	double ClassName::From() const { return unbox_value<double>(GetValue(s_fromProperty)); }  \
	void ClassName::From(double value) { SetValue(s_fromProperty, box_value(value)); }       \
	DependencyProperty ClassName::FromProperty() { return s_fromProperty; }                  \
	                                                                                         \
	double ClassName::To() const { return unbox_value<double>(GetValue(s_toProperty)); }      \
	void ClassName::To(double value) { SetValue(s_toProperty, box_value(value)); }           \
	DependencyProperty ClassName::ToProperty() { return s_toProperty; }

	IMPLEMENT_SCALAR_EFFECT_ANIMATION(BlurEffectAnimation, BlurEffect, BlurEffect)
	IMPLEMENT_SCALAR_EFFECT_ANIMATION(SaturationEffectAnimation, SaturationEffect, SaturationEffect)
	IMPLEMENT_SCALAR_EFFECT_ANIMATION(OpacityEffectAnimation, OpacityEffect, OpacityEffect)
	IMPLEMENT_SCALAR_EFFECT_ANIMATION(ExposureEffectAnimation, ExposureEffect, ExposureEffect)
	IMPLEMENT_SCALAR_EFFECT_ANIMATION(HueRotationEffectAnimation, HueRotationEffect, HueRotationEffect)
	IMPLEMENT_SCALAR_EFFECT_ANIMATION(SepiaEffectAnimation, SepiaEffect, SepiaEffect)

#undef IMPLEMENT_SCALAR_EFFECT_ANIMATION

	// =====================================================================
	// ColorEffectAnimation
	// =====================================================================

	DependencyProperty ColorEffectAnimation::s_targetProperty =
		DependencyProperty::Register(L"Target",
			xaml_typename<OpenNet::UI::Xaml::Media::Effects::TintEffect>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Animations::ColorEffectAnimation>(),
			PropertyMetadata{ nullptr });

	DependencyProperty ColorEffectAnimation::s_fromProperty =
		DependencyProperty::Register(L"From",
			xaml_typename<winrt::Windows::UI::Color>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Animations::ColorEffectAnimation>(),
			PropertyMetadata{ box_value(winrt::Windows::UI::Color{ 0, 0, 0, 0 }) });

	DependencyProperty ColorEffectAnimation::s_toProperty =
		DependencyProperty::Register(L"To",
			xaml_typename<winrt::Windows::UI::Color>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Animations::ColorEffectAnimation>(),
			PropertyMetadata{ box_value(winrt::Windows::UI::Color{ 0, 0, 0, 0 }) });

	OpenNet::UI::Xaml::Media::Effects::TintEffect ColorEffectAnimation::Target() const
	{
		auto val = GetValue(s_targetProperty);
		return val ? val.as<OpenNet::UI::Xaml::Media::Effects::TintEffect>() : nullptr;
	}
	void ColorEffectAnimation::Target(OpenNet::UI::Xaml::Media::Effects::TintEffect const& value)
	{
		SetValue(s_targetProperty, value);
	}
	DependencyProperty ColorEffectAnimation::TargetProperty() { return s_targetProperty; }

	winrt::Windows::UI::Color ColorEffectAnimation::From() const
	{
		return unbox_value<winrt::Windows::UI::Color>(GetValue(s_fromProperty));
	}
	void ColorEffectAnimation::From(winrt::Windows::UI::Color value)
	{
		SetValue(s_fromProperty, box_value(value));
	}
	DependencyProperty ColorEffectAnimation::FromProperty() { return s_fromProperty; }

	winrt::Windows::UI::Color ColorEffectAnimation::To() const
	{
		return unbox_value<winrt::Windows::UI::Color>(GetValue(s_toProperty));
	}
	void ColorEffectAnimation::To(winrt::Windows::UI::Color value)
	{
		SetValue(s_toProperty, box_value(value));
	}
	DependencyProperty ColorEffectAnimation::ToProperty() { return s_toProperty; }
}
