#include "pch.h"
#include "UInt32Extension.h"
#if __has_include("UI/Xaml/Markup/UInt32Extension.g.cpp")
#include "UI/Xaml/Markup/UInt32Extension.g.cpp"
#endif

#include "winrt/Microsoft.UI.Xaml.Markup.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;


namespace winrt::OpenNet::UI::Xaml::Markup::implementation
{
	hstring UInt32Extension::Value()
	{
		return m_value;
	}

	void UInt32Extension::Value(hstring const& value)
	{
		m_value = value;
	}

	winrt::Windows::Foundation::IInspectable UInt32Extension::ProvideValue()
	{
		return Microsoft::UI::Xaml::Markup::XamlBindingHelper::ConvertValue(winrt::xaml_typename<uint32_t>(), box_value(UInt32Extension::m_value));
	}

	winrt::Windows::Foundation::IInspectable UInt32Extension::ProvideValue(winrt::Microsoft::UI::Xaml::IXamlServiceProvider const& /*serviceProvider*/)
	{
		return Microsoft::UI::Xaml::Markup::XamlBindingHelper::ConvertValue(winrt::xaml_typename<uint32_t>(), box_value(UInt32Extension::m_value));
	}
}
