#pragma once

#include "UI/Xaml/View/Dialog/ConfirmationDialog.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct ConfirmationDialog : ConfirmationDialogT<ConfirmationDialog>
	{
		ConfirmationDialog();
		void Configure(winrt::hstring const& title, winrt::hstring const& message, winrt::hstring const& details, winrt::hstring const& primaryButtonText, winrt::hstring const& closeButtonText, bool critical, bool showCheckBox, winrt::hstring const& checkBoxText);
		bool IsChecked();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct ConfirmationDialog : ConfirmationDialogT<ConfirmationDialog, implementation::ConfirmationDialog>
	{
	};
}
