#include "XamlWorkaround.h"
#include "UI/Xaml/View/Dialog/CloseToTrayDialog.h"
#if __has_include("UI/Xaml/View/Dialog/CloseToTrayDialog.g.cpp")
#include "UI/Xaml/View/Dialog/CloseToTrayDialog.g.cpp"
#endif

import OpenNet.Helpers.ThemeHelper;
import winrt.Microsoft.UI.Xaml;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	CloseToTrayDialog::CloseToTrayDialog()
	{
		InitializeComponent();
		this->Style(Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	bool CloseToTrayDialog::RememberChoice()
	{
		return ChkRemember().IsChecked().GetBoolean();
	}
}