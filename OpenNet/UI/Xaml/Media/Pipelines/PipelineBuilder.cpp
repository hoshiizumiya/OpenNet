#include "pch.h"
#include "UI/Xaml/Media/Pipelines/PipelineBuilder.h"

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::Graphics::Effects;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Hosting;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::Graphics::Canvas::Effects;

namespace OpenNet::UI::Xaml::Media::Pipelines
{
	// =====================================================================
	// Private constructors
	// =====================================================================

	PipelineBuilder::PipelineBuilder(BrushProducer factory)
		: m_sourceProducer(nullptr),
		m_animationProperties{},
		m_lazyParameters{}
	{
		auto name = GenerateEffectId();
		auto paramName = name;

		// The source producer returns a CompositionEffectSourceParameter referencing the brush
		m_sourceProducer = [paramName]() -> IGraphicsEffectSource
			{
				return CompositionEffectSourceParameter(winrt::hstring(paramName));
			};

		m_lazyParameters[paramName] = std::move(factory);
	}

	PipelineBuilder::PipelineBuilder(SourceProducer factory)
		: m_sourceProducer(std::move(factory)),
		m_animationProperties{},
		m_lazyParameters{}
	{
	}

	PipelineBuilder::PipelineBuilder(SourceProducer factory, std::vector<std::wstring> animations)
		: m_sourceProducer(std::move(factory)),
		m_animationProperties(std::move(animations)),
		m_lazyParameters{}
	{
	}

	PipelineBuilder::PipelineBuilder(
		SourceProducer factory,
		std::vector<std::wstring> animations,
		std::map<std::wstring, BrushProducer> lazy)
		: m_sourceProducer(std::move(factory)),
		m_animationProperties(std::move(animations)),
		m_lazyParameters(std::move(lazy))
	{
	}

	PipelineBuilder::PipelineBuilder(
		PipelineBuilder const& source,
		SourceProducer factory,
		std::vector<std::wstring> const& newAnimations,
		std::map<std::wstring, BrushProducer> const& newLazy)
		: m_sourceProducer(std::move(factory)),
		m_animationProperties(MergeVectors(source.m_animationProperties, newAnimations)),
		m_lazyParameters(MergeMaps(source.m_lazyParameters, newLazy))
	{
	}

	PipelineBuilder::PipelineBuilder(
		SourceProducer factory,
		PipelineBuilder const& a,
		PipelineBuilder const& b,
		std::vector<std::wstring> const& newAnimations,
		std::map<std::wstring, BrushProducer> const& newLazy)
		: m_sourceProducer(std::move(factory)),
		m_animationProperties(MergeVectors(MergeVectors(a.m_animationProperties, b.m_animationProperties), newAnimations)),
		m_lazyParameters(MergeMaps(MergeMaps(a.m_lazyParameters, b.m_lazyParameters), newLazy))
	{
	}

	// =====================================================================
	// Helpers
	// =====================================================================

	std::vector<std::wstring> PipelineBuilder::MergeVectors(
		std::vector<std::wstring> const& a,
		std::vector<std::wstring> const& b)
	{
		std::vector<std::wstring> result;
		result.reserve(a.size() + b.size());
		result.insert(result.end(), a.begin(), a.end());
		result.insert(result.end(), b.begin(), b.end());
		return result;
	}

	std::map<std::wstring, PipelineBuilder::BrushProducer> PipelineBuilder::MergeMaps(
		std::map<std::wstring, BrushProducer> const& a,
		std::map<std::wstring, BrushProducer> const& b)
	{
		std::map<std::wstring, BrushProducer> result{ a };
		for (auto& [key, val] : b)
		{
			result[key] = val;
		}
		return result;
	}

	// =====================================================================
	// Build methods
	// =====================================================================

	CompositionBrush PipelineBuilder::Build() const
	{
		auto compositor = CompositionTarget::GetCompositorForCurrentThread();

		// Execute the source producer chain to get the final effect graph
		auto effectSource = m_sourceProducer();
		auto effect = effectSource.as<IGraphicsEffect>();

		// Create effect factory, with animation support if needed
		CompositionEffectFactory factory{ nullptr };
		if (m_animationProperties.empty())
		{
			factory = compositor.CreateEffectFactory(effect);
		}
		else
		{
			auto vec = winrt::single_threaded_vector<winrt::hstring>();
			for (auto& prop : m_animationProperties)
			{
				vec.Append(winrt::hstring(prop));
			}
			factory = compositor.CreateEffectFactory(effect, vec);
		}

		// Create the brush and bind lazy parameters
		auto effectBrush = factory.CreateBrush();
		for (auto& [name, producer] : m_lazyParameters)
		{
			effectBrush.SetSourceParameter(winrt::hstring(name), producer());
		}

		return effectBrush;
	}

	SpriteVisual PipelineBuilder::Attach(
		UIElement const& target,
		UIElement const* reference) const
	{
		auto brush = Build();
		auto compositor = brush.Compositor();

		auto visual = compositor.CreateSpriteVisual();
		visual.Brush(brush);

		// Size the visual to the reference element (or target if no reference)
		auto const& sizeElement = reference ? *reference : target;
		auto sizeExpr = compositor.CreateExpressionAnimation(L"visual.Size");
		auto hostVisual = ElementCompositionPreview::GetElementVisual(sizeElement);
		sizeExpr.SetReferenceParameter(L"visual", hostVisual);
		visual.StartAnimation(L"Size", sizeExpr);

		ElementCompositionPreview::SetElementChildVisual(target, visual);

		return visual;
	}

	// =====================================================================
	// Initialization (static factories)
	// =====================================================================

	PipelineBuilder PipelineBuilder::FromBackdrop()
	{
		return PipelineBuilder(BrushProducer([]() -> CompositionBrush
			{
				auto compositor = CompositionTarget::GetCompositorForCurrentThread();
				return compositor.CreateBackdropBrush();
			}));
	}

	PipelineBuilder PipelineBuilder::FromColor(Color color)
	{
		return PipelineBuilder(SourceProducer([color]() -> IGraphicsEffectSource
			{
				ColorSourceEffect effect;
				effect.Color(color);
				return effect;
			}));
	}

	PipelineBuilder PipelineBuilder::FromColor(Color color, EffectSetter<Color>& setter)
	{
		auto id = GenerateEffectId();
		auto propertyName = L"Color";
		auto fullPath = id + L"." + propertyName;

		setter = [fullPath](CompositionBrush const& brush, Color value)
			{
				brush.Properties().InsertColor(winrt::hstring(fullPath), value);
			};

		return PipelineBuilder(
			SourceProducer([id, color]() -> IGraphicsEffectSource
				{
					ColorSourceEffect effect;
					effect.Name(winrt::hstring(id));
					effect.Color(color);
					return effect;
				}),
			{ fullPath });
	}

	PipelineBuilder PipelineBuilder::FromBrush(CompositionBrush const& brush)
	{
		return PipelineBuilder(BrushProducer([brush]() { return brush; }));
	}

	PipelineBuilder PipelineBuilder::FromEffect(IGraphicsEffectSource const& effect)
	{
		return PipelineBuilder(SourceProducer([effect]() { return effect; }));
	}

	// =====================================================================
	// Effect methods
	// =====================================================================

	PipelineBuilder PipelineBuilder::Blur(
		float amount,
		EffectBorderMode mode,
		EffectOptimization optimization) const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer, amount, mode, optimization]() -> IGraphicsEffectSource
			{
				GaussianBlurEffect blur;
				blur.BlurAmount(amount);
				blur.BorderMode(mode);
				blur.Optimization(optimization);
				blur.Source(previousProducer());
				return blur;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::Blur(
		float amount,
		EffectSetter<float>& setter,
		EffectBorderMode mode,
		EffectOptimization optimization) const
	{
		auto previousProducer = m_sourceProducer;
		auto id = GenerateEffectId();
		auto propName = L"BlurAmount";
		auto fullPath = id + L"." + propName;

		setter = [fullPath](CompositionBrush const& brush, float value)
			{
				brush.Properties().InsertScalar(winrt::hstring(fullPath), value);
			};

		SourceProducer newProducer = [previousProducer, amount, mode, optimization, id]() -> IGraphicsEffectSource
			{
				GaussianBlurEffect blur;
				blur.Name(winrt::hstring(id));
				blur.BlurAmount(amount);
				blur.BorderMode(mode);
				blur.Optimization(optimization);
				blur.Source(previousProducer());
				return blur;
			};

		return PipelineBuilder(*this, std::move(newProducer), { fullPath });
	}

	PipelineBuilder PipelineBuilder::Saturation(float saturation) const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer, saturation]() -> IGraphicsEffectSource
			{
				SaturationEffect effect;
				effect.Saturation(saturation);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::Saturation(float saturation, EffectSetter<float>& setter) const
	{
		auto previousProducer = m_sourceProducer;
		auto id = GenerateEffectId();
		auto fullPath = id + L".Saturation";

		setter = [fullPath](CompositionBrush const& brush, float value)
			{
				brush.Properties().InsertScalar(winrt::hstring(fullPath), value);
			};

		SourceProducer newProducer = [previousProducer, saturation, id]() -> IGraphicsEffectSource
			{
				SaturationEffect effect;
				effect.Name(winrt::hstring(id));
				effect.Saturation(saturation);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer), { fullPath });
	}

	PipelineBuilder PipelineBuilder::Sepia(float intensity) const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer, intensity]() -> IGraphicsEffectSource
			{
				SepiaEffect effect;
				effect.Intensity(intensity);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::Opacity(float opacity) const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer, opacity]() -> IGraphicsEffectSource
			{
				OpacityEffect effect;
				effect.Opacity(opacity);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::Opacity(float opacity, EffectSetter<float>& setter) const
	{
		auto previousProducer = m_sourceProducer;
		auto id = GenerateEffectId();
		auto fullPath = id + L".Opacity";

		setter = [fullPath](CompositionBrush const& brush, float value)
			{
				brush.Properties().InsertScalar(winrt::hstring(fullPath), value);
			};

		SourceProducer newProducer = [previousProducer, opacity, id]() -> IGraphicsEffectSource
			{
				OpacityEffect effect;
				effect.Name(winrt::hstring(id));
				effect.Opacity(opacity);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer), { fullPath });
	}

	PipelineBuilder PipelineBuilder::Exposure(float amount) const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer, amount]() -> IGraphicsEffectSource
			{
				ExposureEffect effect;
				effect.Exposure(amount);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::HueRotation(float angle) const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer, angle]() -> IGraphicsEffectSource
			{
				HueRotationEffect effect;
				effect.Angle(angle);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::Tint(Color color) const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer, color]() -> IGraphicsEffectSource
			{
				TintEffect effect;
				effect.Color(color);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::Tint(Color color, EffectSetter<Color>& setter) const
	{
		auto previousProducer = m_sourceProducer;
		auto id = GenerateEffectId();
		auto fullPath = id + L".Color";

		setter = [fullPath](CompositionBrush const& brush, Color value)
			{
				brush.Properties().InsertColor(winrt::hstring(fullPath), value);
			};

		SourceProducer newProducer = [previousProducer, color, id]() -> IGraphicsEffectSource
			{
				TintEffect effect;
				effect.Name(winrt::hstring(id));
				effect.Color(color);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer), { fullPath });
	}

	PipelineBuilder PipelineBuilder::TemperatureAndTint(float temperature, float tint) const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer, temperature, tint]() -> IGraphicsEffectSource
			{
				TemperatureAndTintEffect effect;
				effect.Temperature(temperature);
				effect.Tint(tint);
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::LuminanceToAlpha() const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer]() -> IGraphicsEffectSource
			{
				LuminanceToAlphaEffect effect;
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::Invert() const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer]() -> IGraphicsEffectSource
			{
				InvertEffect effect;
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::Grayscale() const
	{
		auto previousProducer = m_sourceProducer;

		SourceProducer newProducer = [previousProducer]() -> IGraphicsEffectSource
			{
				GrayscaleEffect effect;
				effect.Source(previousProducer());
				return effect;
			};

		return PipelineBuilder(*this, std::move(newProducer));
	}

	PipelineBuilder PipelineBuilder::Shade(Color color, float mix) const
	{
		return Blend(FromColor(color), BlendEffectMode::Multiply)
			.CrossFade(PipelineBuilder(*this, m_sourceProducer), mix);
	}

	// =====================================================================
	// Merge methods
	// =====================================================================

	PipelineBuilder PipelineBuilder::Blend(
		PipelineBuilder const& pipeline,
		BlendEffectMode mode,
		Placement placement) const
	{
		auto foregroundProducer = (placement == Placement::Foreground)
			? pipeline.m_sourceProducer : m_sourceProducer;
		auto backgroundProducer = (placement == Placement::Foreground)
			? m_sourceProducer : pipeline.m_sourceProducer;

		SourceProducer newProducer = [foregroundProducer, backgroundProducer, mode]() -> IGraphicsEffectSource
			{
				BlendEffect blend;
				blend.Mode(mode);
				blend.Foreground(foregroundProducer());
				blend.Background(backgroundProducer());
				return blend;
			};

		return PipelineBuilder(std::move(newProducer), *this, pipeline);
	}

	PipelineBuilder PipelineBuilder::CrossFade(PipelineBuilder const& pipeline, float factor) const
	{
		auto producerA = m_sourceProducer;
		auto producerB = pipeline.m_sourceProducer;

		SourceProducer newProducer = [producerA, producerB, factor]() -> IGraphicsEffectSource
			{
				CrossFadeEffect crossFade;
				crossFade.CrossFade(factor);
				crossFade.Source1(producerA());
				crossFade.Source2(producerB());
				return crossFade;
			};

		return PipelineBuilder(std::move(newProducer), *this, pipeline);
	}

	PipelineBuilder PipelineBuilder::CrossFade(
		PipelineBuilder const& pipeline,
		float factor,
		EffectSetter<float>& setter) const
	{
		auto producerA = m_sourceProducer;
		auto producerB = pipeline.m_sourceProducer;
		auto id = GenerateEffectId();
		auto fullPath = id + L".CrossFade";

		setter = [fullPath](CompositionBrush const& brush, float value)
			{
				brush.Properties().InsertScalar(winrt::hstring(fullPath), value);
			};

		SourceProducer newProducer = [producerA, producerB, factor, id]() -> IGraphicsEffectSource
			{
				CrossFadeEffect crossFade;
				crossFade.Name(winrt::hstring(id));
				crossFade.CrossFade(factor);
				crossFade.Source1(producerA());
				crossFade.Source2(producerB());
				return crossFade;
			};

		return PipelineBuilder(std::move(newProducer), *this, pipeline, { fullPath });
	}

	PipelineBuilder PipelineBuilder::Effect(
		std::function<IGraphicsEffectSource(IGraphicsEffectSource)> factory,
		std::vector<std::wstring> const& animations,
		std::vector<BrushProvider> const& initializers) const
	{
		auto previousProducer = m_sourceProducer;

		std::map<std::wstring, BrushProducer> newLazy;
		for (auto& init : initializers)
		{
			newLazy[init.Name] = init.Initializer;
		}

		SourceProducer newProducer = [previousProducer, factory]() -> IGraphicsEffectSource
			{
				return factory(previousProducer());
			};

		return PipelineBuilder(*this, std::move(newProducer), animations, newLazy);
	}

	// =====================================================================
	// Prebuilt pipelines
	// =====================================================================

	PipelineBuilder PipelineBuilder::FromBackdropAcrylic(
		Color tintColor,
		float tintOpacity,
		float blurAmount)
	{
		return FromBackdrop()
			.Blur(blurAmount)
			.Tint(tintColor)
			.Opacity(tintOpacity);
	}

} // namespace OpenNet::UI::Xaml::Media::Pipelines
