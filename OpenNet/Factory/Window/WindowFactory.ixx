export module OpenNet.Factory.Window;

import OpenNet.Helpers.WindowHelper;
import winrt.WinUI3Package;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Windowing;

using namespace winrt;
using namespace ::OpenNet::Helpers;

export namespace OpenNet::Factory::Window
{
	struct WindowFactory
	{
	public:
		static winrt::Microsoft::UI::Xaml::Window CreateStandardWindow()
		{
			WinUI3Package::WindowEx window;

			window.ExtendsContentIntoTitleBar(true);
			window.HasBorder(true);
			window.HasTitleBar(true);
			window.IsResizable(true);
			window.IsMinimizable(true);
			window.IsMaximizable(true);
			OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window.Window());

			// TO Fix: Here's a problem - The window guid will be presisted cause of the window class name is consisted of the winui3package
			// OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Enable(window);
			// window.Activated(
			// 	[window](auto...) {
			// 		window.AppWindow().Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>().SetBorderAndTitleBar(false, false);

			// 	}
			// );

			return window.Window();
		}

		static winrt::Microsoft::UI::Xaml::Window CreateStandardWindowForPage(winrt::Microsoft::UI::Xaml::Controls::Page page)
		{
			WinUI3Package::WindowEx window;

			window.ExtendsContentIntoTitleBar(true);
			window.HasBorder(false);
			window.HasTitleBar(false);
			window.IsResizable(true);
			window.IsMinimizable(true);
			window.IsMaximizable(true);
			window.Content(page);
			OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window.Window());
			window.Activate();
			return window.Window();
		}
	};
}
