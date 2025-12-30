#include "pch.h"
#include "TorrentCheckModalWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/TorrentCheckModalWindow.g.cpp")
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <windowsx.h>
#include <winuser.h>
#include "Helpers/WindowHelper.h"
#include "App.xaml.h"
#include "../Pages/TorrentCheckGeneralPage.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;
using namespace winrt::OpenNet::UI::Xaml::View::Pages;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	TorrentCheckModalWindow::TorrentCheckModalWindow()
	{
		InitializeComponent();
		AppWindow().Resize(winrt::Windows::Graphics::SizeInt32(1500, 1800));
		ExtendsContentIntoTitleBar(true);
		AppWindow().TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);
		// Set this modal window's owner (the main application window).
		SetWindowOwner();

		// Make the window modal and show it.
		if (auto presenter = AppWindow().Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
		{
			presenter.IsModal(true);
			AppWindow().SetPresenter(presenter);
		}
		AppWindow().Show();

		Closed({ this, &TorrentCheckModalWindow::ModalWindow_Closed });
	}

	// Sets the owner window of the modal window to the main app window.
	void TorrentCheckModalWindow::SetWindowOwner()
	{
		// Owner: main window exposed by App.xaml.h
		auto const& ownerWindow = winrt::OpenNet::implementation::App::window;
		if (!ownerWindow)
		{
			return; // No owner available yet.
		}

		// Get HWND of the owner (main window).
		HWND ownerHwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(ownerWindow);

		// Get HWND of this modal window's AppWindow.
		auto ownedWindowId = AppWindow().Id();
		HWND ownedHwnd = winrt::Microsoft::UI::GetWindowFromWindowId(ownedWindowId);

		if (ownerHwnd && ownedHwnd)
		{
			// Set the owner relationship so this window becomes modal to the owner.
			::SetWindowLongPtrW(ownedHwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(ownerHwnd));
		}
	}

	void TorrentCheckModalWindow::ModalWindow_Closed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::WindowEventArgs const&)
	{
		// Reactivate the main application window when the modal window closes.
		auto const& ownerWindow = winrt::OpenNet::implementation::App::window;
		if (!ownerWindow)
		{
			return; // No owner available yet.
		}
		ownerWindow.Activate();

	}


	void TorrentCheckModalWindow::TorrentCheckModalWindowSeleterBar_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::SelectorBar const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const& /*args*/)
	{
		auto item = sender.SelectedItem();
		uint32_t currentSelectedIndex;
		sender.Items().IndexOf(item, currentSelectedIndex);
		SlideNavigationTransitionInfo slideInfo{};


		switch (currentSelectedIndex)
		{
		case 0:
			TorrentCheckFrame().Navigate(xaml_typename<TorrentCheckGeneralPage>(), nullptr, slideInfo);
			break;
		case 1:
			//TorrentCheckFrame().Navigate(xaml_typename<>(), nullptr, slideInfo);
			break;
		default:
			break;
		}
		if (currentSelectedIndex > m_selected_index)
		{
			slideInfo.Effect(SlideNavigationTransitionEffect::FromRight);
		}
		else if (currentSelectedIndex < m_selected_index)
		{
			slideInfo.Effect(SlideNavigationTransitionEffect::FromLeft);
		}
		m_selected_index = currentSelectedIndex;

	}

	void TorrentCheckModalWindow::TorrentCreateGrid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);

		if (auto rootGrid = sender.try_as<FrameworkElement>())
		{
			if (auto xamlRoot = rootGrid.XamlRoot())
			{
				xamlRoot.Changed(
					{
						this, & TorrentCheckModalWindow::RootGridXamlRoot_Changed
					}
				);
			}
		}

	}

	void TorrentCheckModalWindow::RootGridXamlRoot_Changed(XamlRoot /*sender*/, XamlRootChangedEventArgs /*args*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);
	}

}
