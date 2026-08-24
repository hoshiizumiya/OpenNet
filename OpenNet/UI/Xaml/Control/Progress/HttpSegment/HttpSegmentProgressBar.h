#pragma once

#include "UI/Xaml/Control/Progress/HttpSegment/HttpSegmentProgressBar.g.h"

import OpenNet.Helpers.EnsureDependencyProperties;

namespace winrt::OpenNet::UI::Xaml::Control::Progress::HttpSegment::implementation
{
	struct HttpSegmentProgressBar : HttpSegmentProgressBarT<HttpSegmentProgressBar>, EnsureDependencyProperty<HttpSegmentProgressBar>
	{
		HttpSegmentProgressBar();

		double RangeStart() const;
		void RangeStart(double value);
		double RangeEnd() const;
		void RangeEnd(double value);
		double Value() const;
		void Value(double value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty RangeStartProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty RangeEndProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty ValueProperty();
		static void EnsureDependencyProperties();
		void OnApplyTemplate();

	private:
		static void OnProgressPropertyChanged(winrt::Microsoft::UI::Xaml::DependencyObject const& sender, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		void UpdateColumns();

		winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition m_preRangeColumn{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition m_valueColumn{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition m_remainingColumn{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition m_postRangeColumn{ nullptr };
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_rangeStartProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_rangeEndProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_valueProperty;
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::Progress::HttpSegment::factory_implementation
{
	struct HttpSegmentProgressBar : HttpSegmentProgressBarT<HttpSegmentProgressBar, implementation::HttpSegmentProgressBar>
	{
	};
}
