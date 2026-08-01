#pragma once

import winrt.OpenNet.UI.Xaml.Control.Graph;
#include "UI/Xaml/View/Pages/TaskSpeedGraphPage.g.h"
#include "ViewModels/TaskSpeedGraphSettingsViewModel.h"

import winrt.Microsoft.Graphics.Canvas.UI.Xaml;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Windows.Foundation;
import std;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TaskSpeedGraphPage : TaskSpeedGraphPageT<TaskSpeedGraphPage>
	{
		TaskSpeedGraphPage();
		~TaskSpeedGraphPage();

		winrt::OpenNet::ViewModels::TaskSpeedGraphSettingsViewModel Settings() const;

		void MetricSelector_SelectionChanged(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
		void PerformanceGraph_CreateResources(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl const& canvas);
		void PerformanceGraph_Draw(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::OpenNet::UI::Xaml::Control::Graph::LiveGraphEventArgs const& args);
		void PerformanceGraph_HighlightLineUpdated(
			winrt::Windows::Foundation::IInspectable const& sender,
			float value);
		void ResetGraphSettings_Click(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		enum class MetricMode
		{
			TransferSpeed,
			DiskCache,
			LongTermSeeding,
			CpuOverall,
			CpuLogicalProcessors,
			MemoryUsage,
			DhtUdp,
		};

		struct ProcessorTimes
		{
			std::uint64_t Idle{};
			std::uint64_t Kernel{};
			std::uint64_t User{};
		};

		void RecreateGraphStreams(
			winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasAnimatedControl const& canvas);
		void ApplyGraphSettings();
		void ApplyBrushStrokeWidth();
		void OnSettingsPropertyChanged(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args);
		void DisposeBrushes() noexcept;
		std::size_t RequiredSeriesCount() const;
		std::vector<float> SampleProcessorUsage();
		std::pair<std::vector<double>, std::vector<winrt::hstring>> SampleMetric();
		float NormalizeValue(double value, double maximum);
		void QueueMetricText(
			std::vector<winrt::hstring> const& values,
			winrt::hstring const& scaleText);
		static winrt::hstring FormatBytes(double value);
		static winrt::hstring FormatRate(double value);

		bool m_isInitialized{};
		winrt::OpenNet::ViewModels::TaskSpeedGraphSettingsViewModel
			m_settings{ nullptr };
		winrt::event_token m_settingsPropertyChangedToken{};
		std::atomic<MetricMode> m_mode{ MetricMode::TransferSpeed };
		std::mutex m_graphStateMutex;
		std::mutex m_sampleStateMutex;
		std::vector<winrt::hstring> m_graphKeys;
		std::vector<winrt::OpenNet::UI::Xaml::Control::Graph::GraphBrushData> m_brushes;
		std::vector<ProcessorTimes> m_previousProcessorTimes;
		std::int64_t m_previousDhtReceived{};
		std::int64_t m_previousDhtSent{};
		std::chrono::steady_clock::time_point m_previousDhtSample{};
		double m_lastDhtReceivedRate{};
		double m_lastDhtSentRate{};
		std::atomic<double> m_dynamicScale{ 1024.0 };
		std::atomic<double> m_sampleElapsedSeconds{};
		std::atomic<double> m_graphSampleIntervalSeconds{ 0.1 };
		std::atomic<double> m_graphScrollPixelsPerSecond{ 60.0 };
		std::atomic<float> m_graphStrokeWidth{ 2.0f };
		std::atomic_bool m_smoothCurves{ true };
		std::atomic_bool m_fillEnabled{ true };
		std::atomic_bool m_borderEnabled{ true };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TaskSpeedGraphPage :
		TaskSpeedGraphPageT<TaskSpeedGraphPage, implementation::TaskSpeedGraphPage>
	{
	};
}
