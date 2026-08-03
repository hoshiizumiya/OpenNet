#include <windows.h>
#include <Shlwapi.h>
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <wil/resource.h>
#include <resource.h>

#include "XamlWorkaround.h"
#include "MainWindow.xaml.h"
#include "UI/Xaml/View/Pages/MainView.xaml.h"
#include "UI/Xaml/View/Pages/TasksPage.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.DownloadManager;
import OpenNet.Helpers.WindowHelper;
import winrtplus.Microsoft.UI.Interop;
import winrt.Microsoft.UI.Windowing;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Microsoft.UI.Xaml.Media.Imaging;
import winrt.OpenNet.UI.Xaml.View.Pages;

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Windowing;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace ::OpenNet::Helpers::WinUIWindowHelper;

namespace winrt::OpenNet::implementation
{
	MainWindow::MainWindow()
	{
		InitializeComponent();
		winrt::get_self<winrt::OpenNet::UI::Xaml::View::Pages::implementation::
			MainView>(MainContentView())->AttachBackgroundPresenters(
				BackgroundImagePresenter(), BackgroundVideoPresenter());
		SetTitleBar(AppTitleBar());
		InitWindowStyle(*this);

		AppWindow().SetIcon(L"Assets/AppIcons/win3264.ico");

		// Listen for back-button state changes from MainView
		m_canGoBackChangedToken = MainContentView().CanGoBackChanged([this](IInspectable const&, bool canGoBack)
		{
			try
			{
				AppTitleBar().IsBackButtonVisible(canGoBack);
			}
			catch (...)
			{
			}
		});
		m_appWindowChangedToken = AppWindow().Changed(
			[this](auto const&, auto const&)
			{
				UpdateBackgroundPlaybackState();
			});

		Closed([this](auto&&, auto&&)
		{
			try
			{
				MainContentView().CanGoBackChanged(m_canGoBackChangedToken);
			}
			catch (...)
			{
			}
			try
			{
				AppWindow().Changed(m_appWindowChangedToken);
			}
			catch (...)
			{
			}

			PlacementRestoration::Save(*this);

			// Stop ViewModel background thread (speed refresh)
			try
			{
				winrt::OpenNet::ViewModels::MainViewModel vm = ViewModel();
				if (vm)
				{
					vm.Shutdown();
				}
			}
			catch (...)
			{
				OutputDebugStringA("MainWindow: ViewModel shutdown error\n");
			}
		});
	}

	void MainWindow::UpdateBackgroundPlaybackState()
	{
		try
		{
			auto const appWindow = AppWindow();
			bool active = appWindow.IsVisible();
			if (auto presenter = appWindow.Presenter().try_as<OverlappedPresenter>())
				active = active
					&& presenter.State() != OverlappedPresenterState::Minimized;

			auto mainView = winrt::get_self<winrt::OpenNet::UI::Xaml::View::Pages::
				implementation::MainView>(MainContentView());
			mainView->SetBackgroundPlaybackActive(active);
		}
		catch (...)
		{
		}
	}

	void MainWindow::InvertAppThemeButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		RootGrid().RequestedTheme(RootGrid().RequestedTheme() == ElementTheme::Dark ? ElementTheme::Light : ElementTheme::Dark);
	}

	winrt::OpenNet::ViewModels::MainViewModel MainWindow::ViewModel()
	{
		return MainContentView().ViewModel();
	}

	void MainWindow::Navigate(hstring const& tag)
	{
		MainContentView().Navigate(tag);
	}

	winrt::Windows::Foundation::IAsyncAction MainWindow::ShowAddTaskDialogAsync(
		hstring const& kind)
	{
		auto strong = get_strong();
		Navigate(L"tasks");

		auto mainViewImpl = winrt::get_self<
			winrt::OpenNet::UI::Xaml::View::Pages::implementation::MainView>(
				MainContentView());
		auto tasksPage = mainViewImpl->CurrentTasksPage();
		if (!tasksPage)
		{
			co_return;
		}

		auto tasksPageImpl = winrt::get_self<
			winrt::OpenNet::UI::Xaml::View::Pages::implementation::TasksPage>(
				tasksPage);
		RoutedEventArgs args;

		if (kind == L"file")
		{
			co_await tasksPageImpl->MenuItemAddFromFile_ClickAsync(tasksPage, args);
		}
		else if (kind == L"url")
		{
			co_await tasksPageImpl->MenuItemAddFromLink_ClickAsync(tasksPage, args);
		}
		else if (kind == L"http")
		{
			co_await tasksPageImpl->MenuItemAddFromHttp_ClickAsync(tasksPage, args);
		}
		else if (kind == L"http-batch")
		{
			TextBox urls;
			urls.AcceptsReturn(true);
			urls.TextWrapping(TextWrapping::Wrap);
			urls.MinWidth(420);
			urls.MinHeight(180);
			urls.PlaceholderText(
				L"Enter one HTTP, HTTPS, or FTP URL per line");

			ContentDialog dialog;
			dialog.XamlRoot(tasksPage.XamlRoot());
			dialog.Title(box_value(L"HTTP/FTP Batch Download"));
			dialog.Content(urls);
			dialog.PrimaryButtonText(L"Add");
			dialog.CloseButtonText(L"Cancel");
			dialog.DefaultButton(ContentDialogButton::Primary);

			if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
			{
				co_return;
			}

			std::vector<std::string> parsedUrls;
			std::wistringstream lines{ std::wstring(urls.Text()) };
			for (std::wstring line; std::getline(lines, line);)
			{
				const auto first = line.find_first_not_of(L" \t\r");
				if (first == std::wstring::npos)
				{
					continue;
				}
				const auto last = line.find_last_not_of(L" \t\r");
				line = line.substr(first, last - first + 1);

				std::wstring lower = line;
				std::transform(
					lower.begin(), lower.end(), lower.begin(), ::towlower);
				if (lower.starts_with(L"http://")
					|| lower.starts_with(L"https://")
					|| lower.starts_with(L"ftp://"))
				{
					parsedUrls.push_back(winrt::to_string(line));
				}
			}

			if (parsedUrls.empty())
			{
				co_return;
			}

			co_await ::OpenNet::Core::DownloadManager::Instance().InitializeAsync();
			co_await winrt::resume_background();
			auto& downloadManager = ::OpenNet::Core::DownloadManager::Instance();
			for (auto const& url : parsedUrls)
			{
				downloadManager.AddHttpDownload(url);
			}
		}
	}

	void MainWindow::InitWindowStyle(Window const& window)
	{
		window.ExtendsContentIntoTitleBar(true);
		if (auto appWindow = window.AppWindow())
		{
			appWindow.TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Tall);
			PlacementRestoration::Enable(*this);
#ifdef _DEBUG
			{
				AppTitleBar().Subtitle(L"Dev");
			}
#endif
		}
	}

	void MainWindow::AppTitleBar_BackRequested(Microsoft::UI::Xaml::Controls::TitleBar const&, IInspectable const&)
	{
		MainContentView().GoBack();
	}

	void MainWindow::Grid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);

		if (auto rootGrid = sender.try_as<FrameworkElement>())
		{
			if (auto xamlRoot = rootGrid.XamlRoot())
			{
				xamlRoot.Changed({ this, &MainWindow::RootGridXamlRoot_Changed });
			}
		}
	}

	void MainWindow::RootGrid_PointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
	{
		auto props = e.GetCurrentPoint(nullptr).Properties();

		if (props.IsXButton1Pressed())
		{
			if (MainContentView().CanGoBack())
			{
				MainContentView().GoBack();
				e.Handled(true);
			}
		}
		else if (props.IsXButton2Pressed())
		{
			if (MainContentView().CanGoForward())
			{
				MainContentView().GoForward();
				e.Handled(true);
			}
		}
	}

	Microsoft::UI::Xaml::Visibility MainWindow::IsDebug()
	{
#ifdef _DEBUG
		{
			return Microsoft::UI::Xaml::Visibility::Visible;
		}
#endif
		return Microsoft::UI::Xaml::Visibility::Collapsed;
	}

	void MainWindow::RootGridXamlRoot_Changed(XamlRoot /*sender*/, XamlRootChangedEventArgs /*args*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);
	}
}
