#include "XamlWorkaround.h"
#include "TextMorphEffect.h"
#if __has_include("UI/Xaml/Control/Effect/TextMorphEffect.g.cpp")
#include "UI/Xaml/Control/Effect/TextMorphEffect.g.cpp"
#endif
#undef DrawText
import winrt.Microsoft.Graphics.Canvas;
import winrt.Microsoft.UI.Text;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Microsoft::Graphics::Canvas::UI::Xaml;
using namespace winrt::Microsoft::Graphics::Canvas::Effects;
using namespace winrt::Microsoft::Graphics::Canvas::Text;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::Foundation;

namespace winrt::OpenNet::UI::Xaml::Control::Effect::implementation
{
	// Timeline helper implementation
	double TextMorphItem::Timeline::GetCurrentProgress(TimeSpan currentTime) const
	{
		double currentSeconds = currentTime.count() / 10000000.0;
		double beginSeconds = beginTime.count() / 10000000.0;
		double elapsedSeconds = currentSeconds - beginSeconds;

		if (elapsedSeconds < 0)
		{
			return 0.0;
		}

		double totalDuration = duration;
		if (autoReverse)
		{
			totalDuration *= 2;
		}

		double progress = std::fmod(elapsedSeconds, totalDuration);

		if (autoReverse && progress > duration)
		{
			progress = totalDuration - progress;
		}

		progress = progress / duration;
		progress = std::min(1.0, std::max(0.0, progress));

		// CircleEase EaseInOut
		double easedProgress = progress;
		if (progress <= 0.5)
		{
			easedProgress = (1.0 - std::sqrt(1.0 - 4.0 * progress * progress)) / 2.0;
		}
		else
		{
			easedProgress = (std::sqrt(1.0 - (2.0 * progress - 2.0) * (2.0 * progress - 2.0)) + 1.0) / 2.0;
		}

		return from + (to - from) * easedProgress;
	}

	// TextMorphEffect implementation
	TextMorphEffect::TextMorphEffect()
	{
		DefaultStyleKey(winrt::box_value(winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>()));
	}

	// ThemeResource, Binding, and other XAML value expressions require the
	// dependency properties to exist before the XAML property system assigns
	// the expression. Do not rely on the wrapper accessors to register lazily.
	void TextMorphEffect::EnsureDependencyProperties()
	{
		if (s_colorBrushProperty)
		{
			return;
		}
		s_colorBrushProperty = DependencyProperty::Register(
			L"ColorBrush",
			winrt::xaml_typename<Windows::UI::Color>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(Windows::UI::Colors::White()), PropertyChangedCallback{ &TextMorphEffect::OnResourcePropertyValueChanged } });
	}

	// DependencyProperty definitions
	DependencyProperty TextMorphEffect::EasingProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"Easing",
			winrt::xaml_typename<EasingMode>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(EasingMode::EaseInOut), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::TimeLineFromProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"TimeLineFrom",
			winrt::xaml_typename<double>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::TimeLineToProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"TimeLineTo",
			winrt::xaml_typename<double>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::DurationProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"Duration",
			winrt::xaml_typename<double>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(1.0), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::BeginTimeProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"BeginTime",
			winrt::xaml_typename<TimeSpan>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ CreateBeginTimeDefaultValue(), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::EffectFontSizeProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"EffectFontSize",
			winrt::xaml_typename<double>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(100.0), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::EffectFontWeightProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"EffectFontWeight",
			winrt::xaml_typename<winrt::Windows::UI::Text::FontWeight>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ CreateFontWeightDefaultValue(), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::TextProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"Text",
			winrt::xaml_typename<hstring>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(L""), PropertyChangedCallback{ &TextMorphEffect::OnTextChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::DelimiterProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"Delimiter",
			winrt::xaml_typename<hstring>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(L","), PropertyChangedCallback{ &TextMorphEffect::OnTextChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::DirectionProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"Direction",
			winrt::xaml_typename<CanvasTextDirection>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(CanvasTextDirection::LeftToRightThenTopToBottom), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::EffectVerticalAlignmentProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"EffectVerticalAlignment",
			winrt::xaml_typename<CanvasVerticalAlignment>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(CanvasVerticalAlignment::Center), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::EffectHorizontalAlignmentProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"EffectHorizontalAlignment",
			winrt::xaml_typename<CanvasHorizontalAlignment>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(CanvasHorizontalAlignment::Center), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::AutoReverseProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"AutoReverse",
			winrt::xaml_typename<bool>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(true), PropertyChangedCallback{ &TextMorphEffect::OnAnimationChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::BlurAmountProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"BlurAmount",
			winrt::xaml_typename<double>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ &TextMorphEffect::OnResourcePropertyValueChanged } });
		return s_property;
	}

	DependencyProperty TextMorphEffect::ColorBrushProperty()
	{
		return s_colorBrushProperty;
	}

	DependencyProperty TextMorphEffect::MorphSpeedProperty()
	{
		static DependencyProperty s_property = DependencyProperty::Register(
			L"MorphSpeed",
			winrt::xaml_typename<int32_t>(),
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Effect::TextMorphEffect>(),
			PropertyMetadata{ box_value(2000), PropertyChangedCallback{ &TextMorphEffect::OnPropertyChanged } });
		return s_property;
	}

	// Property accessors
	EasingMode TextMorphEffect::Easing()
	{
		return winrt::unbox_value<EasingMode>(GetValue(EasingProperty()));
	}

	void TextMorphEffect::Easing(EasingMode const& value)
	{
		SetValue(EasingProperty(), box_value(value));
	}

	double TextMorphEffect::TimeLineFrom()
	{
		return winrt::unbox_value<double>(GetValue(TimeLineFromProperty()));
	}

	void TextMorphEffect::TimeLineFrom(double value)
	{
		SetValue(TimeLineFromProperty(), box_value(value));
	}

	double TextMorphEffect::TimeLineTo()
	{
		return winrt::unbox_value<double>(GetValue(TimeLineToProperty()));
	}

	void TextMorphEffect::TimeLineTo(double value)
	{
		SetValue(TimeLineToProperty(), box_value(value));
	}

	double TextMorphEffect::Duration()
	{
		return winrt::unbox_value<double>(GetValue(DurationProperty()));
	}

	void TextMorphEffect::Duration(double value)
	{
		SetValue(DurationProperty(), box_value(value));
	}

	TimeSpan TextMorphEffect::BeginTime()
	{
		return winrt::unbox_value<TimeSpan>(GetValue(BeginTimeProperty()));
	}

	void TextMorphEffect::BeginTime(TimeSpan const& value)
	{
		SetValue(BeginTimeProperty(), box_value(value));
	}

	double TextMorphEffect::EffectFontSize()
	{
		return winrt::unbox_value<double>(GetValue(EffectFontSizeProperty()));
	}

	void TextMorphEffect::EffectFontSize(double value)
	{
		SetValue(EffectFontSizeProperty(), box_value(value));
	}

	winrt::Windows::UI::Text::FontWeight TextMorphEffect::EffectFontWeight()
	{
		return winrt::unbox_value<winrt::Windows::UI::Text::FontWeight>(GetValue(EffectFontWeightProperty()));
	}

	void TextMorphEffect::EffectFontWeight(winrt::Windows::UI::Text::FontWeight const& value)
	{
		SetValue(EffectFontWeightProperty(), box_value(value));
	}

	hstring TextMorphEffect::Text()
	{
		return winrt::unbox_value_or<hstring>(GetValue(TextProperty()), L"");
	}

	void TextMorphEffect::Text(hstring const& value)
	{
		SetValue(TextProperty(), box_value(value));
	}

	hstring TextMorphEffect::Delimiter()
	{
		return winrt::unbox_value_or<hstring>(GetValue(DelimiterProperty()), L",");
	}

	void TextMorphEffect::Delimiter(hstring const& value)
	{
		SetValue(DelimiterProperty(), box_value(value));
	}

	CanvasTextDirection TextMorphEffect::Direction()
	{
		return winrt::unbox_value<CanvasTextDirection>(GetValue(DirectionProperty()));
	}

	void TextMorphEffect::Direction(CanvasTextDirection const& value)
	{
		SetValue(DirectionProperty(), box_value(value));
	}

	CanvasVerticalAlignment TextMorphEffect::EffectVerticalAlignment()
	{
		return winrt::unbox_value<CanvasVerticalAlignment>(GetValue(EffectVerticalAlignmentProperty()));
	}

	void TextMorphEffect::EffectVerticalAlignment(CanvasVerticalAlignment const& value)
	{
		SetValue(EffectVerticalAlignmentProperty(), box_value(value));
	}

	CanvasHorizontalAlignment TextMorphEffect::EffectHorizontalAlignment()
	{
		return winrt::unbox_value<CanvasHorizontalAlignment>(GetValue(EffectHorizontalAlignmentProperty()));
	}

	void TextMorphEffect::EffectHorizontalAlignment(CanvasHorizontalAlignment const& value)
	{
		SetValue(EffectHorizontalAlignmentProperty(), box_value(value));
	}

	bool TextMorphEffect::AutoReverse()
	{
		return winrt::unbox_value<bool>(GetValue(AutoReverseProperty()));
	}

	void TextMorphEffect::AutoReverse(bool value)
	{
		SetValue(AutoReverseProperty(), box_value(value));
	}

	double TextMorphEffect::BlurAmount()
	{
		return winrt::unbox_value<double>(GetValue(BlurAmountProperty()));
	}

	void TextMorphEffect::BlurAmount(double value)
	{
		SetValue(BlurAmountProperty(), box_value(value));
	}

	Color TextMorphEffect::ColorBrush()
	{
		return winrt::unbox_value<Color>(GetValue(ColorBrushProperty()));
	}

	void TextMorphEffect::ColorBrush(Color const& value)
	{
		SetValue(ColorBrushProperty(), box_value(value));
	}

	int32_t TextMorphEffect::MorphSpeed()
	{
		return winrt::unbox_value<int32_t>(GetValue(MorphSpeedProperty()));
	}

	void TextMorphEffect::MorphSpeed(int32_t value)
	{
		SetValue(MorphSpeedProperty(), box_value(value));
	}

	// Static callback helpers
	IInspectable TextMorphEffect::CreateBeginTimeDefaultValue()
	{
		return box_value(TimeSpan{ 0 });
	}

	IInspectable TextMorphEffect::CreateFontWeightDefaultValue()
	{
		return box_value(winrt::Microsoft::UI::Text::FontWeights::Bold());
	}

	void TextMorphEffect::OnPropertyChanged(DependencyObject const& dependencyObject, DependencyPropertyChangedEventArgs const& /*e*/)
	{
		auto control = dependencyObject.as<TextMorphEffect>();
		if (control && control->m_canvas)
		{
			double blurAmount = control->BlurAmount();
			if (std::isnan(blurAmount))
			{
				control->BlurAmount(0.0);
			}
			control->m_canvas.Invalidate();
		}
	}

	void TextMorphEffect::OnTextChanged(DependencyObject const& dependencyObject, DependencyPropertyChangedEventArgs const& /*e*/)
	{
		auto control = dependencyObject.as<TextMorphEffect>();
		if (control && control->m_canvas)
		{
			control->UpdateTextMorph();
			control->m_canvas.Invalidate();
		}
	}

	void TextMorphEffect::OnResourcePropertyValueChanged(DependencyObject const& dependencyObject, DependencyPropertyChangedEventArgs const& /*e*/)
	{
		auto control = dependencyObject.as<TextMorphEffect>();
		if (control && control->m_canvas)
		{
			control->m_canvas.CreateResources(control->m_createResourcesToken);
			control->m_canvas.Invalidate();
		}
	}

	void TextMorphEffect::OnAnimationChanged(DependencyObject const& dependencyObject, DependencyPropertyChangedEventArgs const& /*e*/)
	{
		auto control = dependencyObject.as<TextMorphEffect>();
		if (control && control->m_canvas)
		{
			control->UpdateTextMorph();
			control->m_canvas.Invalidate();
		}
	}

	void TextMorphEffect::OnApplyTemplate()
	{
		// Unsubscribe from old canvas if it exists
		if (m_canvas)
		{
			m_canvas.Draw(m_drawToken);
			m_canvas.CreateResources(m_createResourcesToken);
			m_canvas.SizeChanged(m_sizeChangedToken);
		}

		// Get the new canvas from template
		m_canvas = GetTemplateChild(L"PART_Canvas").as<CanvasControl>();

		if (m_canvas)
		{
			m_drawToken = m_canvas.Draw({ this, &TextMorphEffect::OnDraw });
			m_createResourcesToken = m_canvas.CreateResources({ this, &TextMorphEffect::OnCreateResources });
			m_sizeChangedToken = m_canvas.SizeChanged({ this, &TextMorphEffect::OnCanvasSizeChanged });
		}

		UpdateTextMorph();
	}

	void TextMorphEffect::UpdateTextMorph()
	{
		hstring text = Text();
		if (text.empty())
		{
			return;
		}

		// Convert hstring to std::wstring for string operations
		std::wstring textStr{ text };
		std::wstring delimiterStr{ Delimiter() };

		m_texts.clear();
		size_t start = 0;

		while (start < textStr.size())
		{
			size_t end = textStr.find(delimiterStr, start);
			if (end == std::string::npos)
			{
				end = textStr.size();
			}
			m_texts.push_back(hstring{ textStr.substr(start, end - start) });
			start = end + delimiterStr.size();
		}

		double morphSpeedSeconds = static_cast<double>(MorphSpeed()) / 1000.0;

		m_morphItems.clear();
		for (size_t i = 0; i < m_texts.size(); ++i)
		{
			m_morphItems.emplace_back(
				m_texts[i],
				TextMorphItem::Timeline(
					TimeLineFrom(),
					TimeLineTo(),
					Duration(),
					TimeSpan{ static_cast<int64_t>((BeginTime().count() / 10000000.0 + i * morphSpeedSeconds) * 10000000) },
					AutoReverse())
			);
		}

		std::reverse(m_morphItems.begin(), m_morphItems.end());

		// Create text
		CanvasTextFormat textFormat;
		textFormat.FontSize(static_cast<float>(EffectFontSize()));
		textFormat.Direction(Direction());
		textFormat.VerticalAlignment(EffectVerticalAlignment());
		textFormat.HorizontalAlignment(EffectHorizontalAlignment());
		textFormat.FontWeight(EffectFontWeight());
		m_textFormat = textFormat;
	}

	void TextMorphEffect::OnCanvasSizeChanged(IInspectable const& /*sender*/, SizeChangedEventArgs const& /*e*/)
	{
		if (!m_canvas)
		{
			return;
		}

		auto size = m_canvas.ActualSize();
		m_centerPoint = { size.x / 2.0f, size.y / 2.0f };
	}

	void TextMorphEffect::OnCreateResources(CanvasControl const& /*sender*/, IInspectable const& /*args*/)
	{
		double blurAmount = BlurAmount();
		if (std::isnan(blurAmount))
		{
			BlurAmount(0.0);
			blurAmount = 0.0;
		}

		// Create blur effect
		m_blurEffect = GaussianBlurEffect();
		m_blurEffect.BlurAmount(static_cast<float>(blurAmount));

		// Create color matrix effect
		ColorMatrixEffect colorMatrixEffect;
		colorMatrixEffect.Source(m_blurEffect);
		colorMatrixEffect.ClampOutput(true);
		m_colorMatrixEffect = colorMatrixEffect;
	}

	void TextMorphEffect::OnDraw(CanvasControl const& sender, CanvasDrawEventArgs const& args)
	{
		hstring text = Text();
		if (text.empty() || m_morphItems.empty())
		{
			return;
		}

		CanvasCommandList source(sender);
		double totalSeconds = (static_cast<double>(MorphSpeed()) / 1000.0) * m_morphItems.size();

		uint64_t tickCount = GetTickCount64();
		double totalTime = std::fmod(static_cast<double>(tickCount) / MorphSpeed(), totalSeconds);
		TimeSpan currentTime{ static_cast<int64_t>(totalTime * 10000000) };

		double maxProgress = 0.0;
		{
			auto drawingSession = source.CreateDrawingSession();

			for (auto& item : m_morphItems)
			{
				double progress = item.timeline.GetCurrentProgress(currentTime);
				maxProgress = std::max(maxProgress, progress);

				Color textColor = ColorBrush();
				textColor.A = static_cast<uint8_t>(255 * progress);
				Microsoft::Graphics::Canvas::Brushes::CanvasSolidColorBrush textBrush(sender, textColor);

				drawingSession.DrawText(item.Text, m_centerPoint, textBrush, m_textFormat);
			}
		}

		// Apply blur effect based on progress
		if (m_blurEffect)
		{
			m_blurEffect.BlurAmount(static_cast<float>(20.0 * (1.0 - maxProgress)));
			m_blurEffect.Source(source);
		}

		if (m_colorMatrixEffect)
		{
			args.DrawingSession().DrawImage(m_colorMatrixEffect);
		}
		else
		{
			args.DrawingSession().DrawImage(source);
		}

		m_canvas.Invalidate();
	}
}
