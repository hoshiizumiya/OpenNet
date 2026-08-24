#include "XamlWorkaround.h"
#include "UI/Xaml/Control/Progress/HttpSegment/HttpSegmentProgressBar.h"
#if __has_include("UI/Xaml/Control/Progress/HttpSegment/HttpSegmentProgressBar.g.cpp")
#include "UI/Xaml/Control/Progress/HttpSegment/HttpSegmentProgressBar.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::Control::Progress::HttpSegment::implementation
{
	DependencyProperty HttpSegmentProgressBar::s_rangeStartProperty{ nullptr };
	DependencyProperty HttpSegmentProgressBar::s_rangeEndProperty{ nullptr };
	DependencyProperty HttpSegmentProgressBar::s_valueProperty{ nullptr };

	HttpSegmentProgressBar::HttpSegmentProgressBar()
	{
		DefaultStyleKey(box_value(xaml_typename<class_type>()));
	}

	void HttpSegmentProgressBar::EnsureDependencyProperties()
	{
		if (s_rangeStartProperty) return;
		auto const ownerType = xaml_typename<class_type>();
		s_rangeStartProperty = DependencyProperty::Register(L"RangeStart", xaml_typename<double>(), ownerType, PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ &HttpSegmentProgressBar::OnProgressPropertyChanged } });
		s_rangeEndProperty = DependencyProperty::Register(L"RangeEnd", xaml_typename<double>(), ownerType, PropertyMetadata{ box_value(100.0), PropertyChangedCallback{ &HttpSegmentProgressBar::OnProgressPropertyChanged } });
		s_valueProperty = DependencyProperty::Register(L"Value", xaml_typename<double>(), ownerType, PropertyMetadata{ box_value(0.0), PropertyChangedCallback{ &HttpSegmentProgressBar::OnProgressPropertyChanged } });
	}

	DependencyProperty HttpSegmentProgressBar::RangeStartProperty()
	{
		EnsureDependencyProperties();
		return s_rangeStartProperty;
	}

	DependencyProperty HttpSegmentProgressBar::RangeEndProperty()
	{
		EnsureDependencyProperties();
		return s_rangeEndProperty;
	}

	DependencyProperty HttpSegmentProgressBar::ValueProperty()
	{
		EnsureDependencyProperties();
		return s_valueProperty;
	}

	double HttpSegmentProgressBar::RangeStart() const
	{
		return unbox_value<double>(GetValue(RangeStartProperty()));
	}
	void HttpSegmentProgressBar::RangeStart(double const value)
	{
		SetValue(RangeStartProperty(), box_value(std::clamp(value, 0.0, 100.0)));
	}
	double HttpSegmentProgressBar::RangeEnd() const
	{
		return unbox_value<double>(GetValue(RangeEndProperty()));
	}
	void HttpSegmentProgressBar::RangeEnd(double const value)
	{
		SetValue(RangeEndProperty(), box_value(std::clamp(value, 0.0, 100.0)));
	}
	double HttpSegmentProgressBar::Value() const
	{
		return unbox_value<double>(GetValue(ValueProperty()));
	}
	void HttpSegmentProgressBar::Value(double const value)
	{
		SetValue(ValueProperty(), box_value(std::clamp(value, 0.0, 100.0)));
	}

	void HttpSegmentProgressBar::OnProgressPropertyChanged(DependencyObject const& sender, DependencyPropertyChangedEventArgs const&)
	{
		if (auto control = sender.try_as<class_type>()) get_self<HttpSegmentProgressBar>(control)->UpdateColumns();
	}

	void HttpSegmentProgressBar::OnApplyTemplate()
	{
		base_type::OnApplyTemplate();
		m_preRangeColumn = GetTemplateChild(L"PART_PreRangeColumn").try_as<ColumnDefinition>();
		m_valueColumn = GetTemplateChild(L"PART_ValueColumn").try_as<ColumnDefinition>();
		m_remainingColumn = GetTemplateChild(L"PART_RemainingColumn").try_as<ColumnDefinition>();
		m_postRangeColumn = GetTemplateChild(L"PART_PostRangeColumn").try_as<ColumnDefinition>();
		UpdateColumns();
	}

	void HttpSegmentProgressBar::UpdateColumns()
	{
		if (!m_preRangeColumn || !m_valueColumn || !m_remainingColumn || !m_postRangeColumn) return;
		auto const start = std::clamp(RangeStart(), 0.0, 100.0);
		auto const end = std::clamp(RangeEnd(), start, 100.0);
		auto const range = end - start;
		auto const completed = range * std::clamp(Value(), 0.0, 100.0) / 100.0;
		m_preRangeColumn.Width({ (std::max)(0.0001, start), GridUnitType::Star });
		m_valueColumn.Width({ (std::max)(0.0001, completed), GridUnitType::Star });
		m_remainingColumn.Width({ (std::max)(0.0001, range - completed), GridUnitType::Star });
		m_postRangeColumn.Width({ (std::max)(0.0001, 100.0 - end), GridUnitType::Star });
	}
}
