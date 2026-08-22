#pragma once

#include "UI/Xaml/View/Dialog/NumberInputDialog.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct NumberInputDialog : NumberInputDialogT<NumberInputDialog>
	{
		NumberInputDialog();
		void Configure(winrt::hstring const& title, winrt::hstring const& message, double value, double minimum, double maximum, winrt::hstring const& primaryButtonText, winrt::hstring const& closeButtonText);
		double Value();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct NumberInputDialog : NumberInputDialogT<NumberInputDialog, implementation::NumberInputDialog>
	{
	};
}
