#pragma once

#include "UI/Xaml/Control/Progress/Storage/StorageBar.g.h"

import OpenNet.Helpers.EnsureDependencyProperties;

namespace winrt::OpenNet::UI::Xaml::Control::Progress::Storage::implementation
{
	struct StorageBar : StorageBarT<StorageBar>, EnsureDependencyProperty<StorageBar>
	{
		StorageBar();
		~StorageBar();

		double ValueBarHeight() const;
		void ValueBarHeight(double value);
		double TrackBarHeight() const;
		void TrackBarHeight(double value);
		OpenNet::UI::Xaml::Control::Progress::Storage::BarShapes BarShape() const;
		void BarShape(OpenNet::UI::Xaml::Control::Progress::Storage::BarShapes value);
		double Percent() const;
		double PercentCaution() const;
		void PercentCaution(double value);
		double PercentCritical() const;
		void PercentCritical(double value);

		static winrt::Microsoft::UI::Xaml::DependencyProperty ValueBarHeightProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty TrackBarHeightProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty BarShapeProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty PercentCautionProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty PercentCriticalProperty();
		static void EnsureDependencyProperties();

		void OnApplyTemplate();
		void OnValueChanged(double oldValue, double newValue);
		void OnMaximumChanged(double oldValue, double newValue);
		void OnMinimumChanged(double oldValue, double newValue);

	private:
		static void OnLayoutPropertyChanged(winrt::Microsoft::UI::Xaml::DependencyObject const& sender, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		void UpdateControl();
		void UpdateHeightsAndCorners();
		void UpdateColumnWidths();
		void UpdateVisualState();
		double ValuePercent() const;

		winrt::Microsoft::UI::Xaml::Controls::Grid m_container{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition m_valueColumn{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition m_gapColumn{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition m_trackColumn{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::Border m_valueBar{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::Border m_trackBar{ nullptr };
		winrt::event_token m_sizeChangedToken{};
		winrt::event_token m_isEnabledChangedToken{};
		bool m_updatingControl{};
		winrt::hstring m_visualState;

		static winrt::Microsoft::UI::Xaml::DependencyProperty s_valueBarHeightProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_trackBarHeightProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_barShapeProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_percentCautionProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_percentCriticalProperty;
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::Progress::Storage::factory_implementation
{
	struct StorageBar : StorageBarT<StorageBar, implementation::StorageBar>
	{
	};
}
