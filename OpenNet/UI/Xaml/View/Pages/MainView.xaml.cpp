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

import winrt.Microsoft.Windows.Storage;

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	MainView::MainView()
	{
		InitializeComponent();
		m_backgroundTimer = DispatcherTimer{};
		m_backgroundTimer.Interval(std::chrono::minutes(5));
		m_backgroundTimerToken = m_backgroundTimer.Tick(
			[weak = get_weak()](auto&&, auto&&)
		{
			if (auto self = weak.get())
				self->RefreshBackgroundMediaAsync(true);
		});
		m_backgroundOptionsChangedToken =
			::OpenNet::Service::Background::GetBackgroundMediaService().OptionsChanged(
				[weak = get_weak()](auto&&, auto&&)
		{
			if (auto self = weak.get())
				self->RefreshBackgroundMediaAsync(false);
		});

		winrt::weak_ref<winrt::OpenNet::UI::Xaml::View::Pages::implementation::MainView> weakThis = get_weak();
		Loaded([weakThis](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
		{
			auto self = weakThis.get();
			if (!self)
			{
				return;
			}
			self->m_isUnloaded = false;
			self->m_backgroundPlaybackActive = true;
			self->m_backgroundTimer.Start();
			self->RefreshBackgroundMediaAsync(false);
		});
		Unloaded([weakThis](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
		{
			auto self = weakThis.get();
			if (!self)
			{
				return;
			}
			self->m_isUnloaded = true;
			self->m_backgroundPlaybackActive = false;
			self->m_backgroundTimer.Stop();
			self->StopBackgroundMedia();
		});

		m_viewModel = winrt::OpenNet::ViewModels::MainViewModel();
		m_viewModel.Initialize();

		// Navigate to saved start page (default: tasks)
		{
			winrt::hstring startTag = L"tasks";
			try
			{
				auto localSettings = winrt::Microsoft::Windows::Storage::ApplicationData::GetDefault().LocalSettings();
				auto values = localSettings.Values();
				if (values.HasKey(L"StartPage"))
				{
					startTag = unbox_value_or<hstring>(values.Lookup(L"StartPage"), L"tasks");
				}
			}
			catch (...)
			{
			}

			if (startTag == L"tasks") openTasksPage();
			else if (startTag == L"rss") openRSSPage();
			else if (startTag == L"settings") openSettingsPage();
			else openTasksPage();
		}
	}

	MainView::~MainView()
	{
		::OpenNet::Service::Background::GetBackgroundMediaService().OptionsChanged(
			m_backgroundOptionsChangedToken);
		::OpenNet::Service::Background::GetBackgroundMediaService().Reset(
			BackgroundImagePresenter(), BackgroundVideoPresenter());
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
		::OpenNet::Service::Background::GetBackgroundMediaService().Suspend(
			BackgroundVideoPresenter());
	}

	winrt::Windows::Foundation::IAsyncAction MainView::RefreshBackgroundMediaAsync(bool const advance)
	{
		auto lifetime = get_strong();
		if (m_isUnloaded || !m_backgroundPlaybackActive) co_return;
		auto& service = ::OpenNet::Service::Background::GetBackgroundMediaService();
		m_backgroundTimer.Interval(service.RotationInterval());
		co_await service.RefreshAsync(
			BackgroundImagePresenter(), BackgroundVideoPresenter(), advance);
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
		return NavFrame().Content()
			.try_as<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>();
	}

	winrt::event_token MainView::CanGoBackChanged(winrt::Windows::Foundation::EventHandler<bool> const& handler)
	{
		return m_canGoBackChanged.add(handler);
	}

	void MainView::CanGoBackChanged(winrt::event_token const& token) noexcept
	{
		m_canGoBackChanged.remove(token);
	}

	// ─── Selection helpers ───────────────────────────────────

	void MainView::UpdateNavigationSelection(hstring const& tag)
	{
		if (m_isUnloaded) return;
		if (tag.empty()) return;
		try
		{
			NavigationView nav = NavView();
			for (IInspectable const& obj : nav.MenuItems())
			{
				NavigationViewItem nvi = obj.try_as<NavigationViewItem>();
				if (nvi)
				{
					if (unbox_value_or<hstring>(nvi.Tag(), L"") == tag)
					{
						if (nav.SelectedItem() != nvi) nav.SelectedItem(nvi);
						return;
					}
				}
			}
			for (IInspectable const& obj : nav.FooterMenuItems())
			{
				NavigationViewItem nvi = obj.try_as<NavigationViewItem>();
				if (nvi)
				{
					hstring itemTag = unbox_value_or<hstring>(nvi.Tag(), L"");
					if (itemTag == tag || (tag == L"settings" && itemTag == L"Settings"))
					{
						if (nav.SelectedItem() != nvi) nav.SelectedItem(nvi);
						return;
					}
				}
			}
		}
		catch (...)
		{
		}
	}

	// ─── Page openers ────────────────────────────────────────

	void MainView::openHomePage()
	{
		if (m_isUnloaded) return;
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::HomePage>())
		{
			UpdateNavigationSelection(L"home"); return;
		}
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::HomePage>());
		UpdateNavigationSelection(L"home");
	}
	void MainView::openContactsPage()
	{
		if (m_isUnloaded) return;
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ContactsPage>())
		{
			UpdateNavigationSelection(L"contacts"); return;
		}
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ContactsPage>());
		UpdateNavigationSelection(L"contacts");
	}
	void MainView::openTasksPage()
	{
		if (m_isUnloaded) return;
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>())
		{
			UpdateNavigationSelection(L"tasks"); return;
		}
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>());
		UpdateNavigationSelection(L"tasks");
	}
	void MainView::openFilesPage()
	{
		if (m_isUnloaded) return;
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::FilesPage>())
		{
			UpdateNavigationSelection(L"files"); return;
		}
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::FilesPage>());
		UpdateNavigationSelection(L"files");
	}
	void MainView::openNetworkSettingsPage()
	{
		if (m_isUnloaded) return;
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NetworkSettingsPage>())
		{
			UpdateNavigationSelection(L"net"); return;
		}
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NetworkSettingsPage>());
		UpdateNavigationSelection(L"net");
	}
	void MainView::openServersPage()
	{
		if (m_isUnloaded) return;
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ServersPage>())
		{
			UpdateNavigationSelection(L"servers"); return;
		}
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ServersPage>());
		UpdateNavigationSelection(L"servers");
	}
	void MainView::openRSSPage()
	{
		if (m_isUnloaded) return;
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::RSSPage>())
		{
			UpdateNavigationSelection(L"rss"); return;
		}
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::RSSPage>());
		UpdateNavigationSelection(L"rss");
	}
	void MainView::openNatToolsPage()
	{
		if (m_isUnloaded) return;
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NatToolsPage>())
		{
			UpdateNavigationSelection(L"nattools"); return;
		}
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NatToolsPage>());
		UpdateNavigationSelection(L"nattools");
	}
	void MainView::openSettingsPage()
	{
		if (m_isUnloaded) return;
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::MainSettingsPage>())
		{
			UpdateNavigationSelection(L"Settings"); return;
		}
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::MainSettingsPage>());
		UpdateNavigationSelection(L"Settings");
	}

	// ─── NavView event handlers ──────────────────────────────

	void MainView::NavView_ItemInvoked(NavigationView const&, NavigationViewItemInvokedEventArgs const& args)
	{
		hstring tag;
		if (auto container = args.InvokedItemContainer())
		{
			tag = unbox_value_or<hstring>(container.Tag(), L"");
		}
		if (tag == L"more") return;
		Navigate(tag);
	}

	void MainView::NavView_SelectionChanged(NavigationView const& /*sender*/, NavigationViewSelectionChangedEventArgs const& args)
	{
		if (m_isUnloaded) return;

		try
		{
			winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem selectedItem = args.SelectedItem().try_as<NavigationViewItem>();
			hstring tag = L"";
			if (selectedItem) tag = unbox_value_or<hstring>(selectedItem.Tag(), L"");

			if (winrt::Microsoft::UI::Xaml::Controls::FontIcon icon = NavView().FindName(L"HomeIcon").try_as<FontIcon>())
			{
				icon.Glyph(tag == L"home" ? L"\uEA8A" : L"\uE80F");
			}
			if (winrt::Microsoft::UI::Xaml::Controls::FontIcon icon = NavView().FindName(L"TasksIcon").try_as<FontIcon>())
			{
				icon.Glyph(tag == L"tasks" ? L"\uEB91" : L"\uE7C4");
			}
			if (winrt::Microsoft::UI::Xaml::Controls::FontIcon sicon = NavView().FindName(L"SettingsIcon").try_as<FontIcon>())
			{
				sicon.Glyph(tag == L"settings" ? L"\uF8B0" : L"\uE713");
			}
		}
		catch (...)
		{
		}
	}

	void MainView::Navigate(hstring const& tag)
	{
		if (m_isUnloaded) return;

		if (DispatcherQueue().HasThreadAccess())
		{
			auto frame = NavFrame();
			auto content = frame.Content();

			if (tag == L"home")
			{
				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::HomePage>()) return; openHomePage(); return;
			}
			if (tag == L"contacts")
			{
				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::ContactsPage>()) return; openContactsPage(); return;
			}
			if (tag == L"tasks")
			{
				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>()) return; openTasksPage(); return;
			}
			if (tag == L"files")
			{
				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::FilesPage>()) return; openFilesPage(); return;
			}
			if (tag == L"net")
			{
				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::NetworkSettingsPage>()) return; openNetworkSettingsPage(); return;
			}
			if (tag == L"servers")
			{
				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::ServersPage>()) return; openServersPage(); return;
			}
			if (tag == L"rss")
			{
				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::RSSPage>()) return; openRSSPage(); return;
			}
			if (tag == L"nattools")
			{
				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::NatToolsPage>()) return; openNatToolsPage(); return;
			}
			if (tag == L"settings")
			{
				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::MainSettingsPage>()) return; openSettingsPage(); return;
			}

			if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::HomePage>()) return;
			openTasksPage();
			return;
		}

		winrt::weak_ref<winrt::OpenNet::UI::Xaml::View::Pages::implementation::MainView> weak = get_weak();
		DispatcherQueue().TryEnqueue([weak, tag]()
		{
			auto self = weak.get();
			if (!self)
			{
				return;
			}
			if (self->m_isUnloaded) return;
			self->Navigate(tag);
		});
	}

	void MainView::NavFrame_Navigating(IInspectable const&, Microsoft::UI::Xaml::Navigation::NavigatingCancelEventArgs const&)
	{
	}

	void MainView::NavFrame_Navigated(IInspectable const&, Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		if (m_isUnloaded) return;

		auto name = e.SourcePageType().Name;
		hstring tag;
		if (name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::HomePage>().Name) tag = L"home";
		else if (name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ContactsPage>().Name) tag = L"contacts";
		else if (name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>().Name) tag = L"tasks";
		else if (name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::FilesPage>().Name) tag = L"files";
		else if (name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NetworkSettingsPage>().Name) tag = L"net";
		else if (name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ServersPage>().Name) tag = L"servers";
		else if (name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::RSSPage>().Name) tag = L"rss";
		else if (name == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::MainSettingsPage>().Name) tag = L"Settings";
		if (!tag.empty()) UpdateNavigationSelection(tag);

		m_canGoBackChanged(*this, NavFrame().CanGoBack());
	}

	void MainView::NavFrame_NavigationFailed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Navigation::NavigationFailedEventArgs const& e)
	{
		// https://github.com/microsoft/PowerToys/pull/48457
		// A page constructor or XAML load failure here would otherwise bubble out of the Frame and crash the launcher.
		// Log the failure and mark it handled so the flyout can remain available; the next summon will retry navigation.
		// TODO: uniform exception handle
		OutputDebugStringW((L"Navigation failed to " + e.SourcePageType().Name + L": " + e.Exception() + L"\n").c_str());
		e.Handled(true);
	}

	void MainView::NavItem_More_Tapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& e)
	{
		Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase::ShowAttachedFlyout(sender.as<FrameworkElement>());
	}

	void MainView::SettingButton_PointerEntered(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& /*e*/)
	{
		if (m_isUnloaded) return;
		AnimatedIcon::SetState(this->AnimatedIcon(), L"PointerOver");
	}

	void MainView::SettingButton_PointerExited(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& /*e*/)
	{
		if (m_isUnloaded) return;
		AnimatedIcon::SetState(this->AnimatedIcon(), L"Normal");
	}

	void MainView::PortState_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto window = winrt::make_self<
			winrt::OpenNet::UI::Xaml::View::Windows::implementation::NATDetectorWindow>();
		window->Activate();
	}
}
