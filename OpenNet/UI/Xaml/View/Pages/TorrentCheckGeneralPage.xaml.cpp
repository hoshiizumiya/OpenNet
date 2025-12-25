#include "pch.h"
#include "TorrentCheckGeneralPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.cpp")
#include "UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TorrentCheckGeneralPage::TorrentCheckGeneralPage()
	{
		InitializeComponent();
	}
}
