#pragma once

#include "UI/Xaml/Media/Shadows/AttachedCardShadow.g.h"

namespace winrt::OpenNet::UI::Xaml::Media::Shadows::implementation
{
	struct AttachedCardShadow : AttachedCardShadowT<AttachedCardShadow>
	{
		AttachedCardShadow() = default;

		winrt::Windows::UI::Color Color() const;
		void Color(winrt::Windows::UI::Color value);
		static Microsoft::UI::Xaml::DependencyProperty ColorProperty();

		double Opacity() const;
		void Opacity(double value);
		static Microsoft::UI::Xaml::DependencyProperty OpacityProperty();

		double BlurRadius() const;
		void BlurRadius(double value);
		static Microsoft::UI::Xaml::DependencyProperty BlurRadiusProperty();

		double OffsetX() const;
		void OffsetX(double value);
		static Microsoft::UI::Xaml::DependencyProperty OffsetXProperty();

		double OffsetY() const;
		void OffsetY(double value);
		static Microsoft::UI::Xaml::DependencyProperty OffsetYProperty();

		double CornerRadius() const;
		void CornerRadius(double value);
		static Microsoft::UI::Xaml::DependencyProperty CornerRadiusProperty();

		winrt::OpenNet::UI::Xaml::Media::InnerContentClipMode InnerContentClipMode() const;
		void InnerContentClipMode(winrt::OpenNet::UI::Xaml::Media::InnerContentClipMode value);
		static Microsoft::UI::Xaml::DependencyProperty InnerContentClipModeProperty();

		void AttachShadow(winrt::Microsoft::UI::Xaml::FrameworkElement const& element);
		void DetachShadow();

	private:
		static constexpr float MaxBlurRadius = 72.0f;

		void UpdateShadow();
		void UpdateShadowClip();
		void UpdateShadowMask();
		void OnSizeChanged(winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);

		winrt::Microsoft::UI::Xaml::FrameworkElement m_element{ nullptr };
		winrt::Microsoft::UI::Composition::SpriteVisual m_shadowVisual{ nullptr };
		winrt::Microsoft::UI::Composition::DropShadow m_dropShadow{ nullptr };
		winrt::Microsoft::UI::Composition::CompositionRoundedRectangleGeometry m_roundedGeometry{ nullptr };
		winrt::Microsoft::UI::Composition::ShapeVisual m_shapeVisual{ nullptr };
		winrt::Microsoft::UI::Composition::CompositionSpriteShape m_spriteShape{ nullptr };
		winrt::Microsoft::UI::Composition::CompositionVisualSurface m_visualSurface{ nullptr };
		winrt::Microsoft::UI::Composition::CompositionSurfaceBrush m_surfaceBrush{ nullptr };
		winrt::Microsoft::UI::Composition::CompositionGeometricClip m_geometricClip{ nullptr };
		winrt::Microsoft::UI::Composition::CompositionPathGeometry m_pathGeometry{ nullptr };
		winrt::Microsoft::UI::Xaml::FrameworkElement::SizeChanged_revoker m_sizeChangedRevoker;

		static Microsoft::UI::Xaml::DependencyProperty s_colorProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_opacityProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_blurRadiusProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_offsetXProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_offsetYProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_cornerRadiusProperty;
		static Microsoft::UI::Xaml::DependencyProperty s_innerContentClipModeProperty;
	};
}

namespace winrt::OpenNet::UI::Xaml::Media::Shadows::factory_implementation
{
	struct AttachedCardShadow : AttachedCardShadowT<AttachedCardShadow, implementation::AttachedCardShadow> {};
}
