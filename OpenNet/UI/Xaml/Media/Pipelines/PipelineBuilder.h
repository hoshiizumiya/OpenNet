#pragma once
// PipelineBuilder - C++/WinRT port of CommunityToolkit.WinUI.Media.Pipelines.PipelineBuilder
// Allows building custom composition effects pipelines and creating CompositionBrush instances.
// This is a pure C++ class (not a WinRT runtime type).

#include <functional>
#include <vector>
#include <map>
#include <string>
#include <random>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.Graphics.Canvas.Effects.h>

namespace OpenNet::UI::Xaml::Media::Pipelines
{
	namespace WGE = winrt::Windows::Graphics::Effects;
	namespace MUC = winrt::Microsoft::UI::Composition;
	namespace MUX = winrt::Microsoft::UI::Xaml;
	namespace MGCE = winrt::Microsoft::Graphics::Canvas::Effects;

	/// <summary>
	/// Placement enum for blend operations
	/// </summary>
	enum class Placement
	{
		Foreground,
		Background
	};

	/// <summary>
	/// Delegate that sets a property on a CompositionBrush
	/// </summary>
	template <typename T>
	using EffectSetter = std::function<void(MUC::CompositionBrush const&, T)>;

	/// <summary>
	/// Delegate that animates a property on a CompositionBrush
	/// </summary>
	template <typename T>
	using EffectAnimation = std::function<winrt::Windows::Foundation::IAsyncAction(
		MUC::CompositionBrush const&, T, winrt::Windows::Foundation::TimeSpan)>;

	/// <summary>
	/// A simple container for custom composition effect source parameter info
	/// </summary>
	struct BrushProvider
	{
		std::wstring Name;
		std::function<MUC::CompositionBrush()> Initializer;

		static BrushProvider New(std::wstring name, MUC::CompositionBrush const& brush)
		{
			return { std::move(name), [brush]() { return brush; } };
		}

		static BrushProvider New(std::wstring name, std::function<MUC::CompositionBrush()> factory)
		{
			return { std::move(name), std::move(factory) };
		}
	};

	/// <summary>
	/// Generates a random uppercase ASCII string for unique effect names
	/// </summary>
	inline std::wstring GenerateEffectId()
	{
		static thread_local std::mt19937 rng{ std::random_device{}() };
		static constexpr wchar_t chars[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		std::wstring result(16, L'A');
		for (auto& c : result)
		{
			c = chars[rng() % 26];
		}
		return result;
	}

	class PipelineBuilder
	{
	public:
		using SourceProducer = std::function<WGE::IGraphicsEffectSource()>;
		using BrushProducer = std::function<MUC::CompositionBrush()>;

		// ---- Build methods ----

		/// <summary>
		/// Builds a CompositionBrush from the current effects pipeline
		/// </summary>
		MUC::CompositionBrush Build() const;

		/// <summary>
		/// Builds the pipeline and creates a SpriteVisual applied to the target element
		/// </summary>
		MUC::SpriteVisual Attach(
			MUX::UIElement const& target,
			MUX::UIElement const* reference = nullptr) const;

		// ---- Initialization (static factories) ----

		/// <summary>
		/// Creates a pipeline from a backdrop brush (CompositionBackdropBrush)
		/// </summary>
		static PipelineBuilder FromBackdrop();

		/// <summary>
		/// Creates a pipeline from a solid color
		/// </summary>
		static PipelineBuilder FromColor(winrt::Windows::UI::Color color);

		/// <summary>
		/// Creates a pipeline from a solid color with a setter for animation
		/// </summary>
		static PipelineBuilder FromColor(winrt::Windows::UI::Color color, EffectSetter<winrt::Windows::UI::Color>& setter);

		/// <summary>
		/// Creates a pipeline from an existing CompositionBrush
		/// </summary>
		static PipelineBuilder FromBrush(MUC::CompositionBrush const& brush);

		/// <summary>
		/// Creates a pipeline from an IGraphicsEffectSource
		/// </summary>
		static PipelineBuilder FromEffect(WGE::IGraphicsEffectSource const& effect);

		// ---- Effect methods ----

		/// <summary>
		/// Adds a GaussianBlurEffect
		/// </summary>
		PipelineBuilder Blur(
			float amount,
			MGCE::EffectBorderMode mode = MGCE::EffectBorderMode::Hard,
			MGCE::EffectOptimization optimization = MGCE::EffectOptimization::Balanced) const;

		/// <summary>
		/// Adds a GaussianBlurEffect with a setter
		/// </summary>
		PipelineBuilder Blur(
			float amount,
			EffectSetter<float>& setter,
			MGCE::EffectBorderMode mode = MGCE::EffectBorderMode::Hard,
			MGCE::EffectOptimization optimization = MGCE::EffectOptimization::Balanced) const;

		/// <summary>
		/// Adds a SaturationEffect
		/// </summary>
		PipelineBuilder Saturation(float saturation) const;

		/// <summary>
		/// Adds a SaturationEffect with a setter
		/// </summary>
		PipelineBuilder Saturation(float saturation, EffectSetter<float>& setter) const;

		/// <summary>
		/// Adds a SepiaEffect
		/// </summary>
		PipelineBuilder Sepia(float intensity) const;

		/// <summary>
		/// Adds an OpacityEffect
		/// </summary>
		PipelineBuilder Opacity(float opacity) const;

		/// <summary>
		/// Adds an OpacityEffect with a setter
		/// </summary>
		PipelineBuilder Opacity(float opacity, EffectSetter<float>& setter) const;

		/// <summary>
		/// Adds an ExposureEffect
		/// </summary>
		PipelineBuilder Exposure(float amount) const;

		/// <summary>
		/// Adds a HueRotationEffect
		/// </summary>
		PipelineBuilder HueRotation(float angle) const;

		/// <summary>
		/// Adds a TintEffect
		/// </summary>
		PipelineBuilder Tint(winrt::Windows::UI::Color color) const;

		/// <summary>
		/// Adds a TintEffect with a setter
		/// </summary>
		PipelineBuilder Tint(winrt::Windows::UI::Color color, EffectSetter<winrt::Windows::UI::Color>& setter) const;

		/// <summary>
		/// Adds a TemperatureAndTintEffect
		/// </summary>
		PipelineBuilder TemperatureAndTint(float temperature, float tint) const;

		/// <summary>
		/// Applies a luminance-to-alpha effect
		/// </summary>
		PipelineBuilder LuminanceToAlpha() const;

		/// <summary>
		/// Applies an invert effect
		/// </summary>
		PipelineBuilder Invert() const;

		/// <summary>
		/// Applies a grayscale effect
		/// </summary>
		PipelineBuilder Grayscale() const;

		/// <summary>
		/// Applies a shade (color + crossfade) effect
		/// </summary>
		PipelineBuilder Shade(winrt::Windows::UI::Color color, float mix) const;

		// ---- Merge methods ----

		/// <summary>
		/// Blends with another pipeline using the specified mode
		/// </summary>
		PipelineBuilder Blend(
			PipelineBuilder const& pipeline,
			MGCE::BlendEffectMode mode,
			Placement placement = Placement::Foreground) const;

		/// <summary>
		/// Cross-fades with another pipeline
		/// </summary>
		PipelineBuilder CrossFade(PipelineBuilder const& pipeline, float factor = 0.5f) const;

		/// <summary>
		/// Cross-fades with a setter
		/// </summary>
		PipelineBuilder CrossFade(PipelineBuilder const& pipeline, float factor, EffectSetter<float>& setter) const;

		/// <summary>
		/// Applies a custom effect
		/// </summary>
		PipelineBuilder Effect(
			std::function<WGE::IGraphicsEffectSource(WGE::IGraphicsEffectSource)> factory,
			std::vector<std::wstring> const& animations = {},
			std::vector<BrushProvider> const& initializers = {}) const;

		// ---- Prebuilt pipelines ----

		/// <summary>
		/// Creates an in-app backdrop acrylic effect
		/// </summary>
		static PipelineBuilder FromBackdropAcrylic(
			winrt::Windows::UI::Color tintColor,
			float tintOpacity,
			float blurAmount);

	private:
		// Private constructors - matching the C# pattern

		// From a brush factory
		explicit PipelineBuilder(BrushProducer factory);

		// From a source factory
		explicit PipelineBuilder(SourceProducer factory);

		// From factory + animations
		PipelineBuilder(SourceProducer factory, std::vector<std::wstring> animations);

		// Full constructor
		PipelineBuilder(
			SourceProducer factory,
			std::vector<std::wstring> animations,
			std::map<std::wstring, BrushProducer> lazy);

		// Chaining from existing pipeline
		PipelineBuilder(
			PipelineBuilder const& source,
			SourceProducer factory,
			std::vector<std::wstring> const& newAnimations = {},
			std::map<std::wstring, BrushProducer> const& newLazy = {});

		// Merging two pipelines
		PipelineBuilder(
			SourceProducer factory,
			PipelineBuilder const& a,
			PipelineBuilder const& b,
			std::vector<std::wstring> const& newAnimations = {},
			std::map<std::wstring, BrushProducer> const& newLazy = {});

		// Helper: merge two vectors
		static std::vector<std::wstring> MergeVectors(
			std::vector<std::wstring> const& a,
			std::vector<std::wstring> const& b);

		// Helper: merge two maps
		static std::map<std::wstring, BrushProducer> MergeMaps(
			std::map<std::wstring, BrushProducer> const& a,
			std::map<std::wstring, BrushProducer> const& b);

		SourceProducer m_sourceProducer;
		std::vector<std::wstring> m_animationProperties;
		std::map<std::wstring, BrushProducer> m_lazyParameters;
	};

} // namespace OpenNet::UI::Xaml::Media::Pipelines
