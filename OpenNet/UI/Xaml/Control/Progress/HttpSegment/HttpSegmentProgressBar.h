#pragma once

#include "UI/Xaml/Control/Progress/HttpSegment/HttpSegmentProgressBar.g.h"
#include "UI/Xaml/Control/Progress/HttpSegment/RangePieceBar.g.h"

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

	struct RangePieceBar : RangePieceBarT<RangePieceBar>, EnsureDependencyProperty<RangePieceBar>
	{
		RangePieceBar();
		~RangePieceBar();
		winrt::hstring Pieces() const;
		void Pieces(winrt::hstring const& value);
		double RangeStart() const;
		void RangeStart(double value);
		double RangeEnd() const;
		void RangeEnd(double value);
		double Value() const;
		void Value(double value);
		bool IsRangeMode() const;
		void IsRangeMode(bool value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty PiecesProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty RangeStartProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty RangeEndProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty ValueProperty();
		static winrt::Microsoft::UI::Xaml::DependencyProperty IsRangeModeProperty();
		static void EnsureDependencyProperties();
		void OnApplyTemplate();

	private:
		static void OnDisplayPropertyChanged(winrt::Microsoft::UI::Xaml::DependencyObject const& sender, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& args);
		void Render();
		static winrt::Microsoft::UI::Xaml::Media::SolidColorBrush BrushForState(wchar_t state);

		winrt::Microsoft::UI::Xaml::Controls::Grid m_root{ nullptr };
		winrt::event_token m_sizeChangedToken{};
		std::wstring m_renderedStates;
		bool m_renderedRangeMode{};
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_piecesProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_rangeStartProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_rangeEndProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_valueProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_isRangeModeProperty;
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::Progress::HttpSegment::factory_implementation
{
	struct HttpSegmentProgressBar : HttpSegmentProgressBarT<HttpSegmentProgressBar, implementation::HttpSegmentProgressBar>
	{
	};
	struct RangePieceBar : RangePieceBarT<RangePieceBar, implementation::RangePieceBar>
	{
	};
}
