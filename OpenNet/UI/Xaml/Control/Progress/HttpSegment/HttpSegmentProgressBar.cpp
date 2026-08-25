#include "XamlWorkaround.h"
#include "UI/Xaml/Control/Progress/HttpSegment/HttpSegmentProgressBar.h"
#if __has_include("UI/Xaml/Control/Progress/HttpSegment/HttpSegmentProgressBar.g.cpp")
#include "UI/Xaml/Control/Progress/HttpSegment/HttpSegmentProgressBar.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

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

	DependencyProperty RangePieceBar::s_piecesProperty{ nullptr };
	DependencyProperty RangePieceBar::s_rangeStartProperty{ nullptr };
	DependencyProperty RangePieceBar::s_rangeEndProperty{ nullptr };
	DependencyProperty RangePieceBar::s_valueProperty{ nullptr };
	DependencyProperty RangePieceBar::s_isRangeModeProperty{ nullptr };

	RangePieceBar::RangePieceBar()
	{
		DefaultStyleKey(box_value(xaml_typename<class_type>()));
		m_sizeChangedToken = SizeChanged([weak = get_weak()](auto const&, auto const&)
		{
			if (auto self = weak.get()) self->Render();
		});
	}

	RangePieceBar::~RangePieceBar()
	{
		if (m_sizeChangedToken.value) SizeChanged(m_sizeChangedToken);
	}

	void RangePieceBar::EnsureDependencyProperties()
	{
		if (s_piecesProperty) return;
		auto const ownerType = xaml_typename<class_type>();
		auto const callback = PropertyChangedCallback{ &RangePieceBar::OnDisplayPropertyChanged };
		s_piecesProperty = DependencyProperty::Register(L"Pieces", xaml_typename<hstring>(), ownerType, PropertyMetadata{ box_value(L""), callback });
		s_rangeStartProperty = DependencyProperty::Register(L"RangeStart", xaml_typename<double>(), ownerType, PropertyMetadata{ box_value(0.0), callback });
		s_rangeEndProperty = DependencyProperty::Register(L"RangeEnd", xaml_typename<double>(), ownerType, PropertyMetadata{ box_value(100.0), callback });
		s_valueProperty = DependencyProperty::Register(L"Value", xaml_typename<double>(), ownerType, PropertyMetadata{ box_value(0.0), callback });
		s_isRangeModeProperty = DependencyProperty::Register(L"IsRangeMode", xaml_typename<bool>(), ownerType, PropertyMetadata{ box_value(false), callback });
	}

	DependencyProperty RangePieceBar::PiecesProperty() { EnsureDependencyProperties(); return s_piecesProperty; }
	DependencyProperty RangePieceBar::RangeStartProperty() { EnsureDependencyProperties(); return s_rangeStartProperty; }
	DependencyProperty RangePieceBar::RangeEndProperty() { EnsureDependencyProperties(); return s_rangeEndProperty; }
	DependencyProperty RangePieceBar::ValueProperty() { EnsureDependencyProperties(); return s_valueProperty; }
	DependencyProperty RangePieceBar::IsRangeModeProperty() { EnsureDependencyProperties(); return s_isRangeModeProperty; }
	hstring RangePieceBar::Pieces() const { return unbox_value<hstring>(GetValue(PiecesProperty())); }
	void RangePieceBar::Pieces(hstring const& value) { SetValue(PiecesProperty(), box_value(value)); }
	double RangePieceBar::RangeStart() const { return unbox_value<double>(GetValue(RangeStartProperty())); }
	void RangePieceBar::RangeStart(double const value) { SetValue(RangeStartProperty(), box_value(std::clamp(value, 0.0, 100.0))); }
	double RangePieceBar::RangeEnd() const { return unbox_value<double>(GetValue(RangeEndProperty())); }
	void RangePieceBar::RangeEnd(double const value) { SetValue(RangeEndProperty(), box_value(std::clamp(value, 0.0, 100.0))); }
	double RangePieceBar::Value() const { return unbox_value<double>(GetValue(ValueProperty())); }
	void RangePieceBar::Value(double const value) { SetValue(ValueProperty(), box_value(std::clamp(value, 0.0, 100.0))); }
	bool RangePieceBar::IsRangeMode() const { return unbox_value<bool>(GetValue(IsRangeModeProperty())); }
	void RangePieceBar::IsRangeMode(bool const value) { SetValue(IsRangeModeProperty(), box_value(value)); }

	void RangePieceBar::OnDisplayPropertyChanged(DependencyObject const& sender, DependencyPropertyChangedEventArgs const&)
	{
		if (auto control = sender.try_as<class_type>()) get_self<RangePieceBar>(control)->Render();
	}

	void RangePieceBar::OnApplyTemplate()
	{
		m_root = GetTemplateChild(L"PART_PieceRoot").try_as<Grid>();
		Render();
	}

	SolidColorBrush RangePieceBar::BrushForState(wchar_t const state)
	{
		switch (state)
		{
			case L'1': return SolidColorBrush{ winrt::Windows::UI::Color{ 255, 215, 47, 154 } };
			case L'2': return SolidColorBrush{ winrt::Windows::UI::Color{ 255, 22, 131, 216 } };
			case L'3': return SolidColorBrush{ winrt::Windows::UI::Color{ 255, 105, 105, 105 } };
			case L'4': return SolidColorBrush{ winrt::Windows::UI::Color{ 255, 242, 200, 17 } };
			default: return SolidColorBrush{ winrt::Windows::UI::Color{ 80, 127, 127, 127 } };
		}
	}

	void RangePieceBar::Render()
	{
		if (!m_root) return;
		auto const children = m_root.Children();
		auto const columns = m_root.ColumnDefinitions();
		auto const rangeMode = IsRangeMode();
		if (rangeMode != m_renderedRangeMode)
		{
			children.Clear();
			columns.Clear();
			m_renderedStates.clear();
			m_renderedRangeMode = rangeMode;
		}
		if (rangeMode)
		{
			auto const start = std::clamp(RangeStart(), 0.0, 100.0);
			auto const end = std::clamp(RangeEnd(), start, 100.0);
			auto const range = end - start;
			auto const completed = range * std::clamp(Value(), 0.0, 100.0) / 100.0;
			std::array const widths{ start, completed, range - completed, 100.0 - end };
			if (columns.Size() != widths.size())
			{
				columns.Clear();
				for (auto const width : widths)
				{
					ColumnDefinition column;
					column.Width({ (std::max)(0.0001, width), GridUnitType::Star });
					columns.Append(column);
				}
			}
			else
			{
				for (std::uint32_t index = 0; index < widths.size(); ++index)
				{
					columns.GetAt(index).Width({ (std::max)(0.0001, widths[index]), GridUnitType::Star });
				}
			}
			if (children.Size() != 2)
			{
				children.Clear();
				Border assigned;
				assigned.Background(SolidColorBrush{ winrt::Windows::UI::Color{ 96, 22, 131, 216 } });
				assigned.CornerRadius({ 2, 2, 2, 2 });
				Grid::SetColumn(assigned, 1);
				Grid::SetColumnSpan(assigned, 2);
				children.Append(assigned);
				Border completedBar;
				completedBar.Background(BrushForState(L'2'));
				completedBar.CornerRadius({ 2, 2, 2, 2 });
				Grid::SetColumn(completedBar, 1);
				children.Append(completedBar);
			}
			return;
		}

		auto const pieces = Pieces();
		if (pieces.empty())
		{
			if (!m_renderedStates.empty())
			{
				children.Clear();
				columns.Clear();
				m_renderedStates.clear();
			}
			return;
		}
		auto const targetCount = (std::min<std::size_t>)(pieces.size(), 256);
		std::wstring states(targetCount, L'0');
		for (std::size_t bucket = 0; bucket < targetCount; ++bucket)
		{
			auto const first = pieces.size() * bucket / targetCount;
			auto const last = pieces.size() * (bucket + 1) / targetCount;
			wchar_t state = L'0';
			for (auto index = first; index < last; ++index)
			{
				auto const candidate = pieces[static_cast<std::uint32_t>(index)];
				if (candidate == L'1' || state == L'0') state = candidate;
				if (candidate == L'1') break;
			}
			states[bucket] = state;
		}
		if (children.Size() != targetCount || columns.Size() != targetCount)
		{
			children.Clear();
			columns.Clear();
			for (std::size_t bucket = 0; bucket < targetCount; ++bucket)
			{
				ColumnDefinition column;
				column.Width({ 1, GridUnitType::Star });
				columns.Append(column);
				Border piece;
				piece.Background(BrushForState(states[bucket]));
				piece.Margin({ 0.25, 0, 0.25, 0 });
				Grid::SetColumn(piece, static_cast<int>(bucket));
				children.Append(piece);
			}
		}
		else
		{
			for (std::uint32_t bucket = 0; bucket < targetCount; ++bucket)
			{
				if (bucket < m_renderedStates.size() && states[bucket] == m_renderedStates[bucket]) continue;
				children.GetAt(bucket).as<Border>().Background(BrushForState(states[bucket]));
			}
		}
		m_renderedStates = std::move(states);
	}
}
