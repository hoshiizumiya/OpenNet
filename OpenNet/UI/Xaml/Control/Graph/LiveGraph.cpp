#include "XamlWorkaround.h"
#include "LiveGraph.h"

#if __has_include("UI/Xaml/Control/Graph/GraphPoint.g.cpp")
#include "UI/Xaml/Control/Graph/GraphPoint.g.cpp"
#endif
#if __has_include("UI/Xaml/Control/Graph/GraphBrushData.g.cpp")
#include "UI/Xaml/Control/Graph/GraphBrushData.g.cpp"
#endif
#if __has_include("UI/Xaml/Control/Graph/LiveGraphEventArgs.g.cpp")
#include "UI/Xaml/Control/Graph/LiveGraphEventArgs.g.cpp"
#endif
#if __has_include("UI/Xaml/Control/Graph/LiveGraph.g.cpp")
#include "UI/Xaml/Control/Graph/LiveGraph.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Microsoft::Graphics::Canvas::Brushes;
using namespace winrt::Microsoft::Graphics::Canvas::Geometry;
using namespace winrt::Microsoft::Graphics::Canvas::UI;
using namespace winrt::Microsoft::Graphics::Canvas::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Hosting;

namespace
{
	TimeSpan OneSecond()
	{
		return std::chrono::duration_cast<TimeSpan>(std::chrono::seconds{ 1 });
	}

}

namespace winrt::OpenNet::UI::Xaml::Control::Graph::implementation
{
	GraphPoint::GraphPoint(float value) :
		m_value(value),
		m_space(20.0f)
	{
	}

	GraphPoint::GraphPoint(float value, float space) :
		m_value(value),
		m_space(space)
	{
	}

	float GraphPoint::Value() const
	{
		return m_value;
	}

	void GraphPoint::Value(float value)
	{
		m_value = value;
	}

	float GraphPoint::Space() const
	{
		return m_space;
	}

	void GraphPoint::Space(float value)
	{
		m_space = value;
	}

	GraphBrushData::GraphBrushData(ICanvasBrush const& brush) :
		m_brush(brush),
		m_strokeWidth(2.0f)
	{
	}

	GraphBrushData::GraphBrushData(
		ICanvasBrush const& brush,
		ICanvasBrush const& opacityBrush) :
		m_brush(brush),
		m_opacityBrush(opacityBrush),
		m_strokeWidth(2.0f)
	{
	}

	GraphBrushData::GraphBrushData(
		ICanvasBrush const& brush,
		ICanvasBrush const& opacityBrush,
		ICanvasBrush const& borderBrush) :
		m_brush(brush),
		m_opacityBrush(opacityBrush),
		m_borderBrush(borderBrush),
		m_strokeWidth(2.0f)
	{
	}

	GraphBrushData::GraphBrushData(
		ICanvasBrush const& brush,
		ICanvasBrush const& opacityBrush,
		ICanvasBrush const& borderBrush,
		float strokeWidth) :
		m_brush(brush),
		m_opacityBrush(opacityBrush),
		m_borderBrush(borderBrush),
		m_strokeWidth(strokeWidth)
	{
	}

	GraphBrushData::GraphBrushData(
		ICanvasBrush const& brush,
		ICanvasBrush const& opacityBrush,
		ICanvasBrush const& borderBrush,
		float strokeWidth,
		CanvasStrokeStyle const& strokeStyle) :
		m_brush(brush),
		m_opacityBrush(opacityBrush),
		m_borderBrush(borderBrush),
		m_strokeStyle(strokeStyle),
		m_strokeWidth(strokeWidth)
	{
	}

	ICanvasBrush GraphBrushData::Brush() const
	{
		return m_brush;
	}

	void GraphBrushData::Brush(ICanvasBrush const& value)
	{
		m_brush = value;
	}

	ICanvasBrush GraphBrushData::OpacityBrush() const
	{
		return m_opacityBrush;
	}

	void GraphBrushData::OpacityBrush(ICanvasBrush const& value)
	{
		m_opacityBrush = value;
	}

	ICanvasBrush GraphBrushData::BorderBrush() const
	{
		return m_borderBrush;
	}

	void GraphBrushData::BorderBrush(ICanvasBrush const& value)
	{
		m_borderBrush = value;
	}

	float GraphBrushData::StrokeWidth() const
	{
		return m_strokeWidth;
	}

	void GraphBrushData::StrokeWidth(float value)
	{
		m_strokeWidth = value;
	}

	CanvasStrokeStyle GraphBrushData::StrokeStyle() const
	{
		return m_strokeStyle;
	}

	void GraphBrushData::StrokeStyle(CanvasStrokeStyle const& value)
	{
		m_strokeStyle = value;
	}

	bool GraphBrushData::IsDisposed() const noexcept
	{
		return m_isDisposed;
	}

	void GraphBrushData::Dispose() noexcept
	{
		if (m_isDisposed)
		{
			return;
		}

		// Win2D wrappers may share the same native resource with the page that
		// registered this data. Close() invalidates every wrapper and can race the
		// animated draw callback during navigation. Releasing our references lets
		// COM destroy the resource only after all draw snapshots are gone.
		m_brush = nullptr;
		m_opacityBrush = nullptr;
		m_borderBrush = nullptr;
		m_strokeStyle = nullptr;
		m_isDisposed = true;
	}

	LiveGraphEventArgs::LiveGraphEventArgs(
		ICanvasAnimatedControl const& canvasAnimatedControl,
		CanvasAnimatedDrawEventArgs const& drawEventArgs) :
		m_canvasAnimatedControl(canvasAnimatedControl),
		m_drawEventArgs(drawEventArgs)
	{
	}

	ICanvasAnimatedControl LiveGraphEventArgs::CanvasAnimatedControl() const
	{
		return m_canvasAnimatedControl;
	}

	CanvasAnimatedDrawEventArgs LiveGraphEventArgs::DrawEventArgs() const
	{
		return m_drawEventArgs;
	}

	void LiveGraph::EnsureDependencyProperties()
	{
		if (s_highlightLineBehaviorProperty)
		{
			return;
		}

		auto const ownerType =
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Graph::LiveGraph>();

		s_highlightLineBehaviorProperty = DependencyProperty::Register(
			L"HighlightLineBehavior",
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior>(),
			ownerType,
			PropertyMetadata{
				box_value(OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::EachPoint),
				PropertyChangedCallback{ &LiveGraph::OnHighlightLineBehaviorChanged } });

		s_highlightLineVisibilityProperty = DependencyProperty::Register(
			L"HighlightLineVisibility",
			winrt::xaml_typename<winrt::Microsoft::UI::Xaml::Visibility>(),
			ownerType,
			PropertyMetadata{
				box_value(winrt::Microsoft::UI::Xaml::Visibility::Visible),
				PropertyChangedCallback{ &LiveGraph::OnHighlightLineVisibilityChanged } });

		s_backgroundModeProperty = DependencyProperty::Register(
			L"BackgroundMode",
			winrt::xaml_typename<OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode>(),
			ownerType,
			PropertyMetadata{
				box_value(OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode::Cross),
				PropertyChangedCallback{ &LiveGraph::OnBackgroundModeChanged } });

		s_dotSpacingProperty = DependencyProperty::Register(
			L"DotSpacing",
			winrt::xaml_typename<double>(),
			ownerType,
			PropertyMetadata{
				box_value(6.0),
				PropertyChangedCallback{ &LiveGraph::OnDotSpacingChanged } });

		s_crossSpacingProperty = DependencyProperty::Register(
			L"CrossSpacing",
			winrt::xaml_typename<double>(),
			ownerType,
			PropertyMetadata{
				box_value(30.0),
				PropertyChangedCallback{ &LiveGraph::OnCrossSpacingChanged } });

		s_backgroundColorProperty = DependencyProperty::Register(
			L"BackgroundColor",
			winrt::xaml_typename<IReference<Color>>(),
			ownerType,
			PropertyMetadata{
				nullptr,
				PropertyChangedCallback{ &LiveGraph::OnBackgroundColorChanged } });

		s_clearColorProperty = DependencyProperty::Register(
			L"ClearColor",
			winrt::xaml_typename<IReference<Color>>(),
			ownerType,
			PropertyMetadata{
				box_value(Colors::Transparent()),
				PropertyChangedCallback{ &LiveGraph::OnClearColorChanged } });

		s_horizontalScrollDistanceProperty = DependencyProperty::Register(
			L"HorizontalScrollDistance",
			winrt::xaml_typename<double>(),
			ownerType,
			PropertyMetadata{
				box_value(1.0),
				PropertyChangedCallback{ &LiveGraph::OnHorizontalScrollChanged } });

		s_horizontalScrollDurationProperty = DependencyProperty::Register(
			L"HorizontalScrollDuration",
			winrt::xaml_typename<TimeSpan>(),
			ownerType,
			PropertyMetadata{
				box_value(OneSecond()),
				PropertyChangedCallback{ &LiveGraph::OnHorizontalScrollChanged } });

		s_highlightLineAnimationDurationProperty = DependencyProperty::Register(
			L"HighlightLineAnimationDuration",
			winrt::xaml_typename<TimeSpan>(),
			ownerType,
			PropertyMetadata{
				box_value(std::chrono::duration_cast<TimeSpan>(
					std::chrono::milliseconds{ 300 })),
				PropertyChangedCallback{
					&LiveGraph::OnHighlightLineAnimationDurationChanged } });

		s_historyBufferScreensProperty = DependencyProperty::Register(
			L"HistoryBufferScreens",
			winrt::xaml_typename<double>(),
			ownerType,
			PropertyMetadata{
				box_value(1.0),
				PropertyChangedCallback{
					&LiveGraph::OnHistoryBufferScreensChanged } });

		s_highlightLineContentProperty = DependencyProperty::Register(
			L"HighlightLineContent",
			winrt::xaml_typename<IInspectable>(),
			ownerType,
			PropertyMetadata{
				nullptr,
				PropertyChangedCallback{ &LiveGraph::OnHighlightLineContentChanged } });
	}

	DependencyProperty LiveGraph::HighlightLineBehaviorProperty()
	{
		return s_highlightLineBehaviorProperty;
	}

	DependencyProperty LiveGraph::HighlightLineVisibilityProperty()
	{
		return s_highlightLineVisibilityProperty;
	}

	DependencyProperty LiveGraph::BackgroundModeProperty()
	{
		return s_backgroundModeProperty;
	}

	DependencyProperty LiveGraph::DotSpacingProperty()
	{
		return s_dotSpacingProperty;
	}

	DependencyProperty LiveGraph::CrossSpacingProperty()
	{
		return s_crossSpacingProperty;
	}

	DependencyProperty LiveGraph::BackgroundColorProperty()
	{
		return s_backgroundColorProperty;
	}

	DependencyProperty LiveGraph::ClearColorProperty()
	{
		return s_clearColorProperty;
	}

	DependencyProperty LiveGraph::HorizontalScrollDistanceProperty()
	{
		return s_horizontalScrollDistanceProperty;
	}

	DependencyProperty LiveGraph::HorizontalScrollDurationProperty()
	{
		return s_horizontalScrollDurationProperty;
	}

	DependencyProperty LiveGraph::HighlightLineAnimationDurationProperty()
	{
		return s_highlightLineAnimationDurationProperty;
	}

	DependencyProperty LiveGraph::HistoryBufferScreensProperty()
	{
		return s_historyBufferScreensProperty;
	}

	DependencyProperty LiveGraph::HighlightLineContentProperty()
	{
		return s_highlightLineContentProperty;
	}

	OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior
		LiveGraph::HighlightLineBehavior()
	{
		return unbox_value<OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior>(
			GetValue(s_highlightLineBehaviorProperty));
	}

	void LiveGraph::HighlightLineBehavior(
		OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior value)
	{
		SetValue(s_highlightLineBehaviorProperty, box_value(value));
	}

	winrt::Microsoft::UI::Xaml::Visibility LiveGraph::HighlightLineVisibility()
	{
		return unbox_value<winrt::Microsoft::UI::Xaml::Visibility>(
			GetValue(s_highlightLineVisibilityProperty));
	}

	void LiveGraph::HighlightLineVisibility(
		winrt::Microsoft::UI::Xaml::Visibility value)
	{
		SetValue(s_highlightLineVisibilityProperty, box_value(value));
	}

	OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode
		LiveGraph::BackgroundMode()
	{
		return unbox_value<OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode>(
			GetValue(s_backgroundModeProperty));
	}

	void LiveGraph::BackgroundMode(
		OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode value)
	{
		SetValue(s_backgroundModeProperty, box_value(value));
	}

	double LiveGraph::DotSpacing()
	{
		return unbox_value<double>(GetValue(s_dotSpacingProperty));
	}

	void LiveGraph::DotSpacing(double value)
	{
		SetValue(s_dotSpacingProperty, box_value(value));
	}

	double LiveGraph::CrossSpacing()
	{
		return unbox_value<double>(GetValue(s_crossSpacingProperty));
	}

	void LiveGraph::CrossSpacing(double value)
	{
		SetValue(s_crossSpacingProperty, box_value(value));
	}

	IReference<Color> LiveGraph::BackgroundColor()
	{
		return GetValue(s_backgroundColorProperty).try_as<IReference<Color>>();
	}

	void LiveGraph::BackgroundColor(IReference<Color> const& value)
	{
		SetValue(s_backgroundColorProperty, value);
	}

	IReference<Color> LiveGraph::ClearColor()
	{
		return GetValue(s_clearColorProperty).try_as<IReference<Color>>();
	}

	void LiveGraph::ClearColor(IReference<Color> const& value)
	{
		SetValue(s_clearColorProperty, value);
	}

	double LiveGraph::HorizontalScrollDistance()
	{
		return unbox_value<double>(GetValue(s_horizontalScrollDistanceProperty));
	}

	void LiveGraph::HorizontalScrollDistance(double value)
	{
		SetValue(s_horizontalScrollDistanceProperty, box_value(value));
	}

	TimeSpan LiveGraph::HorizontalScrollDuration()
	{
		return unbox_value<TimeSpan>(GetValue(s_horizontalScrollDurationProperty));
	}

	void LiveGraph::HorizontalScrollDuration(TimeSpan const& value)
	{
		SetValue(s_horizontalScrollDurationProperty, box_value(value));
	}

	TimeSpan LiveGraph::HighlightLineAnimationDuration()
	{
		return unbox_value<TimeSpan>(
			GetValue(s_highlightLineAnimationDurationProperty));
	}

	void LiveGraph::HighlightLineAnimationDuration(TimeSpan const& value)
	{
		SetValue(s_highlightLineAnimationDurationProperty, box_value(value));
	}

	double LiveGraph::HistoryBufferScreens()
	{
		return unbox_value<double>(GetValue(s_historyBufferScreensProperty));
	}

	void LiveGraph::HistoryBufferScreens(double value)
	{
		SetValue(s_historyBufferScreensProperty, box_value(value));
	}

	IInspectable LiveGraph::HighlightLineContent()
	{
		return GetValue(s_highlightLineContentProperty);
	}

	void LiveGraph::HighlightLineContent(IInspectable const& value)
	{
		SetValue(s_highlightLineContentProperty, value);
	}

	void LiveGraph::OnHighlightLineBehaviorChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		std::scoped_lock lock(self->m_graphMutex);
		self->m_highlightLineBehavior =
			unbox_value<OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior>(
				args.NewValue());
		self->m_currentPolygonIndex = 0;
		self->m_currentPointIndex = 0;
		self->m_currentLineY.reset();
		self->m_lastHighlightedRevision = 0;
	}

	void LiveGraph::OnHighlightLineVisibilityChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		std::scoped_lock lock(self->m_graphMutex);
		self->m_highlightLineVisibility =
			unbox_value<winrt::Microsoft::UI::Xaml::Visibility>(args.NewValue());
		if (self->m_highlightLineVisibility == Visibility::Visible)
		{
			// Force the current value to be published again after the overlay is
			// re-enabled, even when its Y coordinate did not change while hidden.
			self->m_currentLineY.reset();
			self->m_lastHighlightedRevision = 0;
		}
	}

	void LiveGraph::OnBackgroundModeChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		std::scoped_lock lock(self->m_graphMutex);
		self->m_backgroundMode =
			unbox_value<OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode>(
				args.NewValue());
	}

	void LiveGraph::OnDotSpacingChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		std::scoped_lock lock(self->m_graphMutex);
		self->m_dotSpacing = static_cast<float>(
			std::max(1.0, unbox_value<double>(args.NewValue())));
	}

	void LiveGraph::OnCrossSpacingChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		std::scoped_lock lock(self->m_graphMutex);
		self->m_crossSpacing = static_cast<float>(
			std::max(1.0, unbox_value<double>(args.NewValue())));
	}

	void LiveGraph::OnBackgroundColorChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		if (args.NewValue())
		{
			std::scoped_lock lock(self->m_graphMutex);
			self->m_drawColor = unbox_value<Color>(args.NewValue());
		}
		else
		{
			self->UpdateDrawColor();
		}
	}

	void LiveGraph::OnClearColorChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		{
			std::scoped_lock lock(self->m_graphMutex);
			self->m_clearColor =
				unbox_value_or<Color>(args.NewValue(), Colors::Transparent());
		}
		self->UpdateClearColor();
	}

	void LiveGraph::OnHorizontalScrollChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const&)
	{
		GetSelf(dependencyObject)->UpdateHorizontalScrollSpeed();
	}

	void LiveGraph::OnHighlightLineAnimationDurationChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		auto duration = unbox_value<TimeSpan>(args.NewValue());
		if (duration.count() < 0)
		{
			duration = TimeSpan{};
		}
		std::scoped_lock lock(self->m_graphMutex);
		self->m_highlightLineAnimationDuration = duration;
	}

	void LiveGraph::OnHistoryBufferScreensChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		auto const screens = std::clamp(
			unbox_value<double>(args.NewValue()),
			0.0,
			4.0);
		std::scoped_lock lock(self->m_graphMutex);
		self->m_historyBufferScreens = static_cast<float>(screens);
	}

	void LiveGraph::OnHighlightLineContentChanged(
		DependencyObject const& dependencyObject,
		DependencyPropertyChangedEventArgs const& args)
	{
		auto self = GetSelf(dependencyObject);
		std::scoped_lock lock(self->m_graphMutex);
		self->m_highlightLineContent = args.NewValue();
	}

	void LiveGraph::OnApplyTemplate()
	{
		DetachCanvasHandlers();
		base_type::OnApplyTemplate();

		m_canvas = GetTemplateChild(CanvasPartName).try_as<CanvasAnimatedControl>();
		m_hostGrid = GetTemplateChild(HostGridPartName).try_as<Grid>();

		if (!m_canvas || !m_hostGrid)
		{
			m_gridVisual = nullptr;
			m_compositor = nullptr;
			return;
		}

		m_gridVisual = ElementCompositionPreview::GetElementVisual(m_hostGrid);
		m_compositor = m_gridVisual.Compositor();

		UpdateDrawColor();
		UpdateClearColor();
		UpdateHorizontalScrollSpeed();

		auto strong = get_strong();

		m_canvasDrawToken = m_canvas.Draw(
			[strong](
				ICanvasAnimatedControl const& sender,
				CanvasAnimatedDrawEventArgs const& args)
		{
			if (auto strongThis = strong.get())
			{
				strongThis->OnDraw(sender, args);
			}
		});

		m_canvasCreateResourcesToken = m_canvas.CreateResources(
			[strong](
				CanvasAnimatedControl const& sender,
				CanvasCreateResourcesEventArgs const& args)
		{
			if (auto strongThis = strong.get())
			{
				strongThis->OnCreateResources(sender, args);
			}
		});

		if (m_actualThemeChangedToken.value)
		{
			ActualThemeChanged(m_actualThemeChangedToken);
		}
		m_actualThemeChangedToken = ActualThemeChanged(
			[strong](FrameworkElement const& sender, IInspectable const& args)
		{
			if (auto strongThis = strong.get())
			{
				strongThis->OnActualThemeChanged(sender, args);
			}
		});

		if (m_sizeChangedToken.value)
		{
			SizeChanged(m_sizeChangedToken);
		}
		m_sizeChangedToken = SizeChanged(
			[strong](IInspectable const& sender, SizeChangedEventArgs const& args)
		{
			if (auto strongThis = strong.get())
			{
				strongThis->OnSizeChanged(sender, args);
			}
		});

		if (m_unloadedToken.value)
		{
			Unloaded(m_unloadedToken);
		}
		m_unloadedToken = Unloaded(
			[strong](IInspectable const& sender, RoutedEventArgs const& args)
		{
			if (auto strongThis = strong.get())
			{
				strongThis->OnUnloaded(sender, args);
			}
		});

		if (m_loadedToken.value)
		{
			Loaded(m_loadedToken);
		}
		m_loadedToken = Loaded([strong](IInspectable const&, RoutedEventArgs const&)
		{
			if (auto strongThis = strong.get(); strongThis && strongThis->m_canvas)
			{
				strongThis->m_canvas.Paused(false);
			}
		});
	}

	void LiveGraph::DetachCanvasHandlers()
	{
		if (!m_canvas)
		{
			return;
		}

		if (m_canvasDrawToken.value)
		{
			m_canvas.Draw(m_canvasDrawToken);
			m_canvasDrawToken = {};
		}

		if (m_canvasCreateResourcesToken.value)
		{
			m_canvas.CreateResources(m_canvasCreateResourcesToken);
			m_canvasCreateResourcesToken = {};
		}
	}

	hstring LiveGraph::RegisterGraphBrush(
		OpenNet::UI::Xaml::Control::Graph::GraphBrushData const& brushData)
	{
		auto key = to_hstring(GuidHelper::CreateNewGuid());
		std::scoped_lock lock(m_graphMutex);
		m_polygonBrushes.insert_or_assign(key, brushData);
		return key;
	}

	void LiveGraph::UpdateGraphBrush(
		hstring const& key,
		OpenNet::UI::Xaml::Control::Graph::GraphBrushData const& brushData)
	{
		std::scoped_lock lock(m_graphMutex);
		m_polygonBrushes.insert_or_assign(key, brushData);
	}

	void LiveGraph::ResetDynamicGraph(hstring const& key)
	{
		std::scoped_lock lock(m_graphMutex);

		if (auto liveIterator = m_livePolygons.find(key);
			liveIterator != m_livePolygons.end())
		{
			auto const polygon = liveIterator->second;
			std::erase(m_polygons, polygon);
			m_livePolygons.erase(liveIterator);
		}

		if (auto brushIterator = m_polygonBrushes.find(key);
			brushIterator != m_polygonBrushes.end())
		{
			if (brushIterator->second)
			{
				get_self<GraphBrushData>(brushIterator->second)->Dispose();
			}
			m_polygonBrushes.erase(brushIterator);
		}

		m_currentPolygonIndex = 0;
		m_currentPointIndex = 0;
		m_currentLineY.reset();
		m_lastHighlightedRevision = 0;
	}

	void LiveGraph::OnUnloaded(IInspectable const&, RoutedEventArgs const&)
	{
		// Unloaded is a navigation state, not object destruction. Pause the game
		// loop but keep registered resources alive for cached-page re-entry.
		if (m_canvas)
		{
			m_canvas.Paused(true);
		}
	}

	void LiveGraph::OnCreateResources(
		CanvasAnimatedControl const& sender,
		CanvasCreateResourcesEventArgs const&)
	{
		m_createResources(*this, sender);
	}

	void LiveGraph::OnActualThemeChanged(
		FrameworkElement const&,
		IInspectable const&)
	{
		UpdateDrawColor();
	}

	void LiveGraph::UpdateDrawColor()
	{
		if (BackgroundColor())
		{
			return;
		}

		auto const color =
			ActualTheme() == ElementTheme::Light
			? ColorHelper::FromArgb(32, 0, 0, 0)
			: ColorHelper::FromArgb(32, 255, 255, 255);

		std::scoped_lock lock(m_graphMutex);
		m_drawColor = color;
	}

	void LiveGraph::OnSizeChanged(
		IInspectable const&,
		SizeChangedEventArgs const& args)
	{
		ResizeGraphPoints(
			static_cast<float>(args.NewSize().Width),
			static_cast<float>(args.NewSize().Height),
			static_cast<float>(args.PreviousSize().Width),
			static_cast<float>(args.PreviousSize().Height));
	}

	void LiveGraph::ResizeGraphPoints(
		float newWidth,
		float newHeight,
		float oldWidth,
		float oldHeight)
	{
		auto const hasWidthChange =
			oldWidth > 0.0f &&
			std::isfinite(newWidth) &&
			std::isfinite(oldWidth);
		auto const hasHeightChange =
			oldHeight > 0.0f &&
			std::isfinite(newHeight) &&
			std::isfinite(oldHeight);
		if (!hasWidthChange && !hasHeightChange)
		{
			return;
		}

		auto const widthDelta =
			hasWidthChange ? newWidth - oldWidth : 0.0f;
		auto const scaleY =
			hasHeightChange ? newHeight / oldHeight : 1.0f;
		std::optional<float> resizedLineY;
		{
			std::scoped_lock lock(m_graphMutex);

			for (auto const& polygon : m_polygons)
			{
				// Point X coordinates are created relative to the canvas width that
				// existed when the graph was populated. Keep the entire polygon
				// anchored to the new right edge when the control width changes.
				polygon->OffsetX += widthDelta;

				if (hasHeightChange)
				{
					for (auto& point : polygon->Points)
					{
						point.y *= scaleY;
					}
					polygon->CurrentY *= scaleY;
				}
			}

			if (hasHeightChange && m_currentLineY)
			{
				*m_currentLineY *= scaleY;
				resizedLineY = *m_currentLineY;
			}
		}

		// The composition visual is independent from the Win2D points. Resize it
		// explicitly or the horizontal line stays at its pre-resize pixel Y.
		if (resizedLineY)
		{
			m_highlightLineUpdated(*this, *resizedLineY);
			MoveHostGridWithAnimation(*resizedLineY);
		}
	}

	void LiveGraph::UpdateClearColor()
	{
		if (!m_canvas)
		{
			return;
		}

		Color color;
		{
			std::scoped_lock lock(m_graphMutex);
			color = m_clearColor;
		}
		m_canvas.ClearColor(color);
	}

	void LiveGraph::UpdateHorizontalScrollSpeed()
	{
		auto const speed = CalculateSpeed(
			static_cast<float>(HorizontalScrollDistance()),
			HorizontalScrollDuration());

		std::scoped_lock lock(m_graphMutex);
		m_horizontalScrollSpeed = speed;
	}

	float LiveGraph::CalculateSpeed(float distance, TimeSpan const& time)
	{
		auto const seconds =
			std::chrono::duration<double>(time).count();
		if (!std::isfinite(distance) ||
			!std::isfinite(seconds) ||
			seconds <= 0.0)
		{
			return 0.0f;
		}

		return distance / static_cast<float>(seconds);
	}

	float LiveGraph::NormalizeY(float percent, float height)
	{
		return height - (percent / 100.0f) * height;
	}

	void LiveGraph::AddDynamicPoint(
		hstring const& key,
		OpenNet::UI::Xaml::Control::Graph::GraphPoint const& point)
	{
		AddDynamicPoint(key, point, false);
	}

	void LiveGraph::AddDynamicPoint(
		hstring const& key,
		OpenNet::UI::Xaml::Control::Graph::GraphPoint const& point,
		bool isRounded)
	{
		if (!m_canvas || !point)
		{
			return;
		}

		auto const canvasSize = m_canvas.Size();
		auto const height = static_cast<float>(canvasSize.Height);
		auto const canvasWidth = static_cast<float>(canvasSize.Width);

		std::scoped_lock lock(m_graphMutex);

		std::shared_ptr<UserPolygon> polygon;
		if (auto iterator = m_livePolygons.find(key);
			iterator != m_livePolygons.end())
		{
			polygon = iterator->second;
		}
		else
		{
			polygon = std::make_shared<UserPolygon>();
			polygon->IsRounded = isRounded;
			polygon->Key = key;
			m_livePolygons.insert_or_assign(key, polygon);
			m_polygons.push_back(polygon);
		}

		auto const startX =
			polygon->Points.empty()
			? canvasWidth
			: polygon->Points.back().x + point.Space();
		auto const y = NormalizeY(point.Value(), height);

		polygon->CurrentY = y;
		++polygon->Revision;
		polygon->Points.push_back({ startX, y });

		// The original per-point cursor reaches one-past-the-end while waiting
		// for more live data. Re-arm it at the newly appended point so the
		// highlight line continues to follow a long-running dynamic graph.
		if (m_currentPolygonIndex >= m_polygons.size())
		{
			auto const iterator =
				std::find(m_polygons.begin(), m_polygons.end(), polygon);
			if (iterator != m_polygons.end())
			{
				m_currentPolygonIndex =
					static_cast<size_t>(std::distance(m_polygons.begin(), iterator));
				m_currentPointIndex = polygon->Points.size() - 1;
				m_currentLineY.reset();
			}
		}
	}

	void LiveGraph::AddStaticPoints(
		hstring const& key,
		IIterable<OpenNet::UI::Xaml::Control::Graph::GraphPoint> const& points)
	{
		AddStaticPoints(key, points, false);
	}

	void LiveGraph::AddStaticPoints(
		hstring const& key,
		IIterable<OpenNet::UI::Xaml::Control::Graph::GraphPoint> const& points,
		bool isRounded)
	{
		if (!m_canvas || !points)
		{
			return;
		}

		auto const canvasSize = m_canvas.Size();
		auto const height = static_cast<float>(canvasSize.Height);
		auto currentX = static_cast<float>(canvasSize.Width) + 10.0f;

		auto polygon = std::make_shared<UserPolygon>();
		polygon->IsRounded = isRounded;
		polygon->Key = key;

		for (auto const& point : points)
		{
			if (!point)
			{
				continue;
			}

			auto const y = NormalizeY(point.Value(), height);
			polygon->Points.push_back({ currentX, y });
			polygon->CurrentY = y;
			currentX += point.Space();
		}

		std::scoped_lock lock(m_graphMutex);
		auto const cursorWasAtEnd = m_currentPolygonIndex >= m_polygons.size();
		m_polygons.push_back(std::move(polygon));
		if (cursorWasAtEnd)
		{
			m_currentPolygonIndex = m_polygons.size() - 1;
			m_currentPointIndex = 0;
			m_currentLineY.reset();
		}
	}

	OpenNet::UI::Xaml::Control::Graph::GraphBrushData LiveGraph::GetCustomBrush(
		ICanvasResourceCreator const& resourceCreator,
		array_view<Color const> colors)
	{
		if (!resourceCreator)
		{
			throw hresult_invalid_argument(L"resourceCreator cannot be null.");
		}
		if (colors.empty())
		{
			throw hresult_invalid_argument(L"colors cannot be empty.");
		}

		std::vector<CanvasGradientStop> colorStops;
		colorStops.reserve(colors.size());

		auto const denominator =
			colors.size() > 1 ? static_cast<float>(colors.size() - 1) : 1.0f;
		for (size_t index = 0; index < colors.size(); ++index)
		{
			colorStops.push_back(
				{ static_cast<float>(index) / denominator, colors[index] });
		}

		std::array<CanvasGradientStop, 2> opacityStops
		{
			CanvasGradientStop{ 0.0f, ColorHelper::FromArgb(0, 0, 0, 0) },
			CanvasGradientStop{ 1.0f, ColorHelper::FromArgb(255, 255, 255, 255) }
		};

		CanvasLinearGradientBrush brush(resourceCreator, colorStops);
		CanvasLinearGradientBrush opacityBrush(resourceCreator, opacityStops);
		CanvasSolidColorBrush borderBrush(resourceCreator, colors.front());

		auto const canvasHeight =
			static_cast<float>(m_canvas ? m_canvas.ActualHeight() : ActualHeight());
		brush.StartPoint({ 0.0f, canvasHeight });
		brush.EndPoint({ 0.0f, 0.0f });
		opacityBrush.StartPoint({ 0.0f, canvasHeight });
		opacityBrush.EndPoint({ 0.0f, 0.0f });

		return winrt::make<GraphBrushData>(
			brush.as<ICanvasBrush>(),
			opacityBrush.as<ICanvasBrush>(),
			borderBrush.as<ICanvasBrush>());
	}

	OpenNet::UI::Xaml::Control::Graph::GraphBrushData LiveGraph::GetGreenBrush(
		ICanvasResourceCreator const& resourceCreator)
	{
		std::array colors
		{
			ColorHelper::FromArgb(128, 144, 238, 144),
			ColorHelper::FromArgb(180, 60, 179, 113),
			ColorHelper::FromArgb(220, 0, 128, 0)
		};
		return GetCustomBrush(resourceCreator, colors);
	}

	OpenNet::UI::Xaml::Control::Graph::GraphBrushData LiveGraph::GetRedBrush(
		ICanvasResourceCreator const& resourceCreator)
	{
		std::array colors
		{
			ColorHelper::FromArgb(128, 255, 182, 193),
			ColorHelper::FromArgb(180, 220, 20, 60),
			ColorHelper::FromArgb(220, 139, 0, 0)
		};
		return GetCustomBrush(resourceCreator, colors);
	}

	OpenNet::UI::Xaml::Control::Graph::GraphBrushData LiveGraph::GetBlueBrush(
		ICanvasResourceCreator const& resourceCreator)
	{
		std::array colors
		{
			ColorHelper::FromArgb(128, 173, 216, 230),
			ColorHelper::FromArgb(180, 70, 130, 180),
			ColorHelper::FromArgb(220, 25, 25, 112)
		};
		return GetCustomBrush(resourceCreator, colors);
	}

	OpenNet::UI::Xaml::Control::Graph::GraphBrushData LiveGraph::GetPurpleBrush(
		ICanvasResourceCreator const& resourceCreator)
	{
		std::array colors
		{
			ColorHelper::FromArgb(128, 216, 191, 216),
			ColorHelper::FromArgb(180, 160, 120, 200),
			ColorHelper::FromArgb(220, 106, 13, 173)
		};
		return GetCustomBrush(resourceCreator, colors);
	}

	CanvasAnimatedControl LiveGraph::GetCanvasAnimatedControl()
	{
		return m_canvas;
	}

	void LiveGraph::UpdateHighlightLine()
	{
		if (m_polygons.empty() ||
			m_highlightLineVisibility == Visibility::Collapsed ||
			!m_highlightLineContent ||
			!m_canvas)
		{
			return;
		}

		auto const canvasWidth = static_cast<float>(m_canvas.Size().Width);

		auto queueLineUpdate = [this](float y)
		{
			auto weakThis = get_weak();
			DispatcherQueue().TryEnqueue(
				[weakThis, y]
			{
				if (auto strongThis = weakThis.get())
				{
					strongThis->m_highlightLineUpdated(*strongThis, y);
					strongThis->MoveHostGridWithAnimation(y);
				}
			});
		};

		auto publishLine = [&queueLineUpdate, this](float y, bool force = false)
		{
			if (!std::isfinite(y))
			{
				return;
			}

			auto const height = static_cast<float>(m_canvas.Size().Height);
			y = std::clamp(y, 0.0f, std::max(0.0f, height));
			if (force || !m_currentLineY || std::abs(*m_currentLineY - y) > 0.01f)
			{
				m_currentLineY = y;
				queueLineUpdate(y);
			}
		};

		switch (m_highlightLineBehavior)
		{
			case OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::
			HigherPointAcrossGraphs:
				case OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::
				LowerPointAcrossGraphs:
				{
					std::optional<float> targetLineY;

					for (auto const& polygon : m_polygons)
					{
						for (auto const& point : polygon->Points)
						{
							auto const screenX = point.x + polygon->OffsetX;
							if (screenX < 0.0f || screenX > canvasWidth)
							{
								continue;
							}

							if (!targetLineY)
							{
								targetLineY = point.y;
							}
							else if (
								m_highlightLineBehavior ==
								OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::
								HigherPointAcrossGraphs &&
								point.y < *targetLineY)
							{
								targetLineY = point.y;
							}
							else if (
								m_highlightLineBehavior ==
								OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::
								LowerPointAcrossGraphs &&
								point.y > *targetLineY)
							{
								targetLineY = point.y;
							}
						}
					}

					if (targetLineY)
					{
						publishLine(*targetLineY);
					}
					break;
				}

				case OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::
				HigherPointPerGraph:
					case OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::
					LowerPointPerGraph:
					{
						while (m_currentPolygonIndex < m_polygons.size())
						{
							auto const& polygon = m_polygons[m_currentPolygonIndex];
							while (m_currentPointIndex < polygon->Points.size()
								   && polygon->Points[m_currentPointIndex].x
								   + polygon->OffsetX < 0.0f)
							{
								++m_currentPointIndex;
							}

							if (m_currentPointIndex >= polygon->Points.size())
							{
								++m_currentPolygonIndex;
								m_currentPointIndex = 0;
								m_currentLineY.reset();
								continue;
							}

							auto const& point = polygon->Points[m_currentPointIndex];
							if (point.x + polygon->OffsetX > canvasWidth)
							{
								return;
							}

							auto const targetY = point.y;
							auto const shouldMove =
								!m_currentLineY ||
								(m_highlightLineBehavior ==
								 OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::
								 HigherPointPerGraph && targetY < *m_currentLineY) ||
								(m_highlightLineBehavior ==
								 OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::
								 LowerPointPerGraph && targetY > *m_currentLineY);
							if (shouldMove)
							{
								publishLine(targetY);
							}
							++m_currentPointIndex;
							return;
						}
						break;
					}

					case OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::EachPoint:
					{
						// A dynamic graph represents a live value. Follow the newest visible
						// point of the primary live series instead of replaying old points
						// from every series through one global cursor.
						for (auto const& polygon : m_polygons)
						{
							auto const live = m_livePolygons.find(polygon->Key);
							if (live == m_livePolygons.end() || live->second != polygon)
							{
								continue;
							}

							for (auto point = polygon->Points.rbegin();
								 point != polygon->Points.rend(); ++point)
							{
								auto const screenX = point->x + polygon->OffsetX;
								if (screenX >= 0.0f && screenX <= canvasWidth)
								{
									if (m_lastHighlightedRevision != polygon->Revision)
									{
										m_lastHighlightedRevision = polygon->Revision;
										// Publish every new sample even if adaptive scaling leaves
										// it at the same normalized Y coordinate. Its real value
										// and formatted unit may still have changed.
										publishLine(point->y, true);
									}
									return;
								}
							}
							return;
						}

						// Static graphs still advance point-by-point. Skip points already
						// left of the viewport so pruning or resizing cannot stall the line.
						while (m_currentPolygonIndex < m_polygons.size())
						{
							auto const& polygon = m_polygons[m_currentPolygonIndex];
							while (m_currentPointIndex < polygon->Points.size()
								   && polygon->Points[m_currentPointIndex].x
								   + polygon->OffsetX < 0.0f)
							{
								++m_currentPointIndex;
							}

							if (m_currentPointIndex >= polygon->Points.size())
							{
								++m_currentPolygonIndex;
								m_currentPointIndex = 0;
								continue;
							}

							auto const& point = polygon->Points[m_currentPointIndex];
							if (point.x + polygon->OffsetX > canvasWidth)
							{
								return;
							}
							publishLine(point.y);
							++m_currentPointIndex;
							return;
						}
						break;
					}
		}
	}

	void LiveGraph::MoveHostGridWithAnimation(float y)
	{
		if (!m_compositor || !m_gridVisual || !m_canvas)
		{
			return;
		}
		y = std::clamp(
			y,
			0.0f,
			std::max(0.0f, static_cast<float>(m_canvas.Size().Height)));

		auto animation = m_compositor.CreateScalarKeyFrameAnimation();
		auto easing = m_compositor.CreateCubicBezierEasingFunction(
			{ 0.0f, 0.0f },
			{ 0.2f, 1.0f });
		animation.InsertKeyFrame(1.0f, y, easing);
		animation.Duration(m_highlightLineAnimationDuration);
		m_gridVisual.StartAnimation(L"Offset.Y", animation);
	}

	void LiveGraph::DrawUserPolygons(
		CanvasDrawingSession const& drawingSession,
		float width,
		float height)
	{
		for (auto const& polygon : m_polygons)
		{
			if (polygon->Points.empty())
			{
				continue;
			}

			CanvasPathBuilder pathBuilder(drawingSession);
			auto const startX = polygon->Points.front().x + polygon->OffsetX;
			pathBuilder.BeginFigure({ startX, height });
			pathBuilder.AddLine({ startX, polygon->Points.front().y });

			if (polygon->IsRounded)
			{
				auto previous = polygon->Points.front();
				constexpr auto smoothness = 0.25f;

				for (size_t index = 1; index < polygon->Points.size(); ++index)
				{
					auto const current = polygon->Points[index];
					auto const next =
						index + 1 < polygon->Points.size()
						? polygon->Points[index + 1]
						: current;
					auto const prior =
						polygon->Points[index >= 2 ? index - 2 : 0];

					float2 controlPoint1
					{
						previous.x + (current.x - prior.x) * smoothness,
						previous.y + (current.y - prior.y) * smoothness
					};
					float2 controlPoint2
					{
						current.x - (next.x - previous.x) * smoothness,
						current.y - (next.y - previous.y) * smoothness
					};

					pathBuilder.AddCubicBezier(
						{ controlPoint1.x + polygon->OffsetX, controlPoint1.y },
						{ controlPoint2.x + polygon->OffsetX, controlPoint2.y },
						{ current.x + polygon->OffsetX, current.y });
					previous = current;
				}
			}
			else
			{
				for (size_t index = 1; index < polygon->Points.size(); ++index)
				{
					auto const point = polygon->Points[index];
					pathBuilder.AddLine(
						{ point.x + polygon->OffsetX, point.y });
				}
			}

			auto endX = polygon->Points.back().x + polygon->OffsetX;
			auto const live = m_livePolygons.find(polygon->Key);
			auto const isLive =
				live != m_livePolygons.end() && live->second == polygon;
			if (isLive && endX < width)
			{
				// Hold the latest live value to the right edge between samples.
				// Otherwise a blank strip grows until the next point arrives.
				endX = width;
				pathBuilder.AddLine({ endX, polygon->Points.back().y });
			}
			pathBuilder.AddLine({ endX, height });
			pathBuilder.EndFigure(CanvasFigureLoop::Open);

			auto const geometry = CanvasGeometry::CreatePath(pathBuilder);
			pathBuilder.Close();

			auto brushIterator = m_polygonBrushes.find(polygon->Key);
			if (brushIterator == m_polygonBrushes.end() || !brushIterator->second)
			{
				continue;
			}

			auto brushData = get_self<GraphBrushData>(brushIterator->second);
			if (brushData->IsDisposed())
			{
				continue;
			}

			try
			{
				auto const brush = brushData->Brush();
				auto const opacityBrush = brushData->OpacityBrush();
				auto const borderBrush = brushData->BorderBrush();

				if (brush && opacityBrush)
				{
					drawingSession.FillGeometry(geometry, brush, opacityBrush);
				}
				else if (brush)
				{
					drawingSession.FillGeometry(geometry, brush);
				}

				if (borderBrush)
				{
					auto const strokeStyle = brushData->StrokeStyle();
					if (strokeStyle)
					{
						drawingSession.DrawGeometry(
							geometry,
							borderBrush,
							brushData->StrokeWidth(),
							strokeStyle);
					}
					else
					{
						drawingSession.DrawGeometry(
							geometry,
							borderBrush,
							brushData->StrokeWidth());
					}
				}
			}
			catch (hresult_error const&)
			{
				// Device loss may invalidate a resource between snapshot and draw.
				continue;
			}
		}
	}

	void LiveGraph::OnDraw(
		ICanvasAnimatedControl const& sender,
		CanvasAnimatedDrawEventArgs const& args)
	{
		auto const eventArgs = winrt::make<LiveGraphEventArgs>(sender, args);
		m_draw(*this, eventArgs);

		auto const drawingSession = args.DrawingSession();
		auto const size = sender.Size();
		auto const width = static_cast<float>(size.Width);
		auto const height = static_cast<float>(size.Height);
		auto const timing = args.Timing();
		auto elapsedSeconds = std::chrono::duration<double>(
			timing.ElapsedTime).count();
		if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0)
		{
			elapsedSeconds = 0.0;
		}
		// Do not let a debugger break, device recovery, or a resumed window
		// advance the graph by an arbitrarily large distance in one frame.
		elapsedSeconds = std::min(elapsedSeconds, 0.25);
		std::scoped_lock lock(m_graphMutex);
		auto const scrollDelta = static_cast<float>(
			m_horizontalScrollSpeed * elapsedSeconds);

		if (std::isfinite(scrollDelta))
		{
			m_backgroundScrollPosition +=
				static_cast<double>(scrollDelta);
		}
		if (!std::isfinite(m_backgroundScrollPosition))
		{
			m_backgroundScrollPosition = 0.0;
		}

		DrawBackground(drawingSession, width, height);
		UpdatePolygonsOffset(scrollDelta, width);
		DrawUserPolygons(drawingSession, width, height);
		UpdateHighlightLine();
	}

	void LiveGraph::DrawBackground(
		CanvasDrawingSession const& drawingSession,
		float width,
		float height)
	{
		auto const calculateOffset =
			[this](float spacing)
		{
			if (!std::isfinite(spacing) || spacing <= 0.0f)
			{
				return 0.0f;
			}

			auto phase = std::fmod(
				m_backgroundScrollPosition,
				static_cast<double>(spacing));
			if (phase < 0.0)
			{
				phase += spacing;
			}
			return -static_cast<float>(phase);
		};

		switch (m_backgroundMode)
		{
			case OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode::None:
				break;

			case OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode::Cross:
			{
				if (!std::isfinite(m_crossSpacing) || m_crossSpacing <= 0.0f)
				{
					break;
				}

				auto const offsetX = calculateOffset(m_crossSpacing);
				for (auto x = offsetX; x < width; x += m_crossSpacing)
				{
					drawingSession.DrawLine(x, 0.0f, x, height, m_drawColor, 1.0f);
				}
				for (auto y = 0.0f; y < height; y += m_crossSpacing)
				{
					for (auto x = offsetX; x < width; x += m_crossSpacing)
					{
						drawingSession.DrawLine(
							x,
							y,
							x + m_crossSpacing,
							y,
							m_drawColor,
							1.0f);
					}
				}
				break;
			}

			case OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode::Dot:
			{
				if (!std::isfinite(m_dotSpacing) || m_dotSpacing <= 0.0f)
				{
					break;
				}

				auto const offsetX = calculateOffset(m_dotSpacing);
				for (auto y = 0.0f; y < height; y += m_dotSpacing)
				{
					for (auto x = offsetX; x < width; x += m_dotSpacing)
					{
						drawingSession.FillCircle(x, y, 1.0f, m_drawColor);
					}
				}
				break;
			}
		}
	}

	void LiveGraph::UpdatePolygonsOffset(float scrollDelta, float canvasWidth)
	{
		if (!std::isfinite(scrollDelta))
		{
			return;
		}
		for (auto iterator = m_polygons.begin(); iterator != m_polygons.end();)
		{
			auto const& polygon = *iterator;
			polygon->OffsetX -= scrollDelta;

			auto const liveIterator = m_livePolygons.find(polygon->Key);
			auto const isLive =
				liveIterator != m_livePolygons.end()
				&& liveIterator->second == polygon;
			if (isLive && polygon->Points.size() > 2)
			{
				auto const retentionBoundary =
					-std::max(0.0f, canvasWidth)
					* m_historyBufferScreens;
				std::size_t removeCount{};
				while (removeCount + 2 < polygon->Points.size()
					   && polygon->Points[removeCount + 1].x
					   + polygon->OffsetX < retentionBoundary)
				{
					++removeCount;
				}
				if (polygon->Points.size() > MaxDynamicPointCount)
				{
					removeCount = std::max(
						removeCount,
						polygon->Points.size() - MaxDynamicPointCount);
				}
				if (removeCount > 0)
				{
					auto const polygonIndex = static_cast<std::size_t>(
						std::distance(m_polygons.begin(), iterator));
					polygon->Points.erase(
						polygon->Points.begin(),
						polygon->Points.begin()
						+ static_cast<std::ptrdiff_t>(removeCount));
					if (m_currentPolygonIndex == polygonIndex)
					{
						m_currentPointIndex =
							m_currentPointIndex > removeCount
							? m_currentPointIndex - removeCount
							: 0;
					}
				}
			}

			if (!polygon->Points.empty() &&
				polygon->Points.back().x + polygon->OffsetX < 0.0f)
			{
				if (isLive)
				{
					m_livePolygons.erase(liveIterator);
				}

				iterator = m_polygons.erase(iterator);
				m_currentPolygonIndex = 0;
				m_currentPointIndex = 0;
				m_currentLineY.reset();
				m_lastHighlightedRevision = 0;
			}
			else
			{
				++iterator;
			}
		}
	}

	event_token LiveGraph::HighlightLineUpdated(
		EventHandler<float> const& handler)
	{
		return m_highlightLineUpdated.add(handler);
	}

	void LiveGraph::HighlightLineUpdated(event_token const& token) noexcept
	{
		m_highlightLineUpdated.remove(token);
	}

	event_token LiveGraph::Draw(
		EventHandler<OpenNet::UI::Xaml::Control::Graph::LiveGraphEventArgs> const& handler)
	{
		return m_draw.add(handler);
	}

	void LiveGraph::Draw(event_token const& token) noexcept
	{
		m_draw.remove(token);
	}

	event_token LiveGraph::CreateResources(
		EventHandler<CanvasAnimatedControl> const& handler)
	{
		return m_createResources.add(handler);
	}

	void LiveGraph::CreateResources(event_token const& token) noexcept
	{
		m_createResources.remove(token);
	}
}
