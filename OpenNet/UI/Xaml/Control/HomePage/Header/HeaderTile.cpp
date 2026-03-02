#include "pch.h"
#include "HeaderTile.h"
#if __has_include("UI/Xaml/Control/HomePage/Header/HeaderTile.g.cpp")
#include "UI/Xaml/Control/HomePage/Header/HeaderTile.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
	// Static dependency properties (lazy initialized)
	static DependencyProperty s_imageUrlProperty{ nullptr };
	static DependencyProperty s_headerProperty{ nullptr };
	static DependencyProperty s_descriptionProperty{ nullptr };
	static DependencyProperty s_featureIDProperty{ nullptr };
	static DependencyProperty s_isSelectedProperty{ nullptr };

	HeaderTile::HeaderTile()
	{
		DefaultStyleKey(winrt::box_value(L"OpenNet.UI.Xaml.Control.HomePage.Header.HeaderTile"));
	}

	void HeaderTile::OnApplyTemplate()
	{
		__super::OnApplyTemplate();

		// Set initial scale and center point via Composition
		auto visual = Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(*this);
		auto compositor = visual.Compositor();

		// Enable Translation property so it can be used in StartAnimation("Translation", ...)
		// without this, animating "Translation" will return E_INVALIDARG (0x80070057)
		Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::SetIsTranslationEnabled(*this, true);

		// Initial scale = 0.8 (tiles appear slightly smaller by default)
		visual.Scale({ 0.8f, 0.8f, 1.0f });

		// Center point dynamically tracks element size
		auto centerExpression = compositor.CreateExpressionAnimation(
			L"Vector3(this.Target.Size.X * 0.5, this.Target.Size.Y * 0.5, 0)");
		visual.StartAnimation(L"CenterPoint", centerExpression);

		// Set up implicit show/hide animations on TextPanel (matching C# original)
		if (auto textPanel = GetTemplateChild(L"TextPanel").try_as<UIElement>())
		{
			Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::SetIsTranslationEnabled(textPanel, true);

			// Show: opacity 0→1 (0.4s) + translation slide up from (0,200,0) (0.6s)
			auto showGroup = compositor.CreateAnimationGroup();

			auto opacityShow = compositor.CreateScalarKeyFrameAnimation();
			opacityShow.InsertKeyFrame(0.0f, 0.0f);
			opacityShow.InsertKeyFrame(1.0f, 1.0f);
			opacityShow.Duration(std::chrono::milliseconds(400));
			opacityShow.Target(L"Opacity");
			showGroup.Add(opacityShow);

			auto translationShow = compositor.CreateVector3KeyFrameAnimation();
			translationShow.InsertKeyFrame(0.0f, { 0.0f, 200.0f, 0.0f });
			translationShow.InsertKeyFrame(1.0f, { 0.0f, 0.0f, 0.0f });
			translationShow.Duration(std::chrono::milliseconds(600));
			translationShow.Target(L"Translation");
			showGroup.Add(translationShow);

			Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::SetImplicitShowAnimation(textPanel, showGroup);

			// Hide: opacity 1→0 (0.35s) + translation slide down to (0,200,0) (0.6s)
			auto hideGroup = compositor.CreateAnimationGroup();

			auto opacityHide = compositor.CreateScalarKeyFrameAnimation();
			opacityHide.InsertKeyFrame(0.0f, 1.0f);
			opacityHide.InsertKeyFrame(1.0f, 0.0f);
			opacityHide.Duration(std::chrono::milliseconds(350));
			opacityHide.Target(L"Opacity");
			hideGroup.Add(opacityHide);

			auto translationHide = compositor.CreateVector3KeyFrameAnimation();
			translationHide.InsertKeyFrame(0.0f, { 0.0f, 0.0f, 0.0f });
			translationHide.InsertKeyFrame(1.0f, { 0.0f, 200.0f, 0.0f });
			translationHide.Duration(std::chrono::milliseconds(600));
			translationHide.Target(L"Translation");
			hideGroup.Add(translationHide);

			Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::SetImplicitHideAnimation(textPanel, hideGroup);
		}

		SetAccessibleName();
	}

	void HeaderTile::OnIsSelectedChanged(bool /*oldValue*/, bool newValue)
	{
		auto visual = Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(*this);
		auto compositor = visual.Compositor();

		if (newValue)
		{
			Canvas::SetZIndex(*this, 10);
			VisualStateManager::GoToState(*this, L"Selected", true);

			auto scaleAnim = compositor.CreateVector3KeyFrameAnimation();
			scaleAnim.InsertKeyFrame(1.0f, { 1.0f, 1.0f, 1.0f });
			scaleAnim.Duration(std::chrono::milliseconds(600));
			visual.StartAnimation(L"Scale", scaleAnim);

			// Elevate shadow
			auto translationAnim = compositor.CreateVector3KeyFrameAnimation();
			translationAnim.InsertKeyFrame(1.0f, { 0.0f, 0.0f, 32.0f });
			translationAnim.Duration(std::chrono::milliseconds(600));
			visual.StartAnimation(L"Translation", translationAnim);
		}
		else
		{
			VisualStateManager::GoToState(*this, L"NotSelected", true);

			auto scaleAnim = compositor.CreateVector3KeyFrameAnimation();
			scaleAnim.InsertKeyFrame(1.0f, { 0.8f, 0.8f, 1.0f });
			scaleAnim.Duration(std::chrono::milliseconds(350));

			// Lower shadow
			auto translationAnim = compositor.CreateVector3KeyFrameAnimation();
			translationAnim.InsertKeyFrame(1.0f, { 0.0f, 0.0f, 0.0f });
			translationAnim.Duration(std::chrono::milliseconds(350));

			// Batch animations to defer ZIndex reset until deselect animation completes
			auto batch = compositor.CreateScopedBatch(winrt::Microsoft::UI::Composition::CompositionBatchTypes::Animation);
			visual.StartAnimation(L"Scale", scaleAnim);
			visual.StartAnimation(L"Translation", translationAnim);
			batch.End();

			auto weakThis = get_weak();
			batch.Completed([weakThis](auto&&, auto&&)
			{
				if (auto strongThis = weakThis.get())
				{
					if (auto element = strongThis.try_as<UIElement>())
					{
						Canvas::SetZIndex(element, 0);
					}
				}
			});
		}
	}

	void HeaderTile::SetAccessibleName()
	{
		if (!Header().empty())
		{
			Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(*this, Header());
		}
	}

	// ---- DependencyProperty registrations ----

	DependencyProperty HeaderTile::ImageUrlProperty()
	{
		if (!s_imageUrlProperty)
		{
			s_imageUrlProperty = DependencyProperty::Register(
				L"ImageUrl",
				winrt::xaml_typename<winrt::hstring>(),
				winrt::xaml_typename<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>(),
				PropertyMetadata{ nullptr });
		}
		return s_imageUrlProperty;
	}

	hstring HeaderTile::ImageUrl() const { return unbox_value_or<hstring>(GetValue(ImageUrlProperty()), L""); }
	void HeaderTile::ImageUrl(hstring const& value) { SetValue(ImageUrlProperty(), box_value(value)); }

	DependencyProperty HeaderTile::HeaderProperty()
	{
		if (!s_headerProperty)
		{
			s_headerProperty = DependencyProperty::Register(
				L"Header",
				winrt::xaml_typename<winrt::hstring>(),
				winrt::xaml_typename<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>(),
				PropertyMetadata{ nullptr,
					PropertyChangedCallback{ [](DependencyObject const& d, DependencyPropertyChangedEventArgs const&)
					{
						if (auto tile = d.try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
						{
							get_self<HeaderTile>(tile)->SetAccessibleName();
						}
					}}
				});
		}
		return s_headerProperty;
	}

	hstring HeaderTile::Header() const { return unbox_value_or<hstring>(GetValue(HeaderProperty()), L""); }
	void HeaderTile::Header(hstring const& value) { SetValue(HeaderProperty(), box_value(value)); }

	DependencyProperty HeaderTile::DescriptionProperty()
	{
		if (!s_descriptionProperty)
		{
			s_descriptionProperty = DependencyProperty::Register(
				L"Description",
				winrt::xaml_typename<winrt::hstring>(),
				winrt::xaml_typename<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>(),
				PropertyMetadata{ nullptr });
		}
		return s_descriptionProperty;
	}

	hstring HeaderTile::Description() const { return unbox_value_or<hstring>(GetValue(DescriptionProperty()), L""); }
	void HeaderTile::Description(hstring const& value) { SetValue(DescriptionProperty(), box_value(value)); }

	DependencyProperty HeaderTile::FeatureIDProperty()
	{
		if (!s_featureIDProperty)
		{
			s_featureIDProperty = DependencyProperty::Register(
				L"FeatureID",
				winrt::xaml_typename<winrt::hstring>(),
				winrt::xaml_typename<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>(),
				PropertyMetadata{ nullptr });
		}
		return s_featureIDProperty;
	}

	hstring HeaderTile::FeatureID() const { return unbox_value_or<hstring>(GetValue(FeatureIDProperty()), L""); }
	void HeaderTile::FeatureID(hstring const& value) { SetValue(FeatureIDProperty(), box_value(value)); }

	DependencyProperty HeaderTile::IsSelectedProperty()
	{
		if (!s_isSelectedProperty)
		{
			s_isSelectedProperty = DependencyProperty::Register(
				L"IsSelected",
				winrt::xaml_typename<bool>(),
				winrt::xaml_typename<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>(),
				PropertyMetadata{ box_value(false),
					PropertyChangedCallback{ [](DependencyObject const& d, DependencyPropertyChangedEventArgs const& e)
					{
						if (auto tile = d.try_as<winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile>())
						{
							get_self<HeaderTile>(tile)->OnIsSelectedChanged(
								unbox_value<bool>(e.OldValue()),
								unbox_value<bool>(e.NewValue()));
						}
					}}
				});
		}
		return s_isSelectedProperty;
	}

	bool HeaderTile::IsSelected() const { return unbox_value_or<bool>(GetValue(IsSelectedProperty()), false); }
	void HeaderTile::IsSelected(bool value) { SetValue(IsSelectedProperty(), box_value(value)); }
}
