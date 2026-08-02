#pragma once

// workaround for xamlc generated .xaml.g.h
import winrt.Microsoft.UI.Xaml.Shapes;

#include "Controls/SpeedGraph/SpeedGraph.g.h"
#include "Core/DataGraph/SpeedGraphData.h"
#include <ViewModels/ViewModelLocator.h>

import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Markup;
import winrt.Microsoft.UI.Xaml.Controls.Primitives;
import winrt.Windows.UI;
import winrt.Microsoft.Graphics.Canvas.Brushes;
import winrt.Microsoft.Graphics.Canvas.Geometry;
import winrt.Microsoft.Graphics.Canvas.UI;
import winrt.Microsoft.Graphics.Canvas.UI.Xaml;

namespace winrt::OpenNet::Controls::SpeedGraph::implementation
{
	struct SpeedGraph : SpeedGraphT<SpeedGraph>
	{
		SpeedGraph() = default;

		void SetSpeed(double percent, uint64_t speed);

		void Pause();
		void Error();

		// Reset the graph data (clears existing data)
		void Reset();

		winrt::Microsoft::UI::Xaml::Media::PointCollection Points();
	private:
		// Each SpeedGraph instance now owns its own SpeedGraphData
		SpeedGraphData m_graphData{};

		bool m_hasData{};
		double m_pendingPercent{};
		std::uint64_t m_pendingSpeed{};
		bool m_hasPendingSample{};

		/**
		 * @brief Recalculate graph point because of the speed scale changed
		 */
		void resizeGraphPoint(float ratio);



		/**
		 * @brief Make animation when the scale is changed
		 */
		void makeAnimation();

		void makeAnimation(float y);
		constexpr static auto BackgroundCircleDistance = 6;

		constexpr static winrt::Microsoft::UI::Xaml::Duration speedLineAndTextAnimationDuration
		{
			.TimeSpan = std::chrono::milliseconds{300},
			.Type = winrt::Microsoft::UI::Xaml::DurationType::TimeSpan
		};
	public:
		void CanvasControl_Draw(winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& sender, winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasDrawEventArgs const& args);
		void UserControl_SizeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);
		void CanvasControl_ActualThemeChanged(winrt::Microsoft::UI::Xaml::FrameworkElement const& sender, winrt::Windows::Foundation::IInspectable const& args);
	};
}

namespace winrt::OpenNet::Controls::SpeedGraph::factory_implementation
{
	struct SpeedGraph : SpeedGraphT<SpeedGraph, implementation::SpeedGraph>
	{
	};
}
