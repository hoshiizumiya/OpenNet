#include "pch.h"
#include "NotifyIconContextMenu.xaml.h"
#if __has_include("UI/Shell/NotifyIconContextMenu.g.cpp")
#include "UI/Shell/NotifyIconContextMenu.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Shell::implementation
{
	NotifyIconContextMenu::NotifyIconContextMenu()
	{
		InitializeComponent();
		//NotifyIconContextMenuIcon().Show();
	}
}
