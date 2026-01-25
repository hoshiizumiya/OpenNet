#include "pch.h"
#include "Implicit.h"
#if __has_include("UI/Xaml/Media/Animations/Implicit.g.cpp")
#include "UI/Xaml/Media/Animations/Implicit.g.cpp"
#endif

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

using namespace winrt;

namespace winrt::OpenNet::UI::Xaml::Media::Animations::implementation
{
	static Microsoft::UI::Xaml::DependencyProperty s_implicitAnimationsProperty{ nullptr };

	Implicit::Implicit()
	{
	}

	Microsoft::UI::Xaml::DependencyProperty Implicit::AnimationsProperty()
	{
		if (s_implicitAnimationsProperty)
		{
			return s_implicitAnimationsProperty;
		}

		s_implicitAnimationsProperty = Microsoft::UI::Xaml::DependencyProperty::RegisterAttached(
			L"Animations",
			xaml_typename<winrt::OpenNet::UI::Xaml::Media::Animations::ImplicitAnimationSet>(),
			xaml_typename<Microsoft::UI::Xaml::DependencyObject>(),
			Microsoft::UI::Xaml::PropertyMetadata{ nullptr, Microsoft::UI::Xaml::PropertyChangedCallback{ [](Microsoft::UI::Xaml::DependencyObject const& d, Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e)
			{
				auto uiElement = d.try_as<Microsoft::UI::Xaml::UIElement>();
				if (!uiElement)
				{
					return;
				}

				auto visual = Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(uiElement);

				auto animationSet = e.NewValue().try_as<winrt::OpenNet::UI::Xaml::Media::Animations::ImplicitAnimationSet>();
				if (!animationSet)
				{
					visual.ImplicitAnimations(nullptr);
					return;
				}

				Windows::Foundation::TimeSpan offsetDuration{};
				for (auto const& child : animationSet.Children())
				{
					if (auto offsetAnim = child.try_as<winrt::OpenNet::UI::Xaml::Media::Animations::OffsetAnimation>())
					{
						offsetDuration = offsetAnim.as<winrt::OpenNet::UI::Xaml::Media::Animations::IOffsetAnimation>().OffsetDuration();
						break;
					}
				}

				if (offsetDuration == Windows::Foundation::TimeSpan{})
				{
					visual.ImplicitAnimations(nullptr);
					return;
				}

				auto compositor = visual.Compositor();
				auto collection = compositor.CreateImplicitAnimationCollection();

				auto animation = compositor.CreateVector3KeyFrameAnimation();
				animation.Target(L"Offset");
				animation.Duration(offsetDuration);
				animation.InsertExpressionKeyFrame(0.0f, L"this.StartingValue");
				animation.InsertExpressionKeyFrame(1.0f, L"this.FinalValue");

				collection.Insert(L"Offset", animation);
				visual.ImplicitAnimations(collection);
			} } });

		return s_implicitAnimationsProperty;
	}

	winrt::OpenNet::UI::Xaml::Media::Animations::ImplicitAnimationSet Implicit::GetAnimations(Microsoft::UI::Xaml::DependencyObject const& obj)
	{
		return obj.GetValue(AnimationsProperty()).try_as<winrt::OpenNet::UI::Xaml::Media::Animations::ImplicitAnimationSet>();
	}

	void Implicit::SetAnimations(Microsoft::UI::Xaml::DependencyObject const& obj, winrt::OpenNet::UI::Xaml::Media::Animations::ImplicitAnimationSet const& value)
	{
		obj.SetValue(AnimationsProperty(), value);
	}
}
