#pragma once

#include "UI/Xaml/View/Dialog/TextInputDialog.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct TextInputDialog : TextInputDialogT<TextInputDialog>
	{
		TextInputDialog();
		void Configure(winrt::hstring const& title, winrt::hstring const& header, winrt::hstring const& text, winrt::hstring const& placeholder, winrt::hstring const& primaryButtonText, winrt::hstring const& closeButtonText, bool multiline);
		winrt::hstring InputText();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct TextInputDialog : TextInputDialogT<TextInputDialog, implementation::TextInputDialog>
	{
	};
}
