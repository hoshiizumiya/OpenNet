#include "XamlWorkaround.h"
#include "UI/Xaml/Control/Progress/Stroge/StorageBar.h"
#if __has_include("UI/Xaml/Control/Progress/Storage/StorageBar.g.cpp")
#include "UI/Xaml/Control/Progress/Storage/StorageBar.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;

namespace winrt::OpenNet::UI::Xaml::Control::Progress::Storage::implementation
{
	DependencyProperty StorageBar::s_valueBarHeightProperty{ nullptr };
	DependencyProperty StorageBar::s_trackBarHeightProperty{ nullptr };
	DependencyProperty StorageBar::s_barShapeProperty{ nullptr };
	DependencyProperty StorageBar::s_percentCautionProperty{ nullptr };
	DependencyProperty StorageBar::s_percentCriticalProperty{ nullptr };

	StorageBar::StorageBar()
	{
		DefaultStyleKey(box_value(xaml_typename<class_type>()));
		m_sizeChangedToken = SizeChanged([weak = get_weak()](auto const&, auto const&)
		{
			if (auto self = weak.get()) self->UpdateControl();
		});
		m_isEnabledChangedToken = IsEnabledChanged([weak = get_weak()](auto const&, auto const&)
		{
			if (auto self = weak.get()) self->UpdateVisualState();
		});
	}

	StorageBar::~StorageBar()
	{
		if (m_sizeChangedToken.value) SizeChanged(m_sizeChangedToken);
		if (m_isEnabledChangedToken.value) IsEnabledChanged(m_isEnabledChangedToken);
	}

	void StorageBar::EnsureDependencyProperties()
	{
		if (s_valueBarHeightProperty) return;
		auto const ownerType = xaml_typename<class_type>();
		s_valueBarHeightProperty = DependencyProperty::Register(
			L"ValueBarHeight", xaml_typename<double>(), ownerType,
			PropertyMetadata{ box_value(6.0), PropertyChangedCallback{ &StorageBar::OnLayoutPropertyChanged } });
		s_trackBarHeightProperty = DependencyProperty::Register(
			L"TrackBarHeight", xaml_typename<double>(), ownerType,
			PropertyMetadata{ box_value(3.0), PropertyChangedCallback{ &StorageBar::OnLayoutPropertyChanged } });
		s_barShapeProperty = DependencyProperty::Register(
			L"BarShape", xaml_typename<OpenNet::UI::Xaml::Control::Progress::Storage::BarShapes>(), ownerType,
			PropertyMetadata{ box_value(OpenNet::UI::Xaml::Control::Progress::Storage::BarShapes::Round), PropertyChangedCallback{ &StorageBar::OnLayoutPropertyChanged } });
		s_percentCautionProperty = DependencyProperty::Register(
			L"PercentCaution", xaml_typename<double>(), ownerType,
			PropertyMetadata{ box_value(75.1), PropertyChangedCallback{ &StorageBar::OnLayoutPropertyChanged } });
		s_percentCriticalProperty = DependencyProperty::Register(
			L"PercentCritical", xaml_typename<double>(), ownerType,
			PropertyMetadata{ box_value(89.9), PropertyChangedCallback{ &StorageBar::OnLayoutPropertyChanged } });
	}

	DependencyProperty StorageBar::ValueBarHeightProperty()
	{
		EnsureDependencyProperties();
		return s_valueBarHeightProperty;
	}

	DependencyProperty StorageBar::TrackBarHeightProperty()
	{
		EnsureDependencyProperties();
		return s_trackBarHeightProperty;
	}

	DependencyProperty StorageBar::BarShapeProperty()
	{
		EnsureDependencyProperties();
		return s_barShapeProperty;
	}

	DependencyProperty StorageBar::PercentCautionProperty()
	{
		EnsureDependencyProperties();
		return s_percentCautionProperty;
	}

	DependencyProperty StorageBar::PercentCriticalProperty()
	{
		EnsureDependencyProperties();
		return s_percentCriticalProperty;
	}

	double StorageBar::ValueBarHeight() const
	{
		return unbox_value<double>(GetValue(ValueBarHeightProperty()));
	}
	void StorageBar::ValueBarHeight(double const value)
	{
		SetValue(ValueBarHeightProperty(), box_value((std::max)(0.0, value)));
	}
	double StorageBar::TrackBarHeight() const
	{
		return unbox_value<double>(GetValue(TrackBarHeightProperty()));
	}
	void StorageBar::TrackBarHeight(double const value)
	{
		SetValue(TrackBarHeightProperty(), box_value((std::max)(0.0, value)));
	}
	OpenNet::UI::Xaml::Control::Progress::Storage::BarShapes StorageBar::BarShape() const
	{
		return unbox_value<OpenNet::UI::Xaml::Control::Progress::Storage::BarShapes>(GetValue(BarShapeProperty()));
	}
	void StorageBar::BarShape(OpenNet::UI::Xaml::Control::Progress::Storage::BarShapes const value)
	{
		SetValue(BarShapeProperty(), box_value(value));
	}
	double StorageBar::Percent() const
	{
		return ValuePercent();
	}
	double StorageBar::PercentCaution() const
	{
		return unbox_value<double>(GetValue(PercentCautionProperty()));
	}
	void StorageBar::PercentCaution(double const value)
	{
		SetValue(PercentCautionProperty(), box_value(std::clamp(value, 0.0, 100.0)));
	}
	double StorageBar::PercentCritical() const
	{
		return unbox_value<double>(GetValue(PercentCriticalProperty()));
	}
	void StorageBar::PercentCritical(double const value)
	{
		SetValue(PercentCriticalProperty(), box_value(std::clamp(value, 0.0, 100.0)));
	}

	void StorageBar::OnLayoutPropertyChanged(DependencyObject const& sender, DependencyPropertyChangedEventArgs const&)
	{
		if (auto control = sender.try_as<class_type>()) get_self<StorageBar>(control)->UpdateControl();
	}

	void StorageBar::OnApplyTemplate()
	{
		m_container = GetTemplateChild(L"PART_Container").try_as<Grid>();
		m_valueColumn = GetTemplateChild(L"PART_ValueColumn").try_as<ColumnDefinition>();
		m_gapColumn = GetTemplateChild(L"PART_GapColumn").try_as<ColumnDefinition>();
		m_trackColumn = GetTemplateChild(L"PART_TrackColumn").try_as<ColumnDefinition>();
		m_valueBar = GetTemplateChild(L"PART_ValueBar").try_as<Border>();
		m_trackBar = GetTemplateChild(L"PART_TrackBar").try_as<Border>();
		UpdateControl();
	}

	void StorageBar::OnValueChanged(double const oldValue, double const newValue)
	{
		(void)oldValue;
		(void)newValue;
		UpdateControl();
	}

	void StorageBar::OnMaximumChanged(double const oldValue, double const newValue)
	{
		(void)oldValue;
		(void)newValue;
		UpdateControl();
	}

	void StorageBar::OnMinimumChanged(double const oldValue, double const newValue)
	{
		(void)oldValue;
		(void)newValue;
		UpdateControl();
	}

	double StorageBar::ValuePercent() const
	{
		auto const range = Maximum() - Minimum();
		if (range <= 0.0) return 0.0;
		return std::clamp((Value() - Minimum()) * 100.0 / range, 0.0, 100.0);
	}

	void StorageBar::UpdateControl()
	{
		if (m_updatingControl || !m_container || !m_valueColumn || !m_gapColumn || !m_trackColumn || !m_valueBar || !m_trackBar) return;
		m_updatingControl = true;
		try
		{
			UpdateHeightsAndCorners();
			UpdateColumnWidths();
			UpdateVisualState();
		}
		catch (...)
		{
			m_updatingControl = false;
			throw;
		}
		m_updatingControl = false;
	}

	void StorageBar::UpdateHeightsAndCorners()
	{
		auto const valueHeight = ValueBarHeight();
		auto const trackHeight = TrackBarHeight();
		if (m_valueBar.Height() != valueHeight) m_valueBar.Height(valueHeight);
		if (m_trackBar.Height() != trackHeight) m_trackBar.Height(trackHeight);
		auto radiusFor = [shape = BarShape()](double const height)
		{
			if (shape == OpenNet::UI::Xaml::Control::Progress::Storage::BarShapes::Round) return height / 2.0;
			if (shape == OpenNet::UI::Xaml::Control::Progress::Storage::BarShapes::Soft) return height / 4.0;
			return 0.0;
		};
		auto const valueRadius = radiusFor(valueHeight);
		auto const trackRadius = radiusFor(trackHeight);
		winrt::Microsoft::UI::Xaml::CornerRadius const valueCorner{ valueRadius, valueRadius, valueRadius, valueRadius };
		winrt::Microsoft::UI::Xaml::CornerRadius const trackCorner{ trackRadius, trackRadius, trackRadius, trackRadius };
		if (m_valueBar.CornerRadius() != valueCorner) m_valueBar.CornerRadius(valueCorner);
		if (m_trackBar.CornerRadius() != trackCorner) m_trackBar.CornerRadius(trackCorner);
		auto const containerHeight = (std::max)(valueHeight, trackHeight);
		if (m_container.Height() != containerHeight) m_container.Height(containerHeight);
	}

	void StorageBar::UpdateColumnWidths()
	{
		auto const setVisibility = [](UIElement const& element, winrt::Microsoft::UI::Xaml::Visibility const value)
		{
			if (element.Visibility() != value) element.Visibility(value);
		};
		auto const setColumn = [](FrameworkElement const& element, int const value)
		{
			if (Grid::GetColumn(element) != value) Grid::SetColumn(element, value);
		};
		auto const setColumnSpan = [](FrameworkElement const& element, int const value)
		{
			if (Grid::GetColumnSpan(element) != value) Grid::SetColumnSpan(element, value);
		};
		auto const setWidth = [](ColumnDefinition const& column, GridLength const& value)
		{
			if (column.Width() != value) column.Width(value);
		};
		auto const percent = ValuePercent();
		if (percent <= 0.0)
		{
			setVisibility(m_valueBar, Visibility::Collapsed);
			setVisibility(m_trackBar, Visibility::Visible);
			setColumn(m_trackBar, 0);
			setColumnSpan(m_trackBar, 3);
			return;
		}
		if (percent >= 100.0)
		{
			setVisibility(m_valueBar, Visibility::Visible);
			setVisibility(m_trackBar, Visibility::Collapsed);
			setColumn(m_valueBar, 0);
			setColumnSpan(m_valueBar, 3);
			return;
		}

		setVisibility(m_valueBar, Visibility::Visible);
		setVisibility(m_trackBar, Visibility::Visible);
		setColumn(m_valueBar, 0);
		setColumnSpan(m_valueBar, 1);
		setColumn(m_trackBar, 2);
		setColumnSpan(m_trackBar, 1);
		auto const padding = Padding();
		auto const width = (std::max)(0.0, ActualWidth() - padding.Left - padding.Right);
		auto const largerHeight = (std::max)(ValueBarHeight(), TrackBarHeight());
		auto const smallerHeight = (std::min)(ValueBarHeight(), TrackBarHeight());
		auto const middleDistance = std::abs(percent - 50.0) / 50.0;
		auto const gap = smallerHeight + (largerHeight - smallerHeight) * middleDistance;
		auto const valueWidth = (std::max)(ValueBarHeight(), width * percent / 100.0 - gap / 2.0);
		setWidth(m_valueColumn, { valueWidth, GridUnitType::Pixel });
		setWidth(m_gapColumn, { gap, GridUnitType::Pixel });
		setWidth(m_trackColumn, { 1.0, GridUnitType::Star });
	}

	void StorageBar::UpdateVisualState()
	{
		hstring const state = !IsEnabled()
			? L"Disabled"
			: ValuePercent() >= PercentCritical()
			? L"Critical"
			: ValuePercent() >= PercentCaution() ? L"Caution" : L"Safe";
		if (state == m_visualState) return;
		m_visualState = state;
		VisualStateManager::GoToState(*this, state, true);
	}
}
