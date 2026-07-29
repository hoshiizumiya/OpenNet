#include <Windows.h>

#include "XamlWorkaround.h"
#include "DevWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/DevWindow.g.cpp")
#include "UI/Xaml/View/Windows/DevWindow.g.cpp"
#endif

import OpenNet.Factory.OperationProgressDialog;

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	DevWindow::DevWindow()
	{
		ExtendsContentIntoTitleBar(true);
	}

	void DevWindow::TriggerXamlException_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		throw winrt::hresult_error(E_FAIL, L"Manually triggered XAML exception for testing purposes.");
	}

	winrt::fire_and_forget DevWindow::OpenOperationProgressDialog_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		auto progressDialog = ::OpenNet::Factory::ContentDialog::OperationProgressDialog(L"Operation in Progress", L"Please wait while the operation is being completed.");
		// Show the progress dialog
		co_await progressDialog.ShowAsync();
	}

}
