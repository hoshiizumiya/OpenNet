#include "XamlWorkaround.h"
#include "NumberInputDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/NumberInputDialog.g.cpp")
#include "UI/Xaml/View/Dialog/NumberInputDialog.g.cpp"
#endif

import OpenNet.Helpers.ThemeHelper;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	NumberInputDialog::NumberInputDialog()
	{
		Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	void NumberInputDialog::Configure(hstring const& title, hstring const& message, double const value, double const minimum, double const maximum, hstring const& primaryButtonText, hstring const& closeButtonText)
	{
		Title(box_value(title));
		PrimaryButtonText(primaryButtonText);
		CloseButtonText(closeButtonText);
		MessageText().Text(message);
		ValueNumberBox().Minimum(minimum);
		ValueNumberBox().Maximum(maximum);
		ValueNumberBox().Value(value);
	}

	double NumberInputDialog::Value()
	{
		return ValueNumberBox().Value();
	}
}
