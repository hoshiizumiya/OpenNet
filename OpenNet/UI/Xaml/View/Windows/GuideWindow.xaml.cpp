#include "XamlWorkaround.h"
#include "GuideWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/GuideWindow.g.cpp")
#include "UI/Xaml/View/Windows/GuideWindow.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	GuideWindow::GuideWindow()
	{
	}
}
