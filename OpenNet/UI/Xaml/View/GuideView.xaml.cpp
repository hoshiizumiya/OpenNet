#include "XamlWorkaround.h"
#include "GuideView.xaml.h"
#if __has_include("UI/Xaml/View/GuideView.g.cpp")
#include "UI/Xaml/View/GuideView.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	GuideView::GuideView()
	{
		InitializeComponent();
	}
}
