export module OpenNet.Factory.ContentDialog;

import winrt.WinUI3Package;
import winrt.Microsoft.UI.Xaml;
import OpenNet.Core.Utils.Message;

using namespace winrt;

export namespace OpenNet::Factory::ContentDialog
{
	struct ContentDialogFactory
	{
	public:
		static winrt::Microsoft::UI::Xaml::Controls::ContentDialog CreateStandardContentDialog()
		{
			winrt::Microsoft::UI::Xaml::Controls::ContentDialog dialog;
			dialog.XamlRoot(winrt::Microsoft::UI::Xaml::Window::Current().Content().try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>().XamlRoot());
			dialog.Style(Microsoft::UI::Xaml::Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).try_as<Microsoft::UI::Xaml::Style>());
			dialog.DefaultButton(winrt::Microsoft::UI::Xaml::Controls::ContentDialogButton::Close);
			dialog.CloseButtonText(ResourceGetString(L"CommonClose"));
			dialog.IsPrimaryButtonEnabled(false);
			dialog.IsSecondaryButtonEnabled(false);
			return dialog;
		}
	};
}
