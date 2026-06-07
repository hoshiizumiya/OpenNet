#include "pch.h"
#include "ShowTeachingTipAction.h"
#if __has_include("UI/Xaml/Behavior/Action/ShowTeachingTipAction.g.cpp")
#include "UI/Xaml/Behavior/Action/ShowTeachingTipAction.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::Behavior::Action::implementation
{
	DependencyProperty ShowTeachingTipAction::TeachingTipProperty()
	{
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_property =
			DependencyProperty::Register(
				L"TeachingTip",
				winrt::xaml_typename<winrt::Microsoft::UI::Xaml::Controls::TeachingTip>(),
				winrt::xaml_typename<OpenNet::UI::Xaml::Behavior::Action::ShowTeachingTipAction>(),
				Microsoft::UI::Xaml::PropertyMetadata{ nullptr }
			);
		return s_property;
	}

	winrt::Windows::Foundation::IInspectable ShowTeachingTipAction::Execute(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& parameter)
	{
		if (sender == nullptr)
		{
			return winrt::Windows::Foundation::IInspectable();
		}

		if (auto teachingTip = this->ShowTeachingTipAction::TeachingTip())
		{
			teachingTip.IsOpen(true);
		}

		return winrt::Windows::Foundation::IInspectable();
	}

	winrt::Microsoft::UI::Xaml::Controls::TeachingTip ShowTeachingTipAction::TeachingTip()
	{
		return winrt::unbox_value<winrt::Microsoft::UI::Xaml::Controls::TeachingTip>(GetValue(TeachingTipProperty()));
	}

	void ShowTeachingTipAction::TeachingTip(winrt::Microsoft::UI::Xaml::Controls::TeachingTip const& value)
	{
		SetValue(TeachingTipProperty(), winrt::box_value(value));
	}
}
