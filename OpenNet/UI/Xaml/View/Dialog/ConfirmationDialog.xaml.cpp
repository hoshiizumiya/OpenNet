#include "XamlWorkaround.h"
#include "ConfirmationDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/ConfirmationDialog.g.cpp")
#include "UI/Xaml/View/Dialog/ConfirmationDialog.g.cpp"
#endif

import OpenNet.Helpers.ThemeHelper;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	ConfirmationDialog::ConfirmationDialog()
	{
		Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	void ConfirmationDialog::Configure(hstring const& title, hstring const& message, hstring const& details, hstring const& primaryButtonText, hstring const& closeButtonText, bool const critical, bool const showCheckBox, hstring const& checkBoxText)
	{
		Title(box_value(title));
		MessageText().Text(message);
		DetailsText().Text(details);
		DetailsText().Visibility(details.empty() ? Visibility::Collapsed : Visibility::Visible);
		PrimaryButtonText(primaryButtonText);
		PrimaryButtonStyle(critical ? Resources().Lookup(box_value(L"CriticalDialogPrimaryButtonStyle")).as<Microsoft::UI::Xaml::Style>() : Microsoft::UI::Xaml::Style{ nullptr });
		CloseButtonText(closeButtonText);
		DefaultButton(primaryButtonText.empty() ? ContentDialogButton::Close : ContentDialogButton::Primary);
		CriticalIcon().Visibility(critical ? Visibility::Visible : Visibility::Collapsed);
		OptionCheckBox().Content(box_value(checkBoxText));
		OptionCheckBox().IsChecked(false);
		OptionCheckBox().Visibility(showCheckBox ? Visibility::Visible : Visibility::Collapsed);
	}

	bool ConfirmationDialog::IsChecked()
	{
		auto const value = OptionCheckBox().IsChecked();
		return value && value.Value();
	}
}
