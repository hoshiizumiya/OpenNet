#include "pch.h"
#include "InfoOverlayWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/InfoOverlayWindow.g.cpp")
#include "UI/Xaml/View/Windows/InfoOverlayWindow.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	InfoOverlayWindow::InfoOverlayWindow()
	{
		InitializeComponent();
	}
}
