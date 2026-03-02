#include "pch.h"
#include "UI/Xaml/Media/Shadows/AttachedCardShadow.h"

#if __has_include("UI/Xaml/Media/Shadows/AttachedCardShadow.g.cpp")
#include "UI/Xaml/Media/Shadows/AttachedCardShadow.g.cpp"
#endif

#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.Graphics.Canvas.Geometry.h>
#include <winrt/Windows.Foundation.Numerics.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Hosting;
using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Windows::Foundation::Numerics;

namespace winrt::OpenNet::UI::Xaml::Media::Shadows::implementation
{
	// ---- DependencyProperty registrations ----

	DependencyProperty AttachedCardShadow::s_colorProperty =
		DependencyProperty::Register(L"Color", xaml_typename<winrt::Windows::UI::Color>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Shadows::AttachedCardShadow>(),
			PropertyMetadata{ box_value(winrt::Windows::UI::Color{ 255, 0, 0, 0 }) });

	DependencyProperty AttachedCardShadow::s_opacityProperty =
		DependencyProperty::Register(L"Opacity", xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Shadows::AttachedCardShadow>(),
			PropertyMetadata{ box_value(1.0) });

	DependencyProperty AttachedCardShadow::s_blurRadiusProperty =
		DependencyProperty::Register(L"BlurRadius", xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Shadows::AttachedCardShadow>(),
			PropertyMetadata{ box_value(12.0) });

	DependencyProperty AttachedCardShadow::s_offsetXProperty =
		DependencyProperty::Register(L"OffsetX", xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Shadows::AttachedCardShadow>(),
			PropertyMetadata{ box_value(0.0) });

	DependencyProperty AttachedCardShadow::s_offsetYProperty =
		DependencyProperty::Register(L"OffsetY", xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Shadows::AttachedCardShadow>(),
			PropertyMetadata{ box_value(4.0) });

	DependencyProperty AttachedCardShadow::s_cornerRadiusProperty =
		DependencyProperty::Register(L"CornerRadius", xaml_typename<double>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Shadows::AttachedCardShadow>(),
			PropertyMetadata{ box_value(4.0) });

	DependencyProperty AttachedCardShadow::s_innerContentClipModeProperty =
		DependencyProperty::Register(L"InnerContentClipMode",
			xaml_typename<OpenNet::UI::Xaml::Media::InnerContentClipMode>(),
			xaml_typename<OpenNet::UI::Xaml::Media::Shadows::AttachedCardShadow>(),
			PropertyMetadata{ box_value(OpenNet::UI::Xaml::Media::InnerContentClipMode::CompositionGeometricClip) });

	// ---- Property accessors ----

	winrt::Windows::UI::Color AttachedCardShadow::Color() const { return unbox_value<winrt::Windows::UI::Color>(GetValue(s_colorProperty)); }
	void AttachedCardShadow::Color(winrt::Windows::UI::Color value) { SetValue(s_colorProperty, box_value(value)); if (m_dropShadow) UpdateShadow(); }
	DependencyProperty AttachedCardShadow::ColorProperty() { return s_colorProperty; }

	double AttachedCardShadow::Opacity() const { return unbox_value<double>(GetValue(s_opacityProperty)); }
	void AttachedCardShadow::Opacity(double value) { SetValue(s_opacityProperty, box_value(value)); if (m_dropShadow) UpdateShadow(); }
	DependencyProperty AttachedCardShadow::OpacityProperty() { return s_opacityProperty; }

	double AttachedCardShadow::BlurRadius() const { return unbox_value<double>(GetValue(s_blurRadiusProperty)); }
	void AttachedCardShadow::BlurRadius(double value) { SetValue(s_blurRadiusProperty, box_value(value)); if (m_dropShadow) UpdateShadow(); }
	DependencyProperty AttachedCardShadow::BlurRadiusProperty() { return s_blurRadiusProperty; }

	double AttachedCardShadow::OffsetX() const { return unbox_value<double>(GetValue(s_offsetXProperty)); }
	void AttachedCardShadow::OffsetX(double value) { SetValue(s_offsetXProperty, box_value(value)); if (m_dropShadow) UpdateShadow(); }
	DependencyProperty AttachedCardShadow::OffsetXProperty() { return s_offsetXProperty; }

	double AttachedCardShadow::OffsetY() const { return unbox_value<double>(GetValue(s_offsetYProperty)); }
	void AttachedCardShadow::OffsetY(double value) { SetValue(s_offsetYProperty, box_value(value)); if (m_dropShadow) UpdateShadow(); }
	DependencyProperty AttachedCardShadow::OffsetYProperty() { return s_offsetYProperty; }

	double AttachedCardShadow::CornerRadius() const { return unbox_value<double>(GetValue(s_cornerRadiusProperty)); }
	void AttachedCardShadow::CornerRadius(double value)
	{
		SetValue(s_cornerRadiusProperty, box_value(value));
		if (m_dropShadow)
		{
			UpdateShadowMask();
			UpdateShadowClip();
		}
	}
	DependencyProperty AttachedCardShadow::CornerRadiusProperty() { return s_cornerRadiusProperty; }

	winrt::OpenNet::UI::Xaml::Media::InnerContentClipMode AttachedCardShadow::InnerContentClipMode() const
	{
		return unbox_value<OpenNet::UI::Xaml::Media::InnerContentClipMode>(GetValue(s_innerContentClipModeProperty));
	}
	void AttachedCardShadow::InnerContentClipMode(winrt::OpenNet::UI::Xaml::Media::InnerContentClipMode value)
	{
		SetValue(s_innerContentClipModeProperty, box_value(value));
		if (m_dropShadow) { UpdateShadowClip(); }
	}
	DependencyProperty AttachedCardShadow::InnerContentClipModeProperty() { return s_innerContentClipModeProperty; }

	// ---- Core implementation ----

	void AttachedCardShadow::AttachShadow(winrt::Microsoft::UI::Xaml::FrameworkElement const& element)
	{
		DetachShadow();

		m_element = element;
		auto visual = ElementCompositionPreview::GetElementVisual(element);
		auto compositor = visual.Compositor();

		// Create shadow visual
		m_shadowVisual = compositor.CreateSpriteVisual();
		m_dropShadow = compositor.CreateDropShadow();
		m_shadowVisual.Shadow(m_dropShadow);
		m_shadowVisual.RelativeSizeAdjustment(float2{ 1.0f, 1.0f });

		// Create rounded rectangle geometry for shadow mask
		m_roundedGeometry = compositor.CreateRoundedRectangleGeometry();
		m_roundedGeometry.CornerRadius(float2{ static_cast<float>(CornerRadius()) });

		// Create shape for shadow mask
		m_spriteShape = compositor.CreateSpriteShape(m_roundedGeometry);
		m_spriteShape.FillBrush(compositor.CreateColorBrush(winrt::Windows::UI::Colors::Black()));

		m_shapeVisual = compositor.CreateShapeVisual();
		m_shapeVisual.Shapes().Append(m_spriteShape);

		// Create visual surface for mask rendering
		m_visualSurface = compositor.CreateVisualSurface();
		m_visualSurface.SourceVisual(m_shapeVisual);

		m_surfaceBrush = compositor.CreateSurfaceBrush(m_visualSurface);

		m_dropShadow.Mask(m_surfaceBrush);

		UpdateShadow();
		UpdateShadowClip();

		// Set as child visual
		ElementCompositionPreview::SetElementChildVisual(element, m_shadowVisual);

		// Listen for size changes
		m_sizeChangedRevoker = element.SizeChanged(winrt::auto_revoke,
			{ this, &AttachedCardShadow::OnSizeChanged });
	}

	void AttachedCardShadow::DetachShadow()
	{
		m_sizeChangedRevoker.revoke();

		if (m_element)
		{
			ElementCompositionPreview::SetElementChildVisual(m_element, nullptr);
		}

		if (m_shadowVisual)
		{
			m_shadowVisual.Close();
			m_shadowVisual = nullptr;
		}
		m_dropShadow = nullptr;
		m_roundedGeometry = nullptr;
		m_shapeVisual = nullptr;
		m_spriteShape = nullptr;
		m_visualSurface = nullptr;
		m_surfaceBrush = nullptr;
		m_geometricClip = nullptr;
		m_pathGeometry = nullptr;
		m_element = nullptr;
	}

	void AttachedCardShadow::UpdateShadow()
	{
		if (!m_dropShadow) return;

		m_dropShadow.Color(Color());
		m_dropShadow.Opacity(static_cast<float>(Opacity()));
		m_dropShadow.BlurRadius(static_cast<float>(BlurRadius()));
		m_dropShadow.Offset(float3{
			static_cast<float>(OffsetX()),
			static_cast<float>(OffsetY()),
			0.0f });
	}

	void AttachedCardShadow::UpdateShadowMask()
	{
		if (!m_element || !m_roundedGeometry) return;

		float2 size{
			static_cast<float>(m_element.ActualWidth()),
			static_cast<float>(m_element.ActualHeight())
		};

		m_roundedGeometry.CornerRadius(float2{ static_cast<float>(CornerRadius()) });
		m_roundedGeometry.Size(size);

		if (m_shapeVisual)
		{
			m_shapeVisual.Size(size);
		}
		if (m_visualSurface)
		{
			m_visualSurface.SourceSize(size);
		}
	}

	void AttachedCardShadow::UpdateShadowClip()
	{
		if (!m_element || !m_shadowVisual) return;

		auto clipMode = InnerContentClipMode();
		auto compositor = m_shadowVisual.Compositor();

		if (clipMode == OpenNet::UI::Xaml::Media::InnerContentClipMode::CompositionGeometricClip)
		{
			// Use Win2D to create a geometric clip that cuts out the inner portion
			try
			{
				if (!m_pathGeometry)
				{
					m_pathGeometry = compositor.CreatePathGeometry();
				}
				if (!m_geometricClip)
				{
					m_geometricClip = compositor.CreateGeometricClip(m_pathGeometry);
				}

				float width = static_cast<float>(m_element.ActualWidth());
				float height = static_cast<float>(m_element.ActualHeight());
				float cornerRadius = static_cast<float>(CornerRadius());

				// Create a rounded rectangle outline using Win2D CanvasGeometry
				auto canvasRect = winrt::Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreateRoundedRectangle(
					nullptr,
					-MaxBlurRadius / 2,
					-MaxBlurRadius / 2,
					width + MaxBlurRadius,
					height + MaxBlurRadius,
					(MaxBlurRadius / 2) + cornerRadius,
					(MaxBlurRadius / 2) + cornerRadius);

				auto canvasStroke = canvasRect.Stroke(MaxBlurRadius);

				m_pathGeometry.Path(CompositionPath(canvasStroke));
				m_shadowVisual.Clip(m_geometricClip);
			}
			catch (...)
			{
				m_shadowVisual.Clip(nullptr);
			}
		}
		else
		{
			m_shadowVisual.Clip(nullptr);
			m_geometricClip = nullptr;
			m_pathGeometry = nullptr;
		}
	}

	void AttachedCardShadow::OnSizeChanged(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const&)
	{
		UpdateShadowMask();
		UpdateShadowClip();
	}
}
