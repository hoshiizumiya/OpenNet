export module OpenNet.Factory.ContentDialog;

import winrt.WinUI3Package;
import winrt.Microsoft.UI.Xaml;

export namespace OpenNet::Factory::ContentDialog
{
	struct ContentDialogFactory
	{
	public:
		static winrt::Microsoft::UI::Xaml::Controls::ContentDialog CreateStandardContentDialog()
		{
			winrt::Microsoft::UI::Xaml::Controls::ContentDialog dialog;
			dialog.XamlRoot(winrt::Microsoft::UI::Xaml::Window::Current().Content().try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>().XamlRoot());
			dialog.DefaultButton(winrt::Microsoft::UI::Xaml::Controls::ContentDialogButton::Close);
			dialog.CloseButtonText(L"Close");
			dialog.IsPrimaryButtonEnabled(false);
			dialog.IsSecondaryButtonEnabled(false);
			return dialog;
		}
	};
}