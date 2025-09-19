#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "Pages/HomePage.xaml.h"
#include "Pages/NetworkSettingsPage.xaml.h"
#include "ViewModels/MainViewModel.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace Microsoft::UI::Xaml;
using MainViewModel = winrt::OpenNet::ViewModels::MainViewModel;

namespace winrt::OpenNet::implementation
{
	MainWindow::MainWindow()
	{
		InitializeComponent();
		SetTitleBar(AppTitleBar());
		InitWindowStyle(*this);

		m_viewModel = MainViewModel{};
		m_viewModel.Initialize();

		NavFrame().Navigating({ this, &MainWindow::NavFrame_Navigating });
		NavFrame().Navigated({ this, &MainWindow::NavFrame_Navigated });

		NavigateTag(L"home", nullptr);
	}

	MainViewModel MainWindow::ViewModel()
	{
		return m_viewModel;
	}

#pragma region Window_State_Management
	IAsyncAction MainWindow::SetIconAsync(Microsoft::UI::Windowing::AppWindow window)
	{
		if (!window) co_return;
		try {}
		catch (...) {}
		co_return;
	}

	void MainWindow::InitWindowStyle(Window const& window)
	{
		window.ExtendsContentIntoTitleBar(true);
		if (auto appWindow = window.AppWindow())
		{
			appWindow.TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);
			SetIconAsync(appWindow);
		}
	}
#pragma endregion

#pragma region Navigation
	void MainWindow::AppTitleBar_BackRequested(winrt::Microsoft::UI::Xaml::Controls::TitleBar const&, winrt::Windows::Foundation::IInspectable const&)
	{
		if (NavFrame().CanGoBack()) NavFrame().GoBack();
	}

	void MainWindow::NavView_ItemInvoked(winrt::Microsoft::UI::Xaml::Controls::NavigationView const&, winrt::Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs const& args)
	{
		if (auto item = args.InvokedItemContainer())
		{
			if (auto tagProp = item.Tag(); tagProp)
			{
				auto tag = winrt::unbox_value_or<winrt::hstring>(tagProp, L"");
				if (!tag.empty())
				{
					NavigateTag(tag, nullptr);
				}
			}
		}
	}

	void MainWindow::Navigate(hstring const& tag, IInspectable const& parameter)
	{
		NavigateTag(tag, parameter);
	}

	void MainWindow::NavigateTag(hstring const& tag, IInspectable const& parameter)
	{
		if (tag == L"home")
		{
			if (!NavFrame().Content() || !NavFrame().Content().try_as<winrt::OpenNet::Pages::HomePage>())
			{
				NavigateToType(xaml_typename<winrt::OpenNet::Pages::HomePage>(), NULL);
			}
		}
		else if (tag == L"contacts")
		{
			if (!NavFrame().Content() || !NavFrame().Content().try_as<winrt::OpenNet::Pages::ContactsPage>())
			{
				NavigateToType(xaml_typename<winrt::OpenNet::Pages::ContactsPage>(), parameter ? parameter : NULL);
			}
		}
		else if (tag == L"Tasks")
		{
			if (!NavFrame().Content() || !NavFrame().Content().try_as<winrt::OpenNet::Pages::TasksPage>())
			{
				NavigateToType(xaml_typename<winrt::OpenNet::Pages::TasksPage>(), NULL);
			}
		}
		else if (tag == L"files")
		{
			if (!NavFrame().Content() || !NavFrame().Content().try_as<winrt::OpenNet::Pages::FilesPage>())
			{
				NavigateToType(xaml_typename<winrt::OpenNet::Pages::FilesPage>(), NULL);
			}
		}
		else if (tag == L"net")
		{
			if (!NavFrame().Content() || !NavFrame().Content().try_as<winrt::OpenNet::Pages::NetworkSettingsPage>())
			{
				NavigateToType(xaml_typename<winrt::OpenNet::Pages::NetworkSettingsPage>(), parameter ? parameter : NULL);
			}
		}
		else if (tag == L"servers")
		{
			if (!NavFrame().Content() || !NavFrame().Content().try_as<winrt::OpenNet::Pages::ServersPage>())
			{
				NavigateToType(xaml_typename<winrt::OpenNet::Pages::ServersPage>(), parameter ? parameter : NULL);
			}
		}
		else if (tag == L"settings")
		{
			if (!NavFrame().Content() || !NavFrame().Content().try_as<winrt::OpenNet::Pages::NetworkSettingsPage>())
			{
				NavigateToType(xaml_typename<winrt::OpenNet::Pages::NetworkSettingsPage>(), parameter ? parameter : NULL);
			}
		}
		else
		{
			if (!NavFrame().Content() || !NavFrame().Content().try_as<winrt::OpenNet::Pages::HomePage>())
			{
				NavigateToType(xaml_typename<winrt::OpenNet::Pages::HomePage>(), NULL);
			}
		}
	}

	void MainWindow::NavigateToType(Windows::UI::Xaml::Interop::TypeName const& type, IInspectable const& parameter)
	{
		NavFrame().Navigate(type, parameter);
	}

	void MainWindow::NavFrame_Navigating(IInspectable const& sender, Microsoft::UI::Xaml::Navigation::NavigatingCancelEventArgs const& e)
	{
		if (e.SourcePageType() == xaml_typename<winrt::OpenNet::Pages::ContactsPage>())
		{
			NavView().SelectedItem() = NavView().MenuItems().GetAt(1);
		}
		if (e.SourcePageType() == xaml_typename<winrt::OpenNet::Pages::TasksPage>())
		{
			NavView().SelectedItem() = NavView().MenuItems().GetAt(2);
		}
		if (e.SourcePageType() == xaml_typename<winrt::OpenNet::Pages::FilesPage>())
		{
			NavView().SelectedItem() = NavView().MenuItems().GetAt(3);
			//替换图标为TaskViewExpanded  &#xEB91;
		}
		if (e.SourcePageType() == xaml_typename<winrt::OpenNet::Pages::NetworkSettingsPage>())
		{
			NavView().SelectedItem() = NavView().MenuItems().GetAt(4);
			//替换图标为TaskViewExpanded  &#xEB91;
		}
		if (e.SourcePageType() == xaml_typename<winrt::OpenNet::Pages::ServersPage>())
		{
			NavView().SelectedItem() = NavView().MenuItems().GetAt(5);
			//替换图标为TaskViewExpanded  &#xEB91;
		}
		if (e.SourcePageType() == xaml_typename<winrt::OpenNet::Pages::SettingsPage>())
		{
			NavView().SelectedItem() = NavView().MenuItems().GetAt(6);
			//替换图标为TaskViewExpanded  &#xEB91;
		}
		else
		{
			NavView().SelectedItem() = NavView().MenuItems().First();
		}
	}

	void MainWindow::NavFrame_Navigated(IInspectable const&, Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		if (AppTitleBar().IsBackButtonEnabled())
		{
			//titleBarIcon().Margin({ 0, 0, 8, 0 });
		}
		else
		{
			//titleBarIcon().Margin({ 16, 0, 0, 0 });
		}
	}
#pragma endregion
}
