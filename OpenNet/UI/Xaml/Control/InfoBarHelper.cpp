#include "XamlWorkaround.h"
#include "InfoBarHelper.h"
#if __has_include("UI/Xaml/Control/InfoBarHelper.g.cpp")
#include "UI/Xaml/Control/InfoBarHelper.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	DependencyProperty InfoBarHelper::s_isTextSelectionEnabledProperty =
		DependencyProperty::RegisterAttached(
			L"IsTextSelectionEnabled",
			xaml_typename<bool>(),
			xaml_typename<class_type>(),
			PropertyMetadata{ box_value(false) });

	DependencyProperty InfoBarHelper::IsTextSelectionEnabledProperty()
	{
		return s_isTextSelectionEnabledProperty;
	}

	bool InfoBarHelper::GetIsTextSelectionEnabled(
		DependencyObject const& obj)
	{
		return unbox_value_or<bool>(
			obj.GetValue(IsTextSelectionEnabledProperty()), false);
	}

	void InfoBarHelper::SetIsTextSelectionEnabled(
		DependencyObject const& obj,
		bool enabled)
	{
		obj.SetValue(IsTextSelectionEnabledProperty(), box_value(enabled));
	}
}
