#pragma once

#include "ViewModels/TaskSpeedGraphSettingsViewModel.g.h"

import OpenNet.ViewModels.ObservableMixin;
import winrt.Microsoft.UI.Dispatching;
import winrt.Windows.Foundation;
import winrt.Windows.UI;

namespace winrt::OpenNet::ViewModels::implementation
{
	struct TaskSpeedGraphSettingsViewModel :
		TaskSpeedGraphSettingsViewModelT<TaskSpeedGraphSettingsViewModel>,
		::OpenNet::ViewModels::ObservableMixin<TaskSpeedGraphSettingsViewModel>
	{
		TaskSpeedGraphSettingsViewModel() = default;

		int32_t BackgroundModeIndex() const;
		void BackgroundModeIndex(int32_t value);
		double DotSpacing() const;
		void DotSpacing(double value);
		double CrossSpacing() const;
		void CrossSpacing(double value);
		double HorizontalScrollDistance() const;
		void HorizontalScrollDistance(double value);
		double HorizontalScrollDurationMilliseconds() const;
		void HorizontalScrollDurationMilliseconds(double value);
		double SampleIntervalMilliseconds() const;
		void SampleIntervalMilliseconds(double value);
		bool SmoothCurves() const;
		void SmoothCurves(bool value);
		bool HighlightEnabled() const;
		void HighlightEnabled(bool value);
		int32_t HighlightBehaviorIndex() const;
		void HighlightBehaviorIndex(int32_t value);
		double HighlightAnimationMilliseconds() const;
		void HighlightAnimationMilliseconds(double value);
		double HistoryBufferScreens() const;
		void HistoryBufferScreens(double value);
		bool UseCustomCanvasColors() const;
		void UseCustomCanvasColors(bool value);
		winrt::Windows::UI::Color BackgroundColor() const;
		void BackgroundColor(winrt::Windows::UI::Color const& value);
		winrt::Windows::UI::Color ClearColor() const;
		void ClearColor(winrt::Windows::UI::Color const& value);
		double StrokeWidth() const;
		void StrokeWidth(double value);
		bool FillEnabled() const;
		void FillEnabled(bool value);
		bool BorderEnabled() const;
		void BorderEnabled(bool value);

		void Initialize();
		void LoadSettings();
		void SaveSettings();
		void ResetDefaults();

	private:
		void PersistIfReady();

		static constexpr int32_t DefaultBackgroundModeIndex = 1;
		static constexpr double DefaultDotSpacing = 14.0;
		static constexpr double DefaultCrossSpacing = 30.0;
		static constexpr double DefaultScrollDistance = 6.0;
		static constexpr double DefaultScrollDuration = 100.0;
		static constexpr double DefaultSampleInterval = 100.0;
		static constexpr bool DefaultSmoothCurves = true;
		static constexpr bool DefaultHighlightEnabled = true;
		static constexpr int32_t DefaultHighlightBehaviorIndex = 4;
		static constexpr double DefaultHighlightAnimation = 300.0;
		static constexpr double DefaultHistoryBufferScreens = 1.0;
		static constexpr bool DefaultUseCustomCanvasColors = false;
		static constexpr double DefaultStrokeWidth = 2.0;
		static constexpr bool DefaultFillEnabled = true;
		static constexpr bool DefaultBorderEnabled = true;

		int32_t m_backgroundModeIndex{ DefaultBackgroundModeIndex };
		double m_dotSpacing{ DefaultDotSpacing };
		double m_crossSpacing{ DefaultCrossSpacing };
		double m_horizontalScrollDistance{ DefaultScrollDistance };
		double m_horizontalScrollDurationMilliseconds{ DefaultScrollDuration };
		double m_sampleIntervalMilliseconds{ DefaultSampleInterval };
		bool m_smoothCurves{ DefaultSmoothCurves };
		bool m_highlightEnabled{ DefaultHighlightEnabled };
		int32_t m_highlightBehaviorIndex{ DefaultHighlightBehaviorIndex };
		double m_highlightAnimationMilliseconds{ DefaultHighlightAnimation };
		double m_historyBufferScreens{ DefaultHistoryBufferScreens };
		bool m_useCustomCanvasColors{ DefaultUseCustomCanvasColors };
		winrt::Windows::UI::Color m_backgroundColor{ 0x20, 0xFF, 0xFF, 0xFF };
		winrt::Windows::UI::Color m_clearColor{ 0x00, 0x00, 0x00, 0x00 };
		double m_strokeWidth{ DefaultStrokeWidth };
		bool m_fillEnabled{ DefaultFillEnabled };
		bool m_borderEnabled{ DefaultBorderEnabled };
		bool m_isLoading{};
		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer
			m_saveTimer{ nullptr };
	};
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct TaskSpeedGraphSettingsViewModel :
		TaskSpeedGraphSettingsViewModelT<
			TaskSpeedGraphSettingsViewModel,
			implementation::TaskSpeedGraphSettingsViewModel>
	{
	};
}
