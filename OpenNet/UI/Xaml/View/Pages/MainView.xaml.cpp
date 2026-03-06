#include "pch.h"
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
#include "UI/Xaml/View/Pages/SettingsPages/MainSettingsPage.xaml.h"

#include <winrt/Windows.Storage.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	MainView::MainView()
	{
		InitializeComponent();

		m_viewModel = winrt::OpenNet::ViewModels::MainViewModel();
		m_viewModel.Initialize();

		// Navigate to saved start page (default: home)
		{
			winrt::hstring startTag = L"home";
			try
			{
				auto localSettings = winrt::Windows::Storage::ApplicationData::Current().LocalSettings();
				auto values = localSettings.Values();
				if (values.HasKey(L"StartPage"))
				{
					startTag = unbox_value_or<hstring>(values.Lookup(L"StartPage"), L"home");
				}
			}
			catch (...) {}

			if (startTag == L"tasks") openTasksPage();
			else if (startTag == L"rss") openRSSPage();
			else if (startTag == L"settings") openSettingsPage();
			else openHomePage();
		}
	}

	winrt::OpenNet::ViewModels::MainViewModel MainView::ViewModel()
	{
		return m_viewModel;
	}

	bool MainView::CanGoBack()
	{
		return NavFrame().CanGoBack();
	}

	void MainView::GoBack()
	{
		if (NavFrame().CanGoBack())
		{
			NavFrame().GoBack();
		}
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
		if (tag.empty()) return;
		auto nav = NavView();
		for (auto const& obj : nav.MenuItems())
		{
			if (auto nvi = obj.try_as<NavigationViewItem>())
			{
				if (unbox_value_or<hstring>(nvi.Tag(), L"") == tag)
				{
					if (nav.SelectedItem() != nvi) nav.SelectedItem(nvi);
					return;
				}
			}
		}
		for (auto const& obj : nav.FooterMenuItems())
		{
			if (auto nvi = obj.try_as<NavigationViewItem>())
			{
				auto itemTag = unbox_value_or<hstring>(nvi.Tag(), L"");
				if (itemTag == tag || (tag == L"settings" && itemTag == L"Settings"))
				{
					if (nav.SelectedItem() != nvi) nav.SelectedItem(nvi);
					return;
				}
			}
		}
	}

	// ─── Page openers ────────────────────────────────────────

	void MainView::openHomePage()
	{
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::HomePage>())
		{ UpdateNavigationSelection(L"home"); return; }
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::HomePage>());
		UpdateNavigationSelection(L"home");
	}
	void MainView::openContactsPage()
	{
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ContactsPage>())
		{ UpdateNavigationSelection(L"contacts"); return; }
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ContactsPage>());
		UpdateNavigationSelection(L"contacts");
	}
	void MainView::openTasksPage()
	{
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>())
		{ UpdateNavigationSelection(L"tasks"); return; }
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>());
		UpdateNavigationSelection(L"tasks");
	}
	void MainView::openFilesPage()
	{
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::FilesPage>())
		{ UpdateNavigationSelection(L"files"); return; }
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::FilesPage>());
		UpdateNavigationSelection(L"files");
	}
	void MainView::openNetworkSettingsPage()
	{
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NetworkSettingsPage>())
		{ UpdateNavigationSelection(L"net"); return; }
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NetworkSettingsPage>());
		UpdateNavigationSelection(L"net");
	}
	void MainView::openServersPage()
	{
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ServersPage>())
		{ UpdateNavigationSelection(L"servers"); return; }
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::ServersPage>());
		UpdateNavigationSelection(L"servers");
	}
	void MainView::openRSSPage()
	{
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::RSSPage>())
		{ UpdateNavigationSelection(L"rss"); return; }
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::RSSPage>());
		UpdateNavigationSelection(L"rss");
	}
	void MainView::openNatToolsPage()
	{
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NatToolsPage>())
		{ UpdateNavigationSelection(L"nattools"); return; }
		NavFrame().Navigate(xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::NatToolsPage>());
		UpdateNavigationSelection(L"nattools");
	}
	void MainView::openSettingsPage()
	{
		if (NavFrame().SourcePageType() == xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::MainSettingsPage>())
		{ UpdateNavigationSelection(L"Settings"); return; }
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
		Navigate(tag);
	}

	void MainView::NavView_SelectionChanged(NavigationView const& /*sender*/, NavigationViewSelectionChangedEventArgs const& args)
	{
		this->DispatcherQueue().TryEnqueue([this, args]() {
			try
			{
				auto selectedItem = args.SelectedItem().try_as<NavigationViewItem>();
				hstring tag = L"";
				if (selectedItem) tag = unbox_value_or<hstring>(selectedItem.Tag(), L"");

				// Update HomeIcon
				if (auto icon = NavView().FindName(L"HomeIcon").try_as<FontIcon>())
				{
					icon.Glyph(tag == L"home" ? L"\uEA8A" : L"\uE80F");
				}
				// Update TasksIcon
				if (auto icon = NavView().FindName(L"TasksIcon").try_as<FontIcon>())
				{
					icon.Glyph(tag == L"tasks" ? L"\uEB91" : L"\uE7C4");
				}
				// Update SettingsIcon
				if (auto sicon = NavView().FindName(L"SettingsIcon").try_as<FontIcon>())
				{
					sicon.Glyph(tag == L"settings" ? L"\uF8B0" : L"\uE713");
				}
			}
			catch (...) {}
		});
	}

	void MainView::Navigate(hstring const& tag)
	{
		auto weak = get_weak();
		this->DispatcherQueue().TryEnqueue([weak, tag]()
		{
			if (auto self = weak.get())
			{
				auto frame = self->NavFrame();
				auto content = frame.Content();

				if (tag == L"home")
				{ if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::HomePage>()) return; self->openHomePage(); return; }
				if (tag == L"contacts")
				{ if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::ContactsPage>()) return; self->openContactsPage(); return; }
				if (tag == L"tasks")
				{ if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::TasksPage>()) return; self->openTasksPage(); return; }
				if (tag == L"files")
				{ if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::FilesPage>()) return; self->openFilesPage(); return; }
				if (tag == L"net")
				{ if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::NetworkSettingsPage>()) return; self->openNetworkSettingsPage(); return; }
				if (tag == L"servers")
				{ if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::ServersPage>()) return; self->openServersPage(); return; }
				if (tag == L"rss")
				{ if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::RSSPage>()) return; self->openRSSPage(); return; }
				if (tag == L"nattools")
				{ if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::NatToolsPage>()) return; self->openNatToolsPage(); return; }
				if (tag == L"settings")
				{ if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::MainSettingsPage>()) return; self->openSettingsPage(); return; }

				if (content && content.try_as<winrt::OpenNet::UI::Xaml::View::Pages::HomePage>()) return;
				self->openHomePage();
			}
		});
	}

	void MainView::NavFrame_Navigating(IInspectable const&, Microsoft::UI::Xaml::Navigation::NavigatingCancelEventArgs const&)
	{
	}

	void MainView::NavFrame_Navigated(IInspectable const&, Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
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

		// Notify parent (MainWindow) about back-button state change
		m_canGoBackChanged(*this, NavFrame().CanGoBack());
	}
}
