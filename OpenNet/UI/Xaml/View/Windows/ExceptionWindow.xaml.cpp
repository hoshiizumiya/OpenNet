#include "pch.h"
#include "ExceptionWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/ExceptionWindow.g.cpp")
#include "UI/Xaml/View/Windows/ExceptionWindow.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	hstring implementation::ExceptionWindow::TraceId()
	{
		return hstring();
	}
	hstring implementation::ExceptionWindow::Exception()
	{
		return hstring();
	}
	hstring implementation::ExceptionWindow::Comment()
	{
		return hstring();
	}
	void implementation::ExceptionWindow::Comment(hstring const& value)
	{
	}
	void ExceptionWindow::ViewWindowExceptionCloseButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{

	}
}
