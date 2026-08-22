#include "XamlWorkaround.h"
#include "TextInputDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/TextInputDialog.g.cpp")
#include "UI/Xaml/View/Dialog/TextInputDialog.g.cpp"
#endif

import OpenNet.Helpers.ThemeHelper;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	TextInputDialog::TextInputDialog()
	{
		Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	void TextInputDialog::Configure(hstring const& title, hstring const& header, hstring const& text, hstring const& placeholder, hstring const& primaryButtonText, hstring const& closeButtonText, bool const multiline)
	{
		Title(box_value(title));
		PrimaryButtonText(primaryButtonText);
		CloseButtonText(closeButtonText);
		InputTextBox().Header(box_value(header));
		InputTextBox().Text(text);
		InputTextBox().PlaceholderText(placeholder);
		InputTextBox().AcceptsReturn(multiline);
		InputTextBox().TextWrapping(multiline ? TextWrapping::NoWrap : TextWrapping::Wrap);
		InputTextBox().MinHeight(multiline ? 260.0 : 0.0);
	}

	hstring TextInputDialog::InputText()
	{
		return InputTextBox().Text();
	}
}
