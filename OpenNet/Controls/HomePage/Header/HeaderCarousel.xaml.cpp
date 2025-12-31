#include "pch.h"
#include "HeaderCarousel.xaml.h"
#if __has_include("Controls/HomePage/Header/HeaderCarousel.g.cpp")
#include "Controls/HomePage/Header/HeaderCarousel.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::Controls::HomePage::Header::implementation
{
	HeaderCarousel::HeaderCarousel()
	{
		InitializeComponent();

	}

}
