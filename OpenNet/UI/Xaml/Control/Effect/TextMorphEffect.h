#pragma once

#include "UI/Xaml/Control/Effect/TextMorphEffect.g.h"

import winrt.Microsoft.Graphics.Canvas.Effects;
import winrt.Microsoft.Graphics.Canvas.Text;
import winrt.Windows.UI;
import winrt.Windows.UI.Text;
import winrt.Microsoft.Graphics.Canvas;
import winrt.Microsoft.Graphics.Canvas.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Media.Animation;

namespace winrt::OpenNet::UI::Xaml::Control::Effect::implementation
{
	// Helper structure to track individual morphing text items
	struct TextMorphItem
	{
		winrt::hstring Text;

		// Timeline parameters
		struct Timeline
		{
			double from;
			double to;
			double duration;
			winrt::Windows::Foundation::TimeSpan beginTime;
			bool autoReverse;

			Timeline(double from_, double to_, double duration_,
					 winrt::Windows::Foundation::TimeSpan beginTime_,
					 bool autoReverse_)
				: from(from_), to(to_), duration(duration_), beginTime(beginTime_),
				autoReverse(autoReverse_)
			{
			}

			double GetCurrentProgress(winrt::Windows::Foundation::TimeSpan currentTime) const;
		};

		Timeline timeline;

		TextMorphItem(const winrt::hstring& text, const Timeline& tl)
			: Text(text), timeline(tl)
		{
		}
	};

	struct TextMorphEffect : TextMorphEffectT<TextMorphEffect>
	{
		TextMorphEffect();

		// DependencyProperty accessors
		static winrt::Microsoft::UI::Xaml::DependencyProperty EasingProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty TimeLineFromProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty TimeLineToProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty DurationProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty BeginTimeProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty EffectFontSizeProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty EffectFontWeightProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty TextProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty DelimiterProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty DirectionProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty EffectVerticalAlignmentProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty EffectHorizontalAlignmentProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty AutoReverseProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty BlurAmountProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty ColorBrushProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty MorphSpeedProperty();

		// Property accessors
		winrt::Microsoft::UI::Xaml::Media::Animation::EasingMode Easing();
		void Easing(winrt::Microsoft::UI::Xaml::Media::Animation::EasingMode const& value);

		double TimeLineFrom();
		void TimeLineFrom(double value);

		double TimeLineTo();
		void TimeLineTo(double value);

		double Duration();
		void Duration(double value);

		winrt::Windows::Foundation::TimeSpan BeginTime();
		void BeginTime(winrt::Windows::Foundation::TimeSpan const& value);

		double EffectFontSize();
		void EffectFontSize(double value);

		winrt::Windows::UI::Text::FontWeight EffectFontWeight();
		void EffectFontWeight(winrt::Windows::UI::Text::FontWeight const& value);

		winrt::hstring Text();
		void Text(winrt::hstring const& value);

		winrt::hstring Delimiter();
		void Delimiter(winrt::hstring const& value);

		winrt::Microsoft::Graphics::Canvas::Text::CanvasTextDirection Direction();
		void Direction(winrt::Microsoft::Graphics::Canvas::Text::CanvasTextDirection const& value);

		winrt::Microsoft::Graphics::Canvas::Text::CanvasVerticalAlignment EffectVerticalAlignment();
		void EffectVerticalAlignment(winrt::Microsoft::Graphics::Canvas::Text::CanvasVerticalAlignment const& value);

		winrt::Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment EffectHorizontalAlignment();
		void EffectHorizontalAlignment(winrt::Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment const& value);

		bool AutoReverse();
		void AutoReverse(bool value);

		double BlurAmount();
		void BlurAmount(double value);

		winrt::Windows::UI::Color ColorBrush();
		void ColorBrush(winrt::Windows::UI::Color const& value);

		int32_t MorphSpeed();
		void MorphSpeed(int32_t value);

		// Template
		void OnApplyTemplate();

	private:
		// Canvas control reference
		winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl m_canvas{ nullptr };

		// Animation and rendering state
		std::vector<TextMorphItem> m_morphItems;
		winrt::Microsoft::Graphics::Canvas::Text::CanvasTextFormat m_textFormat{ nullptr };
		std::vector<winrt::hstring> m_texts;

		// Win2D effects
		winrt::Microsoft::Graphics::Canvas::Effects::GaussianBlurEffect m_blurEffect{ nullptr };
		winrt::Microsoft::Graphics::Canvas::Effects::ColorMatrixEffect m_colorMatrixEffect{ nullptr };

		// Rendering state
		winrt::Windows::Foundation::Numerics::float2 m_centerPoint{};

		// Event tokens
		winrt::event_token m_drawToken;
		winrt::event_token m_createResourcesToken;
		winrt::event_token m_sizeChangedToken;

		// Helper methods
		static winrt::Windows::Foundation::IInspectable CreateBeginTimeDefaultValue();
		static winrt::Windows::Foundation::IInspectable CreateFontWeightDefaultValue();

		static void OnPropertyChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

		static void OnTextChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

		static void OnResourcePropertyValueChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

		static void OnAnimationChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

		void UpdateTextMorph();

		// Event handlers
		void OnDraw(winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& sender,
					winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasDrawEventArgs const& args);
		void OnCreateResources(winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& sender,
							   winrt::Windows::Foundation::IInspectable const& args);
		void OnCanvasSizeChanged(winrt::Windows::Foundation::IInspectable const& sender,
								 winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::Effect::factory_implementation
{
	struct TextMorphEffect : TextMorphEffectT<TextMorphEffect, implementation::TextMorphEffect>
	{
	};
}
