#include "XamlWorkaround.h"
#include "MainView.xaml.h"
#if __has_include("UI/Xaml/View/Pages/MainView.g.cpp")
#include "UI/Xaml/View/Pages/MainView.g.cpp"
#endif

#include "UI/Xaml/View/Pages/HomePage.xaml.h"
#include "UI/Xaml/View/Pages/ContactsPage.xaml.h"
#include "UI/Xaml/View/Pages/TasksPage.xaml.h"
#include "UI/Xaml/View/Pages/FilesPage.xaml.h"
#include "UI/Xaml/View/Pages/NetworkSettingsPage.xaml.h"
#include "UI/Xaml/View/Pages/ServersPage.xaml.h"
#include "UI/Xaml/View/Pages/RSSPage.xaml.h"
#include "UI/Xaml/View/Pages/NatToolsPage.xaml.h"
#include "UI/Xaml/View/Windows/NATDetectorWindow.xaml.h"
#include "UI/Xaml/View/Pages/SettingsPages/MainSettingsPage.xaml.h"
#include "Service/Background/BackgroundMediaService.h"

import OpenNet.Helpers.WindowHelper;
import winrt.Microsoft.Windows.Storage;
import winrt.Windows.UI.Xaml.Interop;

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	void MainView::InitializeComponent()
	{
		m_viewModel.Initialize();
		MainViewT::InitializeComponent();
		m_backgroundTimer = DispatcherTimer{};
		m_backgroundTimer.Interval(std::chrono::minutes(5));
		m_backgroundTimerToken = m_backgroundTimer.Tick(
			[weak = get_weak()](auto&&, auto&&)
		{
			if (auto self = weak.get())
				self->RefreshBackgroundMediaAsync(true);
		});
		m_backgroundOptionsChangedToken = ::OpenNet::Service::Background::GetBackgroundMediaService().OptionsChanged(
			[weak = get_weak()](auto&&, auto&&)
		{
			if (auto self = weak.get())
				self->RefreshBackgroundMediaAsync(false);
		});
		m_backgroundPresentersChangedToken = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::
			BackgroundPresentersChanged(
				[weak = get_weak()](auto&&, auto&&)
		{
			if (auto self = weak.get())
				self->RefreshBackgroundMediaAsync(false);
		});
	}

	void MainView::MainView_Loaded(IInspectable const&, RoutedEventArgs const&)
	{
		m_isUnloaded = false;
		m_backgroundPlaybackActive = true;
		m_backgroundTimer.Start();
		RefreshBackgroundMediaAsync(false);
		winrt::Windows::UI::Xaml::Interop::TypeName pageTypeName;
		try
		{
			auto localSettings = winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().LocalSettings();
			auto values = localSettings.Values();
			if (values.HasKey(L"StartPage"))
			{
				pageTypeName.Name = unbox_value_or<hstring>(values.Lookup(L"StartPage"), L"OpenNet.UI.Xaml.View.Pages.TasksPage");
				pageTypeName.Kind = winrt::Windows::UI::Xaml::Interop::TypeKind::Metadata;
			}
			if (pageTypeName.Name.starts_with(L"OpenNet.UI.Xaml.View.Pages."))
			{
				Navigate(pageTypeName);
			}
			Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>());
		}
		catch (...)
		{
			Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>());
		}
	}

	void MainView::MainView_Unloaded(IInspectable const&, RoutedEventArgs const&)
	{
		m_isUnloaded = true;
		m_backgroundPlaybackActive = false;
		m_backgroundTimer.Stop();
		::OpenNet::Service::Background::GetBackgroundMediaService().Reset(m_backgroundImagePresenter, m_backgroundVideoPresenter);
	}

	MainView::~MainView()
	{
		::OpenNet::Service::Background::GetBackgroundMediaService().OptionsChanged(m_backgroundOptionsChangedToken);
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::BackgroundPresentersChanged(m_backgroundPresentersChangedToken);
		if (m_backgroundTimer)
		{
			m_backgroundTimer.Stop();
			m_backgroundTimer.Tick(m_backgroundTimerToken);
		}
	}

	void MainView::AttachBackgroundPresenters(Image const& image, MediaPlayerElement const& video)
	{
		m_backgroundImagePresenter = image;
		m_backgroundVideoPresenter = video;
	}

	Image MainView::BackgroundImagePresenter() const
	{
		return m_backgroundImagePresenter;
	}

	MediaPlayerElement MainView::BackgroundVideoPresenter() const
	{
		return m_backgroundVideoPresenter;
	}

	void MainView::SetBackgroundPlaybackActive(bool const active)
	{
		if (active == m_backgroundPlaybackActive) return;
		m_backgroundPlaybackActive = active;
		if (active)
		{
			m_backgroundTimer.Start();
			RefreshBackgroundMediaAsync(false);
		}
		else
		{
			// Match the reference behavior: release the decoder while the main
			// window is hidden/minimized instead of burning CPU in the tray.
			m_backgroundTimer.Stop();
			StopBackgroundMedia();
		}
	}

	void MainView::StopBackgroundMedia()
	{
		::OpenNet::Service::Background::GetBackgroundMediaService().Suspend(BackgroundVideoPresenter());
	}

	winrt::Windows::Foundation::IAsyncAction MainView::RefreshBackgroundMediaAsync(bool const advance)
	{
		auto lifetime = get_strong();
		if (m_isUnloaded) co_return;
		auto& service = ::OpenNet::Service::Background::GetBackgroundMediaService();
		m_backgroundTimer.Interval(service.RotationInterval());
		if (m_backgroundPlaybackActive)
		{
			co_await service.RefreshAsync(BackgroundImagePresenter(), BackgroundVideoPresenter(), advance);
		}
		for (auto const& presenters : ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SecondaryBackgroundPresenters())
		{
			co_await service.RefreshAsync(presenters.ImagePresenter, presenters.VideoPresenter, advance);
		}
	}

	winrt::OpenNet::ViewModels::MainViewModel MainView::ViewModel()
	{
		return m_viewModel;
	}

	bool MainView::CanGoBack()
	{
		if (m_isUnloaded)
		{
			return false;
		}
		else
		{
			return NavFrame().CanGoBack();
		}
	}

	void MainView::GoBack()
	{
		if (m_isUnloaded)
			return;
		if (NavFrame().CanGoBack())
		{
			NavFrame().GoBack();
		}
	}

	bool MainView::CanGoForward()
	{
		if (m_isUnloaded)
		{
			return false;
		}
		else
		{
			return NavFrame().CanGoForward();
		}
	}

	void MainView::GoForward()
	{
		if (m_isUnloaded)
			return;
		if (NavFrame().CanGoForward())
		{
			NavFrame().GoForward();
		}
	}

	winrt::OpenNet::UI::Xaml::View::Pages::TasksPage MainView::CurrentTasksPage()
	{
		if (m_isUnloaded)
		{
			return nullptr;
		}
		return NavFrame().Content().try_as<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>();
	}

	winrt::event_token MainView::CanGoBackChanged(winrt::Windows::Foundation::EventHandler<bool> const& handler)
	{
		return m_canGoBackChanged.add(handler);
	}

	void MainView::CanGoBackChanged(winrt::event_token const& token) noexcept
	{
		m_canGoBackChanged.remove(token);
	}

	void MainView::NavView_SelectionChanged(NavigationView const&, NavigationViewSelectionChangedEventArgs const& args)
	{
		auto const selectedItem = args.SelectedItem().try_as<NavigationViewItem>();
		if (!selectedItem || !selectedItem.Tag())
			return;
		if (!IsLoaded()) return;
		auto typeName = selectedItem.Tag().as<winrt::Windows::UI::Xaml::Interop::TypeName>();
		Navigate(typeName);
	}

	void MainView::Navigate(winrt::Windows::UI::Xaml::Interop::TypeName const& pageType)
	{
		if (m_isUnloaded || !pageType.Name.starts_with(L"OpenNet.UI.Xaml.View.Pages."))
			return;
		auto frame = NavFrame();
		if (frame.SourcePageType() != pageType)
			frame.Navigate(pageType);
	}

	void MainView::NavFrame_Navigated(IInspectable const&, Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		if (m_isUnloaded)
			return;

		m_canGoBackChanged(*this, NavFrame().CanGoBack());
	}

	void MainView::NavFrame_NavigationFailed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Navigation::NavigationFailedEventArgs const& e)
	{
		// https://github.com/microsoft/PowerToys/pull/48457
		// A page constructor or XAML load failure here would otherwise bubble out of the Frame and crash the launcher.
		// Log the failure and mark it handled so the flyout can remain available; the next summon will retry navigation.
		throw winrt::hresult_error(e.Exception(), L"Failed to load Page " + e.SourcePageType().Name);
	}

	void MainView::NavItem_More_Tapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& /*e*/)
	{
		Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase::ShowAttachedFlyout(sender.as<FrameworkElement>());
	}

	void MainView::SettingButton_PointerEntered(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& /*e*/)
	{
		if (m_isUnloaded)
			return;
		AnimatedIcon::SetState(this->AnimatedIcon(), L"PointerOver");
	}

	void MainView::SettingButton_PointerExited(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& /*e*/)
	{
		if (m_isUnloaded)
			return;
		AnimatedIcon::SetState(this->AnimatedIcon(), L"Normal");
	}

	void MainView::PortState_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase::ShowAttachedFlyout(sender.as<FrameworkElement>());
	}

	void MainView::OpenPortTestWindow_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto window = winrt::make_self<winrt::OpenNet::UI::Xaml::View::Windows::implementation::NATDetectorWindow>();
		window->Activate();
	}
}
