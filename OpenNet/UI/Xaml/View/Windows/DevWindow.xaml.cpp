#include <Windows.h>

#include "XamlWorkaround.h"
#include "DevWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/DevWindow.g.cpp")
#include "UI/Xaml/View/Windows/DevWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	DevWindow::DevWindow()
	{
	}

	void DevWindow::TriggerXamlException_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		throw winrt::hresult_error(E_FAIL, L"Manually triggered XAML exception for testing purposes.");
	}

}
