#pragma once

#include "UI/Xaml/Markup/UInt32Extension.g.h"

namespace winrt::OpenNet::UI::Xaml::Markup::implementation
{
	struct UInt32Extension : UInt32ExtensionT<UInt32Extension>
	{
		UInt32Extension() = default;

		winrt::hstring Value();
		void Value(winrt::hstring const& value);

		winrt::Windows::Foundation::IInspectable ProvideValue();
		winrt::Windows::Foundation::IInspectable ProvideValue(winrt::Microsoft::UI::Xaml::IXamlServiceProvider const& serviceProvider);

	private:
		hstring m_value;
	};
}

namespace winrt::OpenNet::UI::Xaml::Markup::factory_implementation
{
	struct UInt32Extension : UInt32ExtensionT<UInt32Extension, implementation::UInt32Extension>
	{
	};
}
