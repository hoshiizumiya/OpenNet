#pragma once
// CompositionObjectExtensions - C++/WinRT port of CommunityToolkit CompositionObjectExtensions
// Utility functions for binding visual sizes and animating composition properties

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

namespace OpenNet::UI::Xaml::Media::Helpers
{
	namespace MUC = winrt::Microsoft::UI::Composition;
	namespace MUX = winrt::Microsoft::UI::Xaml;

	/// <summary>
	/// Starts an ExpressionAnimation to keep the size of the source Visual in sync with the target UIElement
	/// </summary>
	inline void BindVisualSize(MUC::Visual const& source, MUX::UIElement const& target)
	{
		auto visual = MUX::Hosting::ElementCompositionPreview::GetElementVisual(target);
		auto bindSizeAnimation = source.Compositor().CreateExpressionAnimation(L"visual.Size");
		bindSizeAnimation.SetReferenceParameter(L"visual", visual);
		source.StartAnimation(L"Size", bindSizeAnimation);
	}

	/// <summary>
	/// Starts a scalar keyframe animation on a composition property and returns when complete
	/// </summary>
	inline winrt::Windows::Foundation::IAsyncAction AnimateScalarAsync(
		MUC::CompositionObject const& target,
		winrt::hstring const& property,
		float value,
		winrt::Windows::Foundation::TimeSpan duration)
	{
		target.StopAnimation(property);

		auto animation = target.Compositor().CreateScalarKeyFrameAnimation();
		animation.InsertKeyFrame(1.0f, value);
		animation.Duration(duration);

		auto batch = target.Compositor().CreateScopedBatch(MUC::CompositionBatchTypes::Animation);

		winrt::handle event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
		HANDLE rawEvent = event.get();

		batch.Completed([rawEvent](auto&&, auto&&) {
			SetEvent(rawEvent);
		});

		target.StartAnimation(property, animation);
		batch.End();

		co_await winrt::resume_on_signal(event.get());
	}

	/// <summary>
	/// Starts a color keyframe animation on a composition property and returns when complete
	/// </summary>
	inline winrt::Windows::Foundation::IAsyncAction AnimateColorAsync(
		MUC::CompositionObject const& target,
		winrt::hstring const& property,
		winrt::Windows::UI::Color value,
		winrt::Windows::Foundation::TimeSpan duration)
	{
		target.StopAnimation(property);

		auto animation = target.Compositor().CreateColorKeyFrameAnimation();
		animation.InsertKeyFrame(1.0f, value);
		animation.Duration(duration);

		auto batch = target.Compositor().CreateScopedBatch(MUC::CompositionBatchTypes::Animation);

		winrt::handle event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
		HANDLE rawEvent = event.get();

		batch.Completed([rawEvent](auto&&, auto&&) {
			SetEvent(rawEvent);
		});

		target.StartAnimation(property, animation);
		batch.End();

		co_await winrt::resume_on_signal(event.get());
	}
}
