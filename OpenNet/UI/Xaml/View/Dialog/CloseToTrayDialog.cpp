#include "pch.h"
#include "UI/Xaml/View/Dialog/CloseToTrayDialog.h"
#if __has_include("UI/Xaml/View/Dialog/CloseToTrayDialog.g.cpp")
#include "UI/Xaml/View/Dialog/CloseToTrayDialog.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.h>

#include "Helpers/ThemeHelper.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	CloseToTrayDialog::CloseToTrayDialog()
	{
		InitializeComponent();
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	bool CloseToTrayDialog::RememberChoice()
	{
		return ChkRemember().IsChecked().GetBoolean();
	}
}