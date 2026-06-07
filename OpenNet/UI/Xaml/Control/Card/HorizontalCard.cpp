#include "pch.h"
#include "HorizontalCard.h"
#if __has_include("UI/Xaml/Control/Card/HorizontalCard.g.cpp")
#include "UI/Xaml/Control/Card/HorizontalCard.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::Control::Card::implementation
{
	HorizontalCard::HorizontalCard()
	{
		DefaultStyleKey(winrt::box_value(L"OpenNet.UI.Xaml.Control.Card.HorizontalCard"));
	}

	winrt::Microsoft::UI::Xaml::DependencyProperty HorizontalCard::LeftProperty()
	{
		static Microsoft::UI::Xaml::DependencyProperty s_Leftproperty =
			Microsoft::UI::Xaml::DependencyProperty::Register(
				L"Left",
				winrt::xaml_typename<UIElement>(),
				winrt::xaml_typename<OpenNet::UI::Xaml::Control::Card::HorizontalCard>(),
				Microsoft::UI::Xaml::PropertyMetadata{ nullptr });
		return s_Leftproperty;
	}

	winrt::Microsoft::UI::Xaml::DependencyProperty HorizontalCard::RightProperty()
	{
		static Microsoft::UI::Xaml::DependencyProperty s_Rightproperty =
			Microsoft::UI::Xaml::DependencyProperty::Register(
				L"Right",
				winrt::xaml_typename<UIElement>(),
				winrt::xaml_typename<OpenNet::UI::Xaml::Control::Card::HorizontalCard>(),
				Microsoft::UI::Xaml::PropertyMetadata{ nullptr });
		return s_Rightproperty;
	}

	winrt::Microsoft::UI::Xaml::UIElement HorizontalCard::Left()
	{
		return GetValue(LeftProperty()).as<winrt::Microsoft::UI::Xaml::UIElement>();
	}

	void HorizontalCard::Left(winrt::Microsoft::UI::Xaml::UIElement const& value)
	{
		SetValue(LeftProperty(), value);
	}

	winrt::Microsoft::UI::Xaml::UIElement HorizontalCard::Right()
	{
		if (auto value = GetValue(RightProperty()))
		{
			return value.try_as<winrt::Microsoft::UI::Xaml::UIElement>();
		}

		return nullptr;
	}

	void HorizontalCard::Right(winrt::Microsoft::UI::Xaml::UIElement const& value)
	{
		SetValue(RightProperty(), value);
	}

}
