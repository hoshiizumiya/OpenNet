#include "XamlWorkaround.h"
#include "TaskSpeedGraphSettingsViewModel.h"
#include "ViewModels/TaskSpeedGraphSettingsViewModel.g.cpp"

import OpenNet.Core.AppSettingsDatabase;
import std;
import winrt.Microsoft.UI.Dispatching;

namespace
{
	constexpr char SettingsCategory[] = "task_speed_graph";

	uint32_t PackColor(winrt::Windows::UI::Color const& value) noexcept
	{
		return
			(static_cast<uint32_t>(value.A) << 24) |
			(static_cast<uint32_t>(value.R) << 16) |
			(static_cast<uint32_t>(value.G) << 8) |
			static_cast<uint32_t>(value.B);
	}

	winrt::Windows::UI::Color UnpackColor(int64_t value) noexcept
	{
		auto const argb = static_cast<uint32_t>(value);
		return winrt::Windows::UI::Color{
			static_cast<uint8_t>((argb >> 24) & 0xFF),
			static_cast<uint8_t>((argb >> 16) & 0xFF),
			static_cast<uint8_t>((argb >> 8) & 0xFF),
			static_cast<uint8_t>(argb & 0xFF) };
	}

	winrt::Windows::UI::Color DefaultGridColor() noexcept
	{
		return { 0x20, 0xFF, 0xFF, 0xFF };
	}

	winrt::Windows::UI::Color DefaultClearColor() noexcept
	{
		return { 0x00, 0x00, 0x00, 0x00 };
	}
}

namespace winrt::OpenNet::ViewModels::implementation
{
	int32_t TaskSpeedGraphSettingsViewModel::BackgroundModeIndex() const
	{
		return m_backgroundModeIndex;
	}

	void TaskSpeedGraphSettingsViewModel::BackgroundModeIndex(int32_t value)
	{
		value = std::clamp(value, 0, 2);
		if (SetProperty(m_backgroundModeIndex, value, L"BackgroundModeIndex"))
		{
			PersistIfReady();
		}
	}

	double TaskSpeedGraphSettingsViewModel::DotSpacing() const
	{
		return m_dotSpacing;
	}

	void TaskSpeedGraphSettingsViewModel::DotSpacing(double value)
	{
		if (!std::isfinite(value))
		{
			value = DefaultDotSpacing;
		}
		value = std::clamp(value, 2.0, 200.0);
		if (SetProperty(m_dotSpacing, value, L"DotSpacing"))
		{
			PersistIfReady();
		}
	}

	double TaskSpeedGraphSettingsViewModel::CrossSpacing() const
	{
		return m_crossSpacing;
	}

	void TaskSpeedGraphSettingsViewModel::CrossSpacing(double value)
	{
		if (!std::isfinite(value))
		{
			value = DefaultCrossSpacing;
		}
		value = std::clamp(value, 2.0, 200.0);
		if (SetProperty(m_crossSpacing, value, L"CrossSpacing"))
		{
			PersistIfReady();
		}
	}

	double TaskSpeedGraphSettingsViewModel::HorizontalScrollDistance() const
	{
		return m_horizontalScrollDistance;
	}

	void TaskSpeedGraphSettingsViewModel::HorizontalScrollDistance(double value)
	{
		if (!std::isfinite(value))
		{
			value = DefaultScrollDistance;
		}
		value = std::clamp(value, 0.1, 500.0);
		if (SetProperty(m_horizontalScrollDistance, value, L"HorizontalScrollDistance"))
		{
			PersistIfReady();
		}
	}

	double TaskSpeedGraphSettingsViewModel::HorizontalScrollDurationMilliseconds() const
	{
		return m_horizontalScrollDurationMilliseconds;
	}

	void TaskSpeedGraphSettingsViewModel::HorizontalScrollDurationMilliseconds(
		double value)
	{
		if (!std::isfinite(value))
		{
			value = DefaultScrollDuration;
		}
		value = std::clamp(value, 1.0, 5000.0);
		if (SetProperty(
			m_horizontalScrollDurationMilliseconds,
			value,
			L"HorizontalScrollDurationMilliseconds"))
		{
			PersistIfReady();
		}
	}

	double TaskSpeedGraphSettingsViewModel::SampleIntervalMilliseconds() const
	{
		return m_sampleIntervalMilliseconds;
	}

	void TaskSpeedGraphSettingsViewModel::SampleIntervalMilliseconds(double value)
	{
		if (!std::isfinite(value))
		{
			value = DefaultSampleInterval;
		}
		value = std::clamp(value, 16.0, 5000.0);
		if (SetProperty(m_sampleIntervalMilliseconds, value, L"SampleIntervalMilliseconds"))
		{
			PersistIfReady();
		}
	}

	bool TaskSpeedGraphSettingsViewModel::SmoothCurves() const
	{
		return m_smoothCurves;
	}

	void TaskSpeedGraphSettingsViewModel::SmoothCurves(bool value)
	{
		if (SetProperty(m_smoothCurves, value, L"SmoothCurves"))
		{
			PersistIfReady();
		}
	}

	bool TaskSpeedGraphSettingsViewModel::HighlightEnabled() const
	{
		return m_highlightEnabled;
	}

	void TaskSpeedGraphSettingsViewModel::HighlightEnabled(bool value)
	{
		if (SetProperty(m_highlightEnabled, value, L"HighlightEnabled"))
		{
			PersistIfReady();
		}
	}

	int32_t TaskSpeedGraphSettingsViewModel::HighlightBehaviorIndex() const
	{
		return m_highlightBehaviorIndex;
	}

	void TaskSpeedGraphSettingsViewModel::HighlightBehaviorIndex(int32_t value)
	{
		value = std::clamp(value, 0, 4);
		if (SetProperty(m_highlightBehaviorIndex, value, L"HighlightBehaviorIndex"))
		{
			PersistIfReady();
		}
	}

	double TaskSpeedGraphSettingsViewModel::HighlightAnimationMilliseconds() const
	{
		return m_highlightAnimationMilliseconds;
	}

	void TaskSpeedGraphSettingsViewModel::HighlightAnimationMilliseconds(double value)
	{
		if (!std::isfinite(value))
		{
			value = DefaultHighlightAnimation;
		}
		value = std::clamp(value, 0.0, 2000.0);
		if (SetProperty(m_highlightAnimationMilliseconds, value, L"HighlightAnimationMilliseconds"))
		{
			PersistIfReady();
		}
	}

	double TaskSpeedGraphSettingsViewModel::HistoryBufferScreens() const
	{
		return m_historyBufferScreens;
	}

	void TaskSpeedGraphSettingsViewModel::HistoryBufferScreens(double value)
	{
		if (!std::isfinite(value))
		{
			value = DefaultHistoryBufferScreens;
		}
		value = std::clamp(value, 0.0, 4.0);
		if (SetProperty(m_historyBufferScreens, value, L"HistoryBufferScreens"))
		{
			PersistIfReady();
		}
	}

	bool TaskSpeedGraphSettingsViewModel::UseCustomCanvasColors() const
	{
		return m_useCustomCanvasColors;
	}

	void TaskSpeedGraphSettingsViewModel::UseCustomCanvasColors(bool value)
	{
		if (SetProperty(m_useCustomCanvasColors, value, L"UseCustomCanvasColors"))
		{
			PersistIfReady();
		}
	}

	winrt::Windows::UI::Color TaskSpeedGraphSettingsViewModel::BackgroundColor() const
	{
		return m_backgroundColor;
	}

	void TaskSpeedGraphSettingsViewModel::BackgroundColor(
		winrt::Windows::UI::Color const& value)
	{
		if (SetProperty(m_backgroundColor, value, L"BackgroundColor"))
		{
			PersistIfReady();
		}
	}

	winrt::Windows::UI::Color TaskSpeedGraphSettingsViewModel::ClearColor() const
	{
		return m_clearColor;
	}

	void TaskSpeedGraphSettingsViewModel::ClearColor(
		winrt::Windows::UI::Color const& value)
	{
		if (SetProperty(m_clearColor, value, L"ClearColor"))
		{
			PersistIfReady();
		}
	}

	double TaskSpeedGraphSettingsViewModel::StrokeWidth() const
	{
		return m_strokeWidth;
	}

	void TaskSpeedGraphSettingsViewModel::StrokeWidth(double value)
	{
		if (!std::isfinite(value))
		{
			value = DefaultStrokeWidth;
		}
		value = std::clamp(value, 0.5, 12.0);
		if (SetProperty(m_strokeWidth, value, L"StrokeWidth"))
		{
			PersistIfReady();
		}
	}

	bool TaskSpeedGraphSettingsViewModel::FillEnabled() const
	{
		return m_fillEnabled;
	}

	void TaskSpeedGraphSettingsViewModel::FillEnabled(bool value)
	{
		if (SetProperty(m_fillEnabled, value, L"FillEnabled"))
		{
			PersistIfReady();
		}
	}

	bool TaskSpeedGraphSettingsViewModel::BorderEnabled() const
	{
		return m_borderEnabled;
	}

	void TaskSpeedGraphSettingsViewModel::BorderEnabled(bool value)
	{
		if (SetProperty(m_borderEnabled, value, L"BorderEnabled"))
		{
			PersistIfReady();
		}
	}

	void TaskSpeedGraphSettingsViewModel::Initialize()
	{
		::OpenNet::Core::AppSettingsDatabase::Instance().Initialize();
		if (auto const dispatcher =
			winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread())
		{
			m_saveTimer = dispatcher.CreateTimer();
			m_saveTimer.Interval(std::chrono::milliseconds{ 350 });
			m_saveTimer.IsRepeating(false);
			m_saveTimer.Tick(
				[weak = get_weak()](auto const&, auto const&)
				{
					if (auto self = weak.get())
					{
						self->SaveSettings();
					}
				});
		}
		LoadSettings();
	}

	void TaskSpeedGraphSettingsViewModel::LoadSettings()
	{
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		m_isLoading = true;

		BackgroundModeIndex(static_cast<int32_t>(
			database.GetInt(SettingsCategory, "background_mode", DefaultBackgroundModeIndex)));
		DotSpacing(database.GetDouble(SettingsCategory, "dot_spacing").value_or(DefaultDotSpacing));
		CrossSpacing(database.GetDouble(SettingsCategory, "cross_spacing").value_or(DefaultCrossSpacing));
		HorizontalScrollDistance(
			database.GetDouble(SettingsCategory, "scroll_distance")
				.value_or(DefaultScrollDistance));
		HorizontalScrollDurationMilliseconds(
			database.GetDouble(SettingsCategory, "scroll_duration")
				.value_or(DefaultScrollDuration));
		SampleIntervalMilliseconds(
			database.GetDouble(SettingsCategory, "sample_interval").value_or(DefaultSampleInterval));
		SmoothCurves(
			database.GetBool(SettingsCategory, "smooth_curves").value_or(DefaultSmoothCurves));
		HighlightEnabled(
			database.GetBool(SettingsCategory, "highlight_enabled").value_or(DefaultHighlightEnabled));
		HighlightBehaviorIndex(static_cast<int32_t>(
			database.GetInt(SettingsCategory, "highlight_behavior", DefaultHighlightBehaviorIndex)));
		HighlightAnimationMilliseconds(
			database.GetDouble(SettingsCategory, "highlight_animation")
				.value_or(DefaultHighlightAnimation));
		HistoryBufferScreens(
			database.GetDouble(SettingsCategory, "history_buffer")
				.value_or(DefaultHistoryBufferScreens));
		UseCustomCanvasColors(
			database.GetBool(SettingsCategory, "custom_canvas_colors")
				.value_or(DefaultUseCustomCanvasColors));
		BackgroundColor(UnpackColor(database.GetInt(
			SettingsCategory,
			"background_color",
			static_cast<int64_t>(PackColor(DefaultGridColor())))));
		ClearColor(UnpackColor(database.GetInt(
			SettingsCategory,
			"clear_color",
			static_cast<int64_t>(PackColor(DefaultClearColor())))));
		StrokeWidth(
			database.GetDouble(SettingsCategory, "stroke_width").value_or(DefaultStrokeWidth));
		FillEnabled(
			database.GetBool(SettingsCategory, "fill_enabled").value_or(DefaultFillEnabled));
		BorderEnabled(
			database.GetBool(SettingsCategory, "border_enabled").value_or(DefaultBorderEnabled));

		m_isLoading = false;
	}

	void TaskSpeedGraphSettingsViewModel::SaveSettings()
	{
		if (m_saveTimer)
		{
			m_saveTimer.Stop();
		}
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.SetInt(SettingsCategory, "background_mode", m_backgroundModeIndex);
		database.SetDouble(SettingsCategory, "dot_spacing", m_dotSpacing);
		database.SetDouble(SettingsCategory, "cross_spacing", m_crossSpacing);
		database.SetDouble(
			SettingsCategory,
			"scroll_distance",
			m_horizontalScrollDistance);
		database.SetDouble(
			SettingsCategory,
			"scroll_duration",
			m_horizontalScrollDurationMilliseconds);
		database.SetDouble(SettingsCategory, "sample_interval", m_sampleIntervalMilliseconds);
		database.SetBool(SettingsCategory, "smooth_curves", m_smoothCurves);
		database.SetBool(SettingsCategory, "highlight_enabled", m_highlightEnabled);
		database.SetInt(SettingsCategory, "highlight_behavior", m_highlightBehaviorIndex);
		database.SetDouble(
			SettingsCategory,
			"highlight_animation",
			m_highlightAnimationMilliseconds);
		database.SetDouble(SettingsCategory, "history_buffer", m_historyBufferScreens);
		database.SetBool(SettingsCategory, "custom_canvas_colors", m_useCustomCanvasColors);
		database.SetInt(SettingsCategory, "background_color", PackColor(m_backgroundColor));
		database.SetInt(SettingsCategory, "clear_color", PackColor(m_clearColor));
		database.SetDouble(SettingsCategory, "stroke_width", m_strokeWidth);
		database.SetBool(SettingsCategory, "fill_enabled", m_fillEnabled);
		database.SetBool(SettingsCategory, "border_enabled", m_borderEnabled);
	}

	void TaskSpeedGraphSettingsViewModel::ResetDefaults()
	{
		m_isLoading = true;
		BackgroundModeIndex(DefaultBackgroundModeIndex);
		DotSpacing(DefaultDotSpacing);
		CrossSpacing(DefaultCrossSpacing);
		HorizontalScrollDistance(DefaultScrollDistance);
		HorizontalScrollDurationMilliseconds(DefaultScrollDuration);
		SampleIntervalMilliseconds(DefaultSampleInterval);
		SmoothCurves(DefaultSmoothCurves);
		HighlightEnabled(DefaultHighlightEnabled);
		HighlightBehaviorIndex(DefaultHighlightBehaviorIndex);
		HighlightAnimationMilliseconds(DefaultHighlightAnimation);
		HistoryBufferScreens(DefaultHistoryBufferScreens);
		UseCustomCanvasColors(DefaultUseCustomCanvasColors);
		BackgroundColor(DefaultGridColor());
		ClearColor(DefaultClearColor());
		StrokeWidth(DefaultStrokeWidth);
		FillEnabled(DefaultFillEnabled);
		BorderEnabled(DefaultBorderEnabled);
		m_isLoading = false;
		SaveSettings();
	}

	void TaskSpeedGraphSettingsViewModel::PersistIfReady()
	{
		if (!m_isLoading)
		{
			if (m_saveTimer)
			{
				m_saveTimer.Stop();
				m_saveTimer.Start();
			}
			else
			{
				SaveSettings();
			}
		}
	}

}
