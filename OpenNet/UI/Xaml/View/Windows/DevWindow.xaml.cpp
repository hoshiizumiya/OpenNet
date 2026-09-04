#include <Windows.h>

#include "XamlWorkaround.h"
#include "DevWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/DevWindow.g.cpp")
#include "UI/Xaml/View/Windows/DevWindow.g.cpp"
#endif

#include "LiveGraphTestWindow.xaml.h"

import OpenNet.Factory.OperationProgressDialog;
import OpenNet.Helpers.WindowHelper;
import winrt.Windows.Foundation;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	DevWindow::DevWindow()
	{
		InitializeWindowExBase();
		Closed([this](auto const&, auto const&)
		{
			::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Save(AppWindow());
		});
	}

	void DevWindow::Root_Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		ExtendsContentIntoTitleBar(true);
		SetTitleBar(devWindowTitleBar());
	}

	void DevWindow::OpenLiveGraphTestWindow_Click(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		winrt::make<LiveGraphTestWindow>().Activate();
	}

	void DevWindow::TriggerXamlException_Click(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		throw hresult_error(
			E_FAIL,
			L"Manually triggered XAML exception for testing purposes.");
	}

	void DevWindow::SendAppNotify_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
	{
	}

	fire_and_forget DevWindow::OpenOperationProgressDialog_Click(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		auto progressDialog = ::OpenNet::Factory::ContentDialog::OperationProgressDialog(
			L"Operation in Progress",
			L"Please wait while the operation is being completed.",
			this->XamlRoot());
		co_await progressDialog.ShowAsync();
	}
}
