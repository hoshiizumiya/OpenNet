#include <Windows.h>
#include <Psapi.h>

#include "XamlWorkaround.h"
#include "TaskSpeedGraphPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskSpeedGraphPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskSpeedGraphPage.g.cpp"
#endif

import OpenNet.Core.P2PManager;
import winrt.Microsoft.Graphics.Canvas;
import winrt.Microsoft.UI.Dispatching;
import winrt.Windows.Foundation.Collections;
import winrt.Windows.UI;

using namespace winrt;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Microsoft::Graphics::Canvas::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::OpenNet::UI::Xaml::Control::Graph;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	namespace
	{
		struct NativeProcessorPerformanceInformation
		{
			LARGE_INTEGER IdleTime;
			LARGE_INTEGER KernelTime;
			LARGE_INTEGER UserTime;
			LARGE_INTEGER DpcTime;
			LARGE_INTEGER InterruptTime;
			ULONG InterruptCount;
		};

		using NtQuerySystemInformationProc =
			LONG(NTAPI*)(ULONG, PVOID, ULONG, PULONG);

		constexpr ULONG SystemProcessorPerformanceInformation = 8;

		IReference<Color> BoxColor(Color const& color)
		{
			return box_value(color).as<IReference<Color>>();
		}

		void CloseResource(auto const& resource) noexcept
		{
			if (!resource)
			{
				return;
			}
			try
			{
				resource.Close();
			}
			catch (...)
			{
			}
		}

		Color SeriesColor(std::size_t index)
		{
			constexpr std::array<Color, 12> Colors
			{
				Color{ 255, 38, 166, 91 },
				Color{ 255, 0, 120, 212 },
				Color{ 255, 232, 91, 70 },
				Color{ 255, 135, 100, 184 },
				Color{ 255, 255, 185, 0 },
				Color{ 255, 0, 183, 195 },
				Color{ 255, 227, 0, 140 },
				Color{ 255, 74, 130, 5 },
				Color{ 255, 104, 118, 138 },
				Color{ 255, 142, 140, 216 },
				Color{ 255, 255, 140, 0 },
				Color{ 255, 0, 153, 188 },
			};
			return Colors[index % Colors.size()];
		}
	}

	TaskSpeedGraphPage::TaskSpeedGraphPage()
	{
		m_settings = winrt::make<
			winrt::OpenNet::ViewModels::implementation::
			TaskSpeedGraphSettingsViewModel>();
		m_settings.Initialize();
		InitializeComponent();
		m_isInitialized = true;
		m_settingsPropertyChangedToken = m_settings.PropertyChanged(
			[weak = get_weak()](
				IInspectable const& sender,
				winrt::Microsoft::UI::Xaml::Data::
				PropertyChangedEventArgs const& args)
		{
			if (auto self = weak.get())
			{
				self->OnSettingsPropertyChanged(sender, args);
			}
		});
		ApplyGraphSettings();
		if (auto const selector = MetricSelector();
			selector && selector.SelectedIndex() >= 0)
		{
			m_mode.store(
				static_cast<MetricMode>(selector.SelectedIndex()),
				std::memory_order_relaxed);
		}
	}

	TaskSpeedGraphPage::~TaskSpeedGraphPage()
	{
		if (m_settings && m_settingsPropertyChangedToken.value)
		{
			m_settings.PropertyChanged(m_settingsPropertyChangedToken);
			m_settingsPropertyChangedToken = {};
		}
		if (m_settings)
		{
			m_settings.SaveSettings();
		}
		std::scoped_lock lock(m_graphStateMutex);
		DisposeBrushes();
	}

	winrt::OpenNet::ViewModels::TaskSpeedGraphSettingsViewModel
		TaskSpeedGraphPage::Settings() const
	{
		return m_settings;
	}

	void TaskSpeedGraphPage::MetricSelector_SelectionChanged(
		IInspectable const& sender,
		SelectionChangedEventArgs const&)
	{
		auto const selector = sender.try_as<ComboBox>();
		if (!m_isInitialized
			|| !selector
			|| selector.SelectedIndex() < 0)
		{
			return;
		}

		m_mode.store(
			static_cast<MetricMode>(selector.SelectedIndex()),
			std::memory_order_relaxed);
		ApplyGraphSettings();
		m_dynamicScale.store(1024.0, std::memory_order_relaxed);
		m_sampleElapsedSeconds.store(0.0, std::memory_order_relaxed);
		{
			std::scoped_lock lock(m_sampleStateMutex);
			m_previousDhtReceived = 0;
			m_previousDhtSent = 0;
			m_previousDhtSample = {};
			m_lastDhtReceivedRate = 0;
			m_lastDhtSentRate = 0;
			m_previousProcessorTimes.clear();
		}
		auto graph = PerformanceGraph();
		if (graph)
		{
			auto canvas = graph.GetCanvasAnimatedControl();
			if (canvas)
			{
				RecreateGraphStreams(canvas);
			}
		}
	}

	void TaskSpeedGraphPage::PerformanceGraph_CreateResources(
		IInspectable const&,
		CanvasAnimatedControl const& canvas)
	{
		RecreateGraphStreams(canvas);
	}

	void TaskSpeedGraphPage::Page_Loaded(
		IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		ApplyGraphSettings();
		if (!m_rebuildOnLoaded.exchange(false, std::memory_order_acq_rel))
		{
			return;
		}
		auto const graph = PerformanceGraph();
		auto const canvas = graph ? graph.GetCanvasAnimatedControl() : nullptr;
		if (canvas)
		{
			// The page is navigation-cached, but LiveGraph releases its Win2D
			// brushes on Unloaded. Recreate them whenever the cached page returns.
			RecreateGraphStreams(canvas);
		}
		else
		{
			m_rebuildOnLoaded.store(true, std::memory_order_release);
		}
	}

	void TaskSpeedGraphPage::Page_Unloaded(
		IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		m_graphActive.store(false, std::memory_order_release);
		m_rebuildOnLoaded.store(true, std::memory_order_release);
		UploadHighlightOverlay().Visibility(
			winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
	}

	void TaskSpeedGraphPage::PerformanceGraph_Draw(
		IInspectable const&,
		LiveGraphEventArgs const& args)
	{
		if (!m_graphActive.load(std::memory_order_acquire))
		{
			return;
		}

		auto frameSeconds = std::chrono::duration<double>(
			args.DrawEventArgs().Timing().ElapsedTime).count();
		if (!std::isfinite(frameSeconds) || frameSeconds < 0.0)
		{
			frameSeconds = 0.0;
		}
		frameSeconds = std::min(frameSeconds, 0.25);
		auto const sampleElapsed =
			m_sampleElapsedSeconds.load(std::memory_order_relaxed)
			+ frameSeconds;
		auto const sampleIntervalSeconds =
			m_graphSampleIntervalSeconds.load(std::memory_order_relaxed);
		if (sampleElapsed < sampleIntervalSeconds)
		{
			m_sampleElapsedSeconds.store(
				sampleElapsed,
				std::memory_order_relaxed);
			return;
		}
		m_sampleElapsedSeconds.store(0.0, std::memory_order_relaxed);

		auto [samples, displayValues] = SampleMetric();
		if (samples.empty())
		{
			return;
		}

		auto const mode = m_mode.load(std::memory_order_relaxed);
		bool const percentageMode =
			mode == MetricMode::CpuOverall
			|| mode == MetricMode::CpuLogicalProcessors
			|| mode == MetricMode::MemoryUsage;
		double maximum = percentageMode
			? 100.0
			: *std::ranges::max_element(samples);
		if (!percentageMode)
		{
			auto const previousScale =
				m_dynamicScale.load(std::memory_order_relaxed);
			auto const nextScale = std::max(
				1024.0,
				std::max(previousScale * 0.985, maximum * 1.20));
			m_dynamicScale.store(nextScale, std::memory_order_relaxed);
			maximum = nextScale;
		}
		m_highlightScale.store(maximum, std::memory_order_relaxed);

		{
			std::scoped_lock lock(m_graphStateMutex);
			auto graph = PerformanceGraph();
			auto const count = std::min(samples.size(), m_graphKeys.size());
			auto const scrollPixelsPerSecond =
				m_graphScrollPixelsPerSecond.load(std::memory_order_relaxed);
			auto const pointSpace = static_cast<float>(std::max(
				0.001,
				scrollPixelsPerSecond * sampleElapsed));
			for (std::size_t index = 0; index < count; ++index)
			{
				graph.AddDynamicPoint(
					m_graphKeys[index],
					GraphPoint{
						NormalizeValue(samples[index], maximum),
						pointSpace },
						m_smoothCurves.load(std::memory_order_relaxed));
			}
		}

		hstring scaleText;
		if (percentageMode)
		{
			scaleText = L"0–100%";
		}
		else if (mode == MetricMode::DiskCache)
		{
			scaleText = FormatBytes(maximum);
		}
		else
		{
			scaleText = FormatRate(maximum);
		}
		QueueMetricText(displayValues, scaleText);
		if (mode == MetricMode::TransferSpeed && samples.size() > 1)
		{
			QueueUploadHighlight(samples[1], maximum);
		}
	}

	void TaskSpeedGraphPage::PerformanceGraph_HighlightLineUpdated(
		IInspectable const&,
		float value)
	{
		auto const graph = PerformanceGraph();
		auto const canvas = graph ? graph.GetCanvasAnimatedControl() : nullptr;
		auto const height = canvas ? canvas.Size().Height : 0.0f;
		if (height <= 0.0f)
		{
			return;
		}

		auto const percentage = std::clamp(
			100.0f - (value * 100.0f / height),
			0.0f,
			100.0f);
		auto const mode = m_mode.load(std::memory_order_relaxed);
		if (mode == MetricMode::CpuOverall
			|| mode == MetricMode::CpuLogicalProcessors
			|| mode == MetricMode::MemoryUsage)
		{
			HighlightValueText().Text(
				hstring{ std::format(L"{:.1f}%", percentage) });
			return;
		}

		auto const actualValue =
			static_cast<double>(percentage) / 100.0
			* m_highlightScale.load(std::memory_order_relaxed);
		auto text = mode == MetricMode::DiskCache
			? FormatBytes(actualValue)
			: FormatRate(actualValue);
		if (mode == MetricMode::TransferSpeed)
		{
			text = hstring{ L"↓ " } + text;
		}
		HighlightValueText().Text(text);
	}

	void TaskSpeedGraphPage::ResetGraphSettings_Click(
		IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		m_settings.ResetDefaults();
	}

	void TaskSpeedGraphPage::ApplyGraphSettings()
	{
		auto const graph = PerformanceGraph();
		if (!graph || !m_settings)
		{
			return;
		}

		auto const scrollDurationSeconds =
			m_settings.HorizontalScrollDurationMilliseconds() / 1000.0;
		m_graphSampleIntervalSeconds.store(
			m_settings.SampleIntervalMilliseconds() / 1000.0,
			std::memory_order_relaxed);
		m_graphScrollPixelsPerSecond.store(
			scrollDurationSeconds > 0.0
			? m_settings.HorizontalScrollDistance()
			/ scrollDurationSeconds
			: 0.0,
			std::memory_order_relaxed);
		m_graphStrokeWidth.store(
			static_cast<float>(m_settings.StrokeWidth()),
			std::memory_order_relaxed);
		m_smoothCurves.store(
			m_settings.SmoothCurves(),
			std::memory_order_relaxed);
		m_fillEnabled.store(
			m_settings.FillEnabled(),
			std::memory_order_relaxed);
		m_borderEnabled.store(
			m_settings.BorderEnabled(),
			std::memory_order_relaxed);

		graph.BackgroundMode(static_cast<LiveGraphBackgroundMode>(
			std::clamp(m_settings.BackgroundModeIndex(), 0, 2)));
		graph.DotSpacing(m_settings.DotSpacing());
		graph.CrossSpacing(m_settings.CrossSpacing());
		graph.HorizontalScrollDistance(
			m_settings.HorizontalScrollDistance());
		graph.HorizontalScrollDuration(
			std::chrono::duration_cast<TimeSpan>(std::chrono::milliseconds{
				static_cast<std::int64_t>(
					m_settings.HorizontalScrollDurationMilliseconds()) }));
		graph.HighlightLineBehavior(static_cast<HighlightLineBehavior>(
			std::clamp(m_settings.HighlightBehaviorIndex(), 0, 4)));
		graph.HighlightLineVisibility(
			m_settings.HighlightEnabled()
			? winrt::Microsoft::UI::Xaml::Visibility::Visible
			: winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
		if (!m_settings.HighlightEnabled()
			|| m_mode.load(std::memory_order_relaxed) != MetricMode::TransferSpeed)
		{
			UploadHighlightOverlay().Visibility(
				winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
		}
		graph.HighlightLineAnimationDuration(
			std::chrono::duration_cast<TimeSpan>(std::chrono::milliseconds{
				static_cast<std::int64_t>(
					m_settings.HighlightAnimationMilliseconds()) }));
		graph.HistoryBufferScreens(m_settings.HistoryBufferScreens());

		if (m_settings.UseCustomCanvasColors())
		{
			graph.BackgroundColor(BoxColor(m_settings.BackgroundColor()));
			graph.ClearColor(BoxColor(m_settings.ClearColor()));
		}
		else
		{
			graph.BackgroundColor(nullptr);
			graph.ClearColor(BoxColor(Color{ 0, 0, 0, 0 }));
		}
	}

	void TaskSpeedGraphPage::ApplyBrushStrokeWidth()
	{
		std::scoped_lock lock(m_graphStateMutex);
		auto const graph = PerformanceGraph();
		if (!graph)
		{
			return;
		}

		auto const strokeWidth =
			m_graphStrokeWidth.load(std::memory_order_relaxed);
		auto const count = std::min(m_brushes.size(), m_graphKeys.size());
		for (std::size_t index = 0; index < count; ++index)
		{
			m_brushes[index].StrokeWidth(strokeWidth);
			graph.UpdateGraphBrush(m_graphKeys[index], m_brushes[index]);
		}
	}

	void TaskSpeedGraphPage::OnSettingsPropertyChanged(
		IInspectable const&,
		winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		ApplyGraphSettings();
		auto const propertyName = args.PropertyName();
		if (propertyName == L"StrokeWidth")
		{
			ApplyBrushStrokeWidth();
		}
		else if (propertyName == L"FillEnabled"
				 || propertyName == L"BorderEnabled")
		{
			auto const graph = PerformanceGraph();
			auto const canvas = graph ? graph.GetCanvasAnimatedControl() : nullptr;
			if (canvas)
			{
				RecreateGraphStreams(canvas);
			}
		}
	}

	void TaskSpeedGraphPage::RecreateGraphStreams(
		CanvasAnimatedControl const& canvas)
	{
		if (!canvas)
		{
			return;
		}
		m_graphActive.store(false, std::memory_order_release);

		std::scoped_lock lock(m_graphStateMutex);
		auto graph = PerformanceGraph();
		if (!graph)
		{
			return;
		}

		for (auto const& key : m_graphKeys)
		{
			graph.ResetDynamicGraph(key);
		}
		DisposeBrushes();
		m_graphKeys.clear();

		auto const seriesCount = RequiredSeriesCount();
		for (std::size_t index = 0; index < seriesCount; ++index)
		{
			GraphBrushData brush{ nullptr };
			switch (index)
			{
				case 0: brush = graph.GetGreenBrush(canvas); break;
				case 1: brush = graph.GetBlueBrush(canvas); break;
				case 2: brush = graph.GetRedBrush(canvas); break;
				case 3: brush = graph.GetPurpleBrush(canvas); break;
				default:
				{
					auto const color = SeriesColor(index);
					auto const faded = Color{
						110,
						static_cast<std::uint8_t>((color.R + 255) / 2),
						static_cast<std::uint8_t>((color.G + 255) / 2),
						static_cast<std::uint8_t>((color.B + 255) / 2) };
					std::array<Color, 2> colors{ faded, color };
					brush = graph.GetCustomBrush(canvas, colors);
					break;
				}
			}
			brush.StrokeWidth(
				m_graphStrokeWidth.load(std::memory_order_relaxed));
			if (!m_fillEnabled.load(std::memory_order_relaxed))
			{
				CloseResource(brush.Brush());
				CloseResource(brush.OpacityBrush());
				brush.Brush(nullptr);
				brush.OpacityBrush(nullptr);
			}
			if (!m_borderEnabled.load(std::memory_order_relaxed))
			{
				CloseResource(brush.BorderBrush());
				brush.BorderBrush(nullptr);
			}
			m_graphKeys.push_back(graph.RegisterGraphBrush(brush));
			m_brushes.push_back(brush);
		}
		m_sampleElapsedSeconds.store(0.0, std::memory_order_relaxed);
		m_rebuildOnLoaded.store(false, std::memory_order_release);
		m_graphActive.store(true, std::memory_order_release);
	}

	void TaskSpeedGraphPage::DisposeBrushes() noexcept
	{
		for (auto const& brush : m_brushes)
		{
			if (!brush)
			{
				continue;
			}
			CloseResource(brush.Brush());
			CloseResource(brush.OpacityBrush());
			CloseResource(brush.BorderBrush());
		}
		m_brushes.clear();
	}

	std::size_t TaskSpeedGraphPage::RequiredSeriesCount() const
	{
		switch (m_mode.load(std::memory_order_relaxed))
		{
			case MetricMode::TransferSpeed:
			case MetricMode::DhtUdp:
				return 2;
			case MetricMode::CpuLogicalProcessors:
				return std::max<DWORD>(
					DWORD{ 1 },
					::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
			default:
				return 1;
		}
	}

	std::vector<float> TaskSpeedGraphPage::SampleProcessorUsage()
	{
		auto module = ::GetModuleHandleW(L"ntdll.dll");
		auto query = reinterpret_cast<NtQuerySystemInformationProc>(
			::GetProcAddress(module, "NtQuerySystemInformation"));
		if (!query)
		{
			return {};
		}

		auto const processorCount = std::max<DWORD>(
			DWORD{ 1 },
			::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
		std::vector<NativeProcessorPerformanceInformation> current(processorCount);
		ULONG returned{};
		if (query(
			SystemProcessorPerformanceInformation,
			current.data(),
			static_cast<ULONG>(current.size() * sizeof(current.front())),
			&returned) < 0)
		{
			return {};
		}

		auto const actualCount =
			std::min<std::size_t>(current.size(), returned / sizeof(current.front()));
		std::vector<float> values(actualCount, 0.0f);
		if (m_previousProcessorTimes.size() == actualCount)
		{
			for (std::size_t index = 0; index < actualCount; ++index)
			{
				auto const idle = static_cast<std::uint64_t>(
					current[index].IdleTime.QuadPart);
				auto const kernel = static_cast<std::uint64_t>(
					current[index].KernelTime.QuadPart);
				auto const user = static_cast<std::uint64_t>(
					current[index].UserTime.QuadPart);
				auto const idleDelta = idle - m_previousProcessorTimes[index].Idle;
				auto const totalDelta =
					(kernel - m_previousProcessorTimes[index].Kernel)
					+ (user - m_previousProcessorTimes[index].User);
				if (totalDelta > 0)
				{
					values[index] = std::clamp(
						static_cast<float>(
							100.0 * (static_cast<double>(totalDelta - idleDelta)
									 / totalDelta)),
						0.0f,
						100.0f);
				}
			}
		}

		m_previousProcessorTimes.resize(actualCount);
		for (std::size_t index = 0; index < actualCount; ++index)
		{
			m_previousProcessorTimes[index] = {
				static_cast<std::uint64_t>(current[index].IdleTime.QuadPart),
				static_cast<std::uint64_t>(current[index].KernelTime.QuadPart),
				static_cast<std::uint64_t>(current[index].UserTime.QuadPart)
			};
		}
		return values;
	}

	std::pair<std::vector<double>, std::vector<hstring>>
		TaskSpeedGraphPage::SampleMetric()
	{
		std::scoped_lock sampleLock(m_sampleStateMutex);
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		auto const mode = m_mode.load(std::memory_order_relaxed);
		if (mode == MetricMode::CpuOverall
			|| mode == MetricMode::CpuLogicalProcessors)
		{
			auto processorValues = SampleProcessorUsage();
			if (processorValues.empty())
			{
				return {};
			}
			if (mode == MetricMode::CpuOverall)
			{
				double const average =
					std::accumulate(processorValues.begin(), processorValues.end(), 0.0)
					/ processorValues.size();
				return {
					std::vector<double>{ average },
					std::vector<hstring>{
						hstring{ std::format(L"{:.1f}%", average) } }
				};
			}

			std::vector<double> values(processorValues.begin(), processorValues.end());
			auto const average =
				std::accumulate(values.begin(), values.end(), 0.0) / values.size();
			auto const peak = *std::ranges::max_element(values);
			return {
				std::move(values),
				std::vector<hstring>{
					hstring{ std::format(L"{:.1f}% average", average) },
					hstring{ std::format(
						L"{:.1f}% peak · {} logical processors",
						peak,
						processorValues.size()) }
				}
			};
		}

		if (mode == MetricMode::MemoryUsage)
		{
			MEMORYSTATUSEX memory{ sizeof(memory) };
			PROCESS_MEMORY_COUNTERS_EX processMemory{};
			if (!::GlobalMemoryStatusEx(&memory)
				|| !::K32GetProcessMemoryInfo(
					::GetCurrentProcess(),
					reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&processMemory),
					sizeof(processMemory)))
			{
				return {};
			}
			double const bytes = static_cast<double>(processMemory.WorkingSetSize);
			double const percent = memory.ullTotalPhys > 0
				? bytes * 100.0 / memory.ullTotalPhys
				: 0.0;
			return {
				std::vector<double>{ percent },
				std::vector<hstring>{
					FormatBytes(bytes),
					hstring{ std::format(
						L"{:.1f}% of physical memory",
						percent) }
				}
			};
		}

		if (!core)
		{
			return {};
		}
		auto const stats = core->GetSessionStats();
		switch (mode)
		{
			case MetricMode::TransferSpeed:
				return {
					std::vector<double>{
						static_cast<double>(stats.totalDownloadRate),
						static_cast<double>(stats.totalUploadRate)
					},
					std::vector<hstring>{
						FormatRate(stats.totalDownloadRate),
						FormatRate(stats.totalUploadRate)
					}
				};

			case MetricMode::DiskCache:
				return {
					std::vector<double>{
						static_cast<double>(stats.diskCacheBytes) },
					std::vector<hstring>{
						FormatBytes(stats.diskCacheBytes),
						hstring{ L"Libtorrent disk blocks in use" } }
				};

			case MetricMode::LongTermSeeding:
				return {
					std::vector<double>{
						static_cast<double>(stats.longTermSeedingUploadRate) },
					std::vector<hstring>{
						FormatRate(stats.longTermSeedingUploadRate),
						hstring{ L"Finished and seeding tasks" } }
				};

			case MetricMode::DhtUdp:
			{
				auto const now = std::chrono::steady_clock::now();
				if (m_previousDhtSample.time_since_epoch().count() == 0)
				{
					m_previousDhtReceived = stats.dhtBytesReceived;
					m_previousDhtSent = stats.dhtBytesSent;
					m_previousDhtSample = now;
				}
				else if (stats.dhtBytesReceived != m_previousDhtReceived
						 || stats.dhtBytesSent != m_previousDhtSent)
				{
					double const seconds =
						std::chrono::duration<double>(now - m_previousDhtSample).count();
					if (seconds > 0)
					{
						auto const receivedDelta = std::max<std::int64_t>(
							0,
							stats.dhtBytesReceived - m_previousDhtReceived);
						auto const sentDelta = std::max<std::int64_t>(
							0,
							stats.dhtBytesSent - m_previousDhtSent);
						m_lastDhtReceivedRate =
							receivedDelta / seconds;
						m_lastDhtSentRate =
							sentDelta / seconds;
					}
					m_previousDhtReceived = stats.dhtBytesReceived;
					m_previousDhtSent = stats.dhtBytesSent;
					m_previousDhtSample = now;
				}
				return {
					std::vector<double>{
						std::max(0.0, m_lastDhtReceivedRate),
						std::max(0.0, m_lastDhtSentRate)
					},
					std::vector<hstring>{
						FormatRate(std::max(0.0, m_lastDhtReceivedRate)),
						hstring{ std::format(
							L"{} · {} nodes",
							FormatRate(std::max(0.0, m_lastDhtSentRate)).c_str(),
							stats.dhtNodes) }
					}
				};
			}

			default:
				return {};
		}
	}

	float TaskSpeedGraphPage::NormalizeValue(double value, double maximum)
	{
		if (maximum <= 0)
		{
			return 0.0f;
		}
		return std::clamp(
			static_cast<float>(value * 100.0 / maximum),
			0.0f,
			100.0f);
	}

	void TaskSpeedGraphPage::QueueMetricText(
		std::vector<hstring> const& values,
		hstring const& scaleText)
	{
		auto weak = get_weak();
		auto copy = values;
		DispatcherQueue().TryEnqueue([weak, values = std::move(copy), scaleText]
		{
			if (auto self = weak.get())
			{
				self->PrimaryMetricValue().Text(
					values.empty() ? hstring{ L"—" } : values[0]);
				self->SecondaryMetricValue().Text(
					values.size() > 1 ? values[1] : hstring{ L"—" });
				self->ScaleMetricValue().Text(scaleText);
			}
		});
	}

	void TaskSpeedGraphPage::QueueUploadHighlight(
		double value,
		double maximum)
	{
		if (!std::isfinite(value) || !std::isfinite(maximum) || maximum <= 0.0)
		{
			return;
		}

		auto weak = get_weak();
		DispatcherQueue().TryEnqueue([weak, value, maximum]
		{
			if (auto self = weak.get())
			{
				if (!self->m_settings.HighlightEnabled()
					|| self->m_mode.load(std::memory_order_relaxed)
					!= MetricMode::TransferSpeed)
				{
					self->UploadHighlightOverlay().Visibility(
						winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
					return;
				}

				auto const graph = self->PerformanceGraph();
				auto const canvas = graph ? graph.GetCanvasAnimatedControl() : nullptr;
				auto const height = canvas ? canvas.Size().Height : 0.0f;
				if (height <= 0.0f)
				{
					return;
				}

				auto const normalized = std::clamp(value / maximum, 0.0, 1.0);
				self->UploadHighlightTransform().Y(height * (1.0 - normalized));
				self->UploadHighlightValueText().Text(
					hstring{ L"↑ " } + TaskSpeedGraphPage::FormatRate(value));
				self->UploadHighlightOverlay().Visibility(
					winrt::Microsoft::UI::Xaml::Visibility::Visible);
			}
		});
	}

	hstring TaskSpeedGraphPage::FormatBytes(double value)
	{
		constexpr std::array units{ L"B", L"KiB", L"MiB", L"GiB", L"TiB" };
		std::size_t unit{};
		while (value >= 1024.0 && unit + 1 < units.size())
		{
			value /= 1024.0;
			++unit;
		}
		return hstring{ std::format(L"{:.1f} {}", value, units[unit]) };
	}

	hstring TaskSpeedGraphPage::FormatRate(double value)
	{
		return FormatBytes(value) + L"/s";
	}
}
