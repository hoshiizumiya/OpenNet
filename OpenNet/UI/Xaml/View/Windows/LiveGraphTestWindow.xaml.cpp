#include <Windows.h>

#include "XamlWorkaround.h"
#include "LiveGraphTestWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/LiveGraphTestWindow.g.cpp")
#include "UI/Xaml/View/Windows/LiveGraphTestWindow.g.cpp"
#endif

import winrt.Microsoft.Graphics.Canvas.Brushes;
import winrt.Microsoft.Graphics.Canvas.Geometry;
import winrt.Microsoft.Graphics.Canvas.UI.Xaml;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.OpenNet.UI.Xaml.Control.Graph;
import OpenNet.Core.Utils.Message;
import winrt.Windows.Foundation;
import winrt.Windows.Foundation.Collections;
import winrt.Windows.Foundation.Numerics;
import winrt.Windows.UI;

using namespace winrt;
using namespace winrt::Microsoft::Graphics::Canvas::Brushes;
using namespace winrt::Microsoft::Graphics::Canvas::Geometry;
using namespace winrt::Microsoft::Graphics::Canvas::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::OpenNet::UI::Xaml::Control::Graph;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	namespace
	{
		float ClampPercent(float value)
		{
			return std::clamp(value, 0.0f, 100.0f);
		}

		IReference<Color> BoxColor(Color const& color)
		{
			return box_value(color).as<IReference<Color>>();
		}
	}

	LiveGraphTestWindow::LiveGraphTestWindow()
	{
		InitializeComponent();
		InitializeWindowExBase();
		ExtendsContentIntoTitleBar(true);
		m_uiReady = true;
		ApplyGraphSettings();
	}

	void LiveGraphTestWindow::DynamicGraph_CreateResources(IInspectable const&, CanvasAnimatedControl const& canvas)
	{
		RecreateDynamicStreams(canvas);
		ResourceStatusText().Text(ResourceGetString(L"ViewLiveGraphTestWindowDynamicResourcesReady"));
	}

	void LiveGraphTestWindow::StaticGraph_CreateResources(IInspectable const&, CanvasAnimatedControl const& canvas)
	{
		auto graph = StaticGraph();
		if (!graph)
		{
			return;
		}

		if (!m_staticGraphKey.empty())
		{
			graph.ResetDynamicGraph(m_staticGraphKey);
		}

		m_staticBrush = graph.GetPurpleBrush(canvas);
		UpdateGradientBounds(
			m_staticBrush,
			static_cast<float>(graph.ActualHeight()));
		m_staticGraphKey = graph.RegisterGraphBrush(m_staticBrush);
		AddStaticBatch(true);
		ResourceStatusText().Text(ResourceGetString(L"ViewLiveGraphTestWindowAllResourcesReady"));
	}

	void LiveGraphTestWindow::RecreateDynamicStreams(CanvasAnimatedControl const& canvas)
	{
		auto graph = DynamicGraph();
		if (!graph || !canvas)
		{
			return;
		}

		if (!m_primaryGraphKey.empty())
		{
			graph.ResetDynamicGraph(m_primaryGraphKey);
		}
		if (!m_secondaryGraphKey.empty())
		{
			graph.ResetDynamicGraph(m_secondaryGraphKey);
		}

		m_primaryBrush = graph.GetGreenBrush(canvas);
		m_secondaryBrush = graph.GetBlueBrush(canvas);
		auto const height = static_cast<float>(graph.ActualHeight());
		UpdateGradientBounds(m_primaryBrush, height);
		UpdateGradientBounds(m_secondaryBrush, height);

		m_primaryGraphKey = graph.RegisterGraphBrush(m_primaryBrush);
		m_secondaryGraphKey = graph.RegisterGraphBrush(m_secondaryBrush);
		m_drawFrame = 0;
		m_phase = 0.0f;
		m_dynamicSampleElapsed = 0.0;
		m_dynamicStatusElapsed = 0.0;
		ApplyBrushAppearance();
	}

	void LiveGraphTestWindow::DynamicGraph_Draw(IInspectable const&, LiveGraphEventArgs const& args)
	{
		if (m_primaryGraphKey.empty() || m_secondaryGraphKey.empty())
		{
			return;
		}

		++m_drawFrame;
		auto frameSeconds = std::chrono::duration<double>(
			args.DrawEventArgs().Timing().ElapsedTime).count();
		if (!std::isfinite(frameSeconds) || frameSeconds < 0.0)
		{
			frameSeconds = 0.0;
		}
		frameSeconds = std::min(frameSeconds, 0.25);
		m_dynamicSampleElapsed += frameSeconds;
		constexpr double SampleIntervalSeconds = 1.0 / 12.0;
		if (m_dynamicSampleElapsed < SampleIntervalSeconds)
		{
			return;
		}
		auto const sampleElapsed = m_dynamicSampleElapsed;
		m_dynamicSampleElapsed = 0.0;

		m_phase += static_cast<float>(1.92 * sampleElapsed);
		auto const cpuValue = ClampPercent(
			52.0f +
			32.0f * std::sin(m_phase) +
			9.0f * std::sin(m_phase * 2.7f));
		auto const networkValue = ClampPercent(
			35.0f +
			24.0f * std::sin(m_phase * 0.63f + 1.2f) +
			18.0f * std::abs(std::sin(m_phase * 1.8f)));
		auto const pointSpace = static_cast<float>(std::max(
			0.001,
			m_dynamicScrollPixelsPerSecond.load(
				std::memory_order_relaxed) * sampleElapsed));

		auto graph = DynamicGraph();
		graph.AddDynamicPoint(
			m_primaryGraphKey,
			GraphPoint{ cpuValue, pointSpace },
			true);
		graph.AddDynamicPoint(
			m_secondaryGraphKey,
			GraphPoint{ networkValue, pointSpace },
			false);

		m_dynamicStatusElapsed += sampleElapsed;
		if (m_dynamicStatusElapsed >= 1.0)
		{
			m_dynamicStatusElapsed = 0.0;
			auto weak = get_weak();
			DispatcherQueue().TryEnqueue(
				[weak, frame = m_drawFrame, cpuValue, networkValue]
			{
				if (auto self = weak.get())
				{
					self->DynamicFrameStatusText().Text(
						ResourceGetString(L"ViewLiveGraphTestWindowFrame") + L" " +
						std::to_wstring(frame) + L" · " +
						ResourceGetString(L"ViewLiveGraphTestWindowCPU") + L" " +
						std::format(L"{:.1f}%", cpuValue) + L" · " +
						ResourceGetString(L"ViewLiveGraphTestWindowNetwork") + L" " +
						std::format(L"{:.1f}%", networkValue));
				}
			});
		}
	}

	void LiveGraphTestWindow::DynamicGraph_HighlightLineUpdated(IInspectable const&, float value)
	{
		auto const text = ResourceGetString(L"ViewLiveGraphTestWindowDynamicHighlightY") + L" " + std::format(L"{:.1f}px", value);
		DynamicHighlightText().Text(text);
		HighlightStatusText().Text(text);
	}

	void LiveGraphTestWindow::StaticGraph_HighlightLineUpdated(IInspectable const&, float value)
	{
		StaticHighlightText().Text(ResourceGetString(L"ViewLiveGraphTestWindowStaticY") + L" " + std::format(L"{:.1f}px", value));
	}

	void LiveGraphTestWindow::ResetDynamicGraph_Click(IInspectable const&, RoutedEventArgs const&)
	{
		auto graph = DynamicGraph();
		if (!graph)
		{
			return;
		}

		auto canvas = graph.GetCanvasAnimatedControl();
		if (canvas)
		{
			RecreateDynamicStreams(canvas);
			DynamicFrameStatusText().Text(ResourceGetString(L"ViewLiveGraphTestWindowDynamicStreamsReset"));
		}
	}

	void LiveGraphTestWindow::AddStaticLine_Click(IInspectable const&, RoutedEventArgs const&)
	{
		AddStaticBatch(false);
	}

	void LiveGraphTestWindow::AddStaticRounded_Click(IInspectable const&, RoutedEventArgs const&)
	{
		AddStaticBatch(true);
	}

	void LiveGraphTestWindow::AddStaticBatch(bool rounded)
	{
		auto graph = StaticGraph();
		if (!graph || m_staticGraphKey.empty())
		{
			return;
		}

		auto points = single_threaded_vector<GraphPoint>();
		constexpr std::size_t PointCount = 54;
		for (std::size_t index = 0; index < PointCount; ++index)
		{
			auto const position = static_cast<float>(index);
			auto const value = ClampPercent(
				48.0f +
				28.0f * std::sin(position * 0.31f + m_phase) +
				12.0f * std::cos(position * 0.73f));
			points.Append(GraphPoint{ value, 9.0f });
		}

		graph.AddStaticPoints(m_staticGraphKey, points, rounded);
	}

	void LiveGraphTestWindow::GreenBrush_Click(IInspectable const&, RoutedEventArgs const&)
	{
		auto graph = DynamicGraph();
		auto canvas = graph ? graph.GetCanvasAnimatedControl() : nullptr;
		if (canvas)
		{
			SetPrimaryBrush(graph.GetGreenBrush(canvas));
		}
	}

	void LiveGraphTestWindow::BlueBrush_Click(IInspectable const&, RoutedEventArgs const&)
	{
		auto graph = DynamicGraph();
		auto canvas = graph ? graph.GetCanvasAnimatedControl() : nullptr;
		if (canvas)
		{
			SetPrimaryBrush(graph.GetBlueBrush(canvas));
		}
	}

	void LiveGraphTestWindow::RedBrush_Click(IInspectable const&, RoutedEventArgs const&)
	{
		auto graph = DynamicGraph();
		auto canvas = graph ? graph.GetCanvasAnimatedControl() : nullptr;
		if (canvas)
		{
			SetPrimaryBrush(graph.GetRedBrush(canvas));
		}
	}

	void LiveGraphTestWindow::PurpleBrush_Click(IInspectable const&, RoutedEventArgs const&)
	{
		auto graph = DynamicGraph();
		auto canvas = graph ? graph.GetCanvasAnimatedControl() : nullptr;
		if (canvas)
		{
			SetPrimaryBrush(graph.GetPurpleBrush(canvas));
		}
	}

	void LiveGraphTestWindow::CustomBrush_Click(IInspectable const&, RoutedEventArgs const&)
	{
		auto graph = DynamicGraph();
		auto canvas = graph ? graph.GetCanvasAnimatedControl() : nullptr;
		if (!canvas)
		{
			return;
		}

		std::array colors
		{
			ColorHelper::FromArgb(150, 255, 196, 74),
			ColorHelper::FromArgb(205, 255, 92, 92),
			ColorHelper::FromArgb(235, 146, 64, 190)
		};
		SetPrimaryBrush(graph.GetCustomBrush(canvas, colors));
	}

	void LiveGraphTestWindow::SetPrimaryBrush(GraphBrushData const& brush)
	{
		if (!brush || m_primaryGraphKey.empty())
		{
			return;
		}

		auto previous = m_primaryBrush;
		m_primaryBrush = brush;
		UpdateGradientBounds(
			m_primaryBrush,
			static_cast<float>(DynamicGraph().ActualHeight()));
		ApplyBrushAppearance();
		DisposeBrush(previous);
	}

	void LiveGraphTestWindow::ApplyBrushAppearance()
	{
		if (!m_uiReady || !m_primaryBrush || !m_secondaryBrush)
		{
			return;
		}

		auto graph = DynamicGraph();
		if (!graph || m_primaryGraphKey.empty() || m_secondaryGraphKey.empty())
		{
			return;
		}

		auto const strokeValue = StrokeWidthBox().Value();
		auto const strokeWidth =
			std::isfinite(strokeValue)
			? static_cast<float>(strokeValue)
			: 2.0f;
		auto const showFill = ShowFillToggle().IsOn();
		auto const showBorder = ShowBorderToggle().IsOn();
		auto const dashed = DashedBorderToggle().IsOn();

		auto createData = [=](GraphBrushData const& source)
		{
			GraphBrushData data;
			if (showFill)
			{
				data.Brush(source.Brush());
				data.OpacityBrush(source.OpacityBrush());
			}
			if (showBorder)
			{
				data.BorderBrush(source.BorderBrush());
			}
			data.StrokeWidth(strokeWidth);
			if (showBorder && dashed)
			{
				CanvasStrokeStyle style;
				style.DashStyle(CanvasDashStyle::Dash);
				style.DashOffset(6.0f);
				data.StrokeStyle(style);
			}
			return data;
		};

		graph.UpdateGraphBrush(
			m_primaryGraphKey,
			createData(m_primaryBrush));
		graph.UpdateGraphBrush(
			m_secondaryGraphKey,
			createData(m_secondaryBrush));
	}

	void LiveGraphTestWindow::UpdateGradientBounds(GraphBrushData const& brush, float height)
	{
		if (!brush)
		{
			return;
		}

		if (auto gradient =
			brush.Brush().try_as<CanvasLinearGradientBrush>())
		{
			gradient.StartPoint(float2{ 0.0f, height });
			gradient.EndPoint(float2{ 0.0f, 0.0f });
		}
		if (auto opacityGradient =
			brush.OpacityBrush().try_as<CanvasLinearGradientBrush>())
		{
			opacityGradient.StartPoint(float2{ 0.0f, height });
			opacityGradient.EndPoint(float2{ 0.0f, 0.0f });
		}
	}

	void LiveGraphTestWindow::DisposeBrush(GraphBrushData const& brush) noexcept
	{
		// Releasing the GraphBrushData wrapper is sufficient. The animated draw
		// loop may still hold a native brush snapshot when this window navigates.
		(void)brush;
	}

	void LiveGraphTestWindow::BackgroundMode_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		ApplyGraphSettings();
	}

	void LiveGraphTestWindow::HighlightBehavior_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		ApplyGraphSettings();
	}

	void LiveGraphTestWindow::GraphSettings_ValueChanged(NumberBox const&, NumberBoxValueChangedEventArgs const&)
	{
		ApplyGraphSettings();
	}

	void LiveGraphTestWindow::BrushAppearance_ValueChanged(NumberBox const&, NumberBoxValueChangedEventArgs const&)
	{
		ApplyBrushAppearance();
	}

	void LiveGraphTestWindow::HighlightVisibility_Toggled(IInspectable const&, RoutedEventArgs const&)
	{
		ApplyGraphSettings();
	}

	void LiveGraphTestWindow::CustomColors_Toggled(IInspectable const&, RoutedEventArgs const&)
	{
		ApplyGraphSettings();
	}

	void LiveGraphTestWindow::BrushAppearance_Toggled(IInspectable const&, RoutedEventArgs const&)
	{
		ApplyBrushAppearance();
	}

	void LiveGraphTestWindow::ApplyGraphSettings()
	{
		if (!m_uiReady)
		{
			return;
		}

		auto dynamicGraph = DynamicGraph();
		auto staticGraph = StaticGraph();
		if (!dynamicGraph || !staticGraph)
		{
			return;
		}

		auto const backgroundIndex =
			std::clamp(BackgroundModeBox().SelectedIndex(), 0, 2);
		auto const highlightIndex =
			std::clamp(HighlightBehaviorBox().SelectedIndex(), 0, 4);
		auto const backgroundMode =
			static_cast<LiveGraphBackgroundMode>(backgroundIndex);
		auto const highlightBehavior =
			static_cast<HighlightLineBehavior>(highlightIndex);

		auto dotSpacing = DotSpacingBox().Value();
		auto crossSpacing = CrossSpacingBox().Value();
		auto scrollDistance = ScrollDistanceBox().Value();
		auto durationMilliseconds = ScrollDurationBox().Value();
		if (!std::isfinite(dotSpacing))
		{
			dotSpacing = 14.0;
		}
		if (!std::isfinite(crossSpacing))
		{
			crossSpacing = 32.0;
		}
		if (!std::isfinite(scrollDistance))
		{
			scrollDistance = 60.0;
		}
		if (!std::isfinite(durationMilliseconds))
		{
			durationMilliseconds = 1000.0;
		}
		m_dynamicScrollPixelsPerSecond.store(
			durationMilliseconds > 0.0
			? scrollDistance * 1000.0 / durationMilliseconds
			: 0.0,
			std::memory_order_relaxed);

		auto const duration = std::chrono::duration_cast<TimeSpan>(
			std::chrono::milliseconds{
				static_cast<std::int64_t>(durationMilliseconds) });
		auto const visibility =
			ShowHighlightToggle().IsOn()
			? Visibility::Visible
			: Visibility::Collapsed;

		for (auto const& graph : { dynamicGraph, staticGraph })
		{
			graph.BackgroundMode(backgroundMode);
			graph.DotSpacing(dotSpacing);
			graph.CrossSpacing(crossSpacing);
			graph.HorizontalScrollDistance(scrollDistance);
			graph.HorizontalScrollDuration(duration);
			graph.HighlightLineBehavior(highlightBehavior);
			graph.HighlightLineVisibility(visibility);

			if (UseCustomColorsToggle().IsOn())
			{
				graph.BackgroundColor(BoxColor(
					ColorHelper::FromArgb(70, 108, 99, 255)));
				graph.ClearColor(BoxColor(
					ColorHelper::FromArgb(255, 12, 18, 32)));
			}
			else
			{
				graph.BackgroundColor(nullptr);
				graph.ClearColor(BoxColor(
					ColorHelper::FromArgb(0, 0, 0, 0)));
			}
		}
	}
}

