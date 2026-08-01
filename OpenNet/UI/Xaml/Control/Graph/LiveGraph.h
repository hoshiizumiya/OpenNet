#pragma once

#include "UI/Xaml/Control/Graph/GraphPoint.g.h"
#include "UI/Xaml/Control/Graph/GraphBrushData.g.h"
#include "UI/Xaml/Control/Graph/LiveGraphEventArgs.g.h"
#include "UI/Xaml/Control/Graph/LiveGraph.g.h"

import std;
import OpenNet.Helpers.EnsureDependencyProperties;
import OpenNet.Helpers.TemplateControlHelper;
import winrt.Microsoft.Graphics.Canvas;
import winrt.Microsoft.Graphics.Canvas.Brushes;
import winrt.Microsoft.Graphics.Canvas.Geometry;
import winrt.Microsoft.Graphics.Canvas.UI;
import winrt.Microsoft.Graphics.Canvas.UI.Xaml;
import winrt.Microsoft.UI.Composition;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Hosting;
import winrt.Windows.Foundation;
import winrt.Windows.Foundation.Collections;
import winrt.Windows.Foundation.Numerics;
import winrt.Windows.UI;

namespace winrt::OpenNet::UI::Xaml::Control::Graph::implementation
{
	struct GraphPoint : GraphPointT<GraphPoint>
	{
		GraphPoint() = default;
		GraphPoint(float value);
		GraphPoint(float value, float space);

		float Value() const;
		void Value(float value);

		float Space() const;
		void Space(float value);

	private:
		float m_value{};
		float m_space{};
	};

	struct GraphBrushData : GraphBrushDataT<GraphBrushData>
	{
		GraphBrushData() = default;
		GraphBrushData(
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& brush);
		GraphBrushData(
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& brush,
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& opacityBrush);
		GraphBrushData(
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& brush,
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& opacityBrush,
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& borderBrush);
		GraphBrushData(
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& brush,
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& opacityBrush,
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& borderBrush,
			float strokeWidth);
		GraphBrushData(
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& brush,
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& opacityBrush,
			winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& borderBrush,
			float strokeWidth,
			winrt::Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle const& strokeStyle);

		winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush Brush() const;
		void Brush(winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& value);

		winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush OpacityBrush() const;
		void OpacityBrush(winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& value);

		winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush BorderBrush() const;
		void BorderBrush(winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush const& value);

		float StrokeWidth() const;
		void StrokeWidth(float value);

		winrt::Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle StrokeStyle() const;
		void StrokeStyle(
			winrt::Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle const& value);

		bool IsDisposed() const noexcept;
		void Dispose() noexcept;

	private:
		winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush m_brush{ nullptr };
		winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush m_opacityBrush{ nullptr };
		winrt::Microsoft::Graphics::Canvas::Brushes::ICanvasBrush m_borderBrush{ nullptr };
		winrt::Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle m_strokeStyle{ nullptr };
		float m_strokeWidth{};
		bool m_isDisposed{};
	};

	struct LiveGraphEventArgs : LiveGraphEventArgsT<LiveGraphEventArgs>
	{
		LiveGraphEventArgs(
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::ICanvasAnimatedControl const& canvasAnimatedControl,
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedDrawEventArgs const& drawEventArgs);

		winrt::Microsoft::Graphics::Canvas::UI::Xaml::ICanvasAnimatedControl
			CanvasAnimatedControl() const;
		winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedDrawEventArgs
			DrawEventArgs() const;

	private:
		winrt::Microsoft::Graphics::Canvas::UI::Xaml::ICanvasAnimatedControl
			m_canvasAnimatedControl{ nullptr };
		winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedDrawEventArgs
			m_drawEventArgs{ nullptr };
	};

	struct LiveGraph :
		LiveGraphT<LiveGraph>,
		TemplateControlHelper<LiveGraph>,
		EnsureDependencyProperty<LiveGraph>
	{
		using LiveGraphT<LiveGraph>::DefaultStyleKey;

		LiveGraph() = default;

		static void EnsureDependencyProperties();
		void OnApplyTemplate();

		static winrt::Microsoft::UI::Xaml::DependencyProperty HighlightLineBehaviorProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty HighlightLineVisibilityProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty BackgroundModeProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty DotSpacingProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty CrossSpacingProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty BackgroundColorProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty ClearColorProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty HorizontalScrollDistanceProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty HorizontalScrollDurationProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty HighlightLineAnimationDurationProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty HistoryBufferScreensProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty HighlightLineContentProperty();

		OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior HighlightLineBehavior();
		void HighlightLineBehavior(
			OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior value);

		winrt::Microsoft::UI::Xaml::Visibility HighlightLineVisibility();
		void HighlightLineVisibility(winrt::Microsoft::UI::Xaml::Visibility value);

		OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode BackgroundMode();
		void BackgroundMode(
			OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode value);

		double DotSpacing();
		void DotSpacing(double value);

		double CrossSpacing();
		void CrossSpacing(double value);

		winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> BackgroundColor();
		void BackgroundColor(
			winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> const& value);

		winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> ClearColor();
		void ClearColor(
			winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> const& value);

		double HorizontalScrollDistance();
		void HorizontalScrollDistance(double value);

		winrt::Windows::Foundation::TimeSpan HorizontalScrollDuration();
		void HorizontalScrollDuration(winrt::Windows::Foundation::TimeSpan const& value);

		winrt::Windows::Foundation::TimeSpan HighlightLineAnimationDuration();
		void HighlightLineAnimationDuration(winrt::Windows::Foundation::TimeSpan const& value);

		double HistoryBufferScreens();
		void HistoryBufferScreens(double value);

		winrt::Windows::Foundation::IInspectable HighlightLineContent();
		void HighlightLineContent(winrt::Windows::Foundation::IInspectable const& value);

		winrt::hstring RegisterGraphBrush(
			OpenNet::UI::Xaml::Control::Graph::GraphBrushData const& brushData);
		void UpdateGraphBrush(
			winrt::hstring const& key,
			OpenNet::UI::Xaml::Control::Graph::GraphBrushData const& brushData);
		void ResetDynamicGraph(winrt::hstring const& key);

		void AddDynamicPoint(
			winrt::hstring const& key,
			OpenNet::UI::Xaml::Control::Graph::GraphPoint const& point);
		void AddDynamicPoint(
			winrt::hstring const& key,
			OpenNet::UI::Xaml::Control::Graph::GraphPoint const& point,
			bool isRounded);
		void AddStaticPoints(
			winrt::hstring const& key,
			winrt::Windows::Foundation::Collections::IIterable<
			OpenNet::UI::Xaml::Control::Graph::GraphPoint> const& points);
		void AddStaticPoints(
			winrt::hstring const& key,
			winrt::Windows::Foundation::Collections::IIterable<
			OpenNet::UI::Xaml::Control::Graph::GraphPoint> const& points,
			bool isRounded);

		OpenNet::UI::Xaml::Control::Graph::GraphBrushData GetCustomBrush(
			winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreator const& resourceCreator,
			winrt::array_view<winrt::Windows::UI::Color const> colors);
		OpenNet::UI::Xaml::Control::Graph::GraphBrushData GetGreenBrush(
			winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreator const& resourceCreator);
		OpenNet::UI::Xaml::Control::Graph::GraphBrushData GetRedBrush(
			winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreator const& resourceCreator);
		OpenNet::UI::Xaml::Control::Graph::GraphBrushData GetBlueBrush(
			winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreator const& resourceCreator);
		OpenNet::UI::Xaml::Control::Graph::GraphBrushData GetPurpleBrush(
			winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreator const& resourceCreator);

		winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl
			GetCanvasAnimatedControl();

		winrt::event_token HighlightLineUpdated(
			winrt::Windows::Foundation::EventHandler<float> const& handler);
		void HighlightLineUpdated(winrt::event_token const& token) noexcept;

		winrt::event_token Draw(
			winrt::Windows::Foundation::EventHandler<
			OpenNet::UI::Xaml::Control::Graph::LiveGraphEventArgs> const& handler);
		void Draw(winrt::event_token const& token) noexcept;

		winrt::event_token CreateResources(
			winrt::Windows::Foundation::EventHandler<
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl> const& handler);
		void CreateResources(winrt::event_token const& token) noexcept;

		constexpr static auto ResourceUri =
			L"ms-appx:///UI/Xaml/Control/Graph/LiveGraph_ResourceDictionary.xaml";

	private:
		struct UserPolygon
		{
			std::vector<winrt::Windows::Foundation::Numerics::float2> Points;
			float OffsetX{};
			float CurrentY{};
			winrt::hstring Key;
			bool IsRounded{};
		};

		static void OnHighlightLineBehaviorChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnHighlightLineVisibilityChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnBackgroundModeChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnDotSpacingChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnCrossSpacingChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnBackgroundColorChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnClearColorChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnHorizontalScrollChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnHighlightLineAnimationDurationChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnHistoryBufferScreensChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		static void OnHighlightLineContentChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& dependencyObject,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);

		void DetachCanvasHandlers();
		void OnActualThemeChanged(
			winrt::Microsoft::UI::Xaml::FrameworkElement const& sender,
			winrt::Windows::Foundation::IInspectable const& args);
		void OnSizeChanged(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
		void OnUnloaded(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OnCreateResources(
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl const& sender,
			winrt::Microsoft::Graphics::Canvas::UI::CanvasCreateResourcesEventArgs const& args);
		void OnDraw(
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::ICanvasAnimatedControl const& sender,
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedDrawEventArgs const& args);

		void UpdateDrawColor();
		void UpdateClearColor();
		void UpdateHorizontalScrollSpeed();
		void ResizeGraphPoints(
			float newWidth,
			float newHeight,
			float oldWidth,
			float oldHeight);
		void UpdateHighlightLine();
		void MoveHostGridWithAnimation(float y);
		void DrawUserPolygons(
			winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& drawingSession,
			float width,
			float height);
		void DrawBackground(
			winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& drawingSession,
			float width,
			float height);
		void UpdatePolygonsOffset(float scrollDelta, float canvasWidth);

		static float NormalizeY(float percent, float height);
		static float CalculateSpeed(
			float distance,
			winrt::Windows::Foundation::TimeSpan const& time);

		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_highlightLineBehaviorProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_highlightLineVisibilityProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_backgroundModeProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_dotSpacingProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_crossSpacingProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_backgroundColorProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_clearColorProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_horizontalScrollDistanceProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_horizontalScrollDurationProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_highlightLineAnimationDurationProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_historyBufferScreensProperty{ nullptr };
		static inline winrt::Microsoft::UI::Xaml::DependencyProperty
			s_highlightLineContentProperty{ nullptr };

		winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl m_canvas{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::Grid m_hostGrid{ nullptr };
		winrt::Microsoft::UI::Composition::Compositor m_compositor{ nullptr };
		winrt::Microsoft::UI::Composition::Visual m_gridVisual{ nullptr };

		std::vector<std::shared_ptr<UserPolygon>> m_polygons;
		std::unordered_map<winrt::hstring, std::shared_ptr<UserPolygon>> m_livePolygons;
		std::unordered_map<
			winrt::hstring,
			OpenNet::UI::Xaml::Control::Graph::GraphBrushData> m_polygonBrushes;
		std::mutex m_graphMutex;

		winrt::Windows::UI::Color m_drawColor{};
		winrt::Windows::UI::Color m_clearColor{};
		double m_backgroundScrollPosition{};
		float m_horizontalScrollSpeed{ 1.0f };
		float m_crossSpacing{ 30.0f };
		float m_dotSpacing{ 6.0f };
		float m_historyBufferScreens{ 1.0f };
		std::optional<float> m_currentLineY;
		winrt::Windows::Foundation::TimeSpan m_highlightLineAnimationDuration{
			std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(
				std::chrono::milliseconds{ 300 }) };
		OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior
			m_highlightLineBehavior{
				OpenNet::UI::Xaml::Control::Graph::HighlightLineBehavior::EachPoint };
		OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode
			m_backgroundMode{
				OpenNet::UI::Xaml::Control::Graph::LiveGraphBackgroundMode::Cross };
		winrt::Microsoft::UI::Xaml::Visibility
			m_highlightLineVisibility{ winrt::Microsoft::UI::Xaml::Visibility::Visible };
		winrt::Windows::Foundation::IInspectable m_highlightLineContent{ nullptr };
		size_t m_currentPolygonIndex{};
		size_t m_currentPointIndex{};

		winrt::event_token m_canvasDrawToken{};
		winrt::event_token m_canvasCreateResourcesToken{};
		winrt::event_token m_actualThemeChangedToken{};
		winrt::event_token m_sizeChangedToken{};
		winrt::event_token m_unloadedToken{};

		winrt::event<winrt::Windows::Foundation::EventHandler<float>>
			m_highlightLineUpdated;
		winrt::event<winrt::Windows::Foundation::EventHandler<
			OpenNet::UI::Xaml::Control::Graph::LiveGraphEventArgs>>
			m_draw;
		winrt::event<winrt::Windows::Foundation::EventHandler<
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl>>
			m_createResources;

		constexpr static auto CanvasPartName = L"PART_Canvas";
		constexpr static auto HostGridPartName = L"PART_HostGrid";
		constexpr static std::size_t MaxDynamicPointCount = 8192;
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::Graph::factory_implementation
{
	struct GraphPoint : GraphPointT<GraphPoint, implementation::GraphPoint>
	{
	};

	struct GraphBrushData : GraphBrushDataT<GraphBrushData, implementation::GraphBrushData>
	{
	};

	struct LiveGraphEventArgs :
		LiveGraphEventArgsT<LiveGraphEventArgs, implementation::LiveGraphEventArgs>
	{
	};

	struct LiveGraph : LiveGraphT<LiveGraph, implementation::LiveGraph>
	{
	};
}
