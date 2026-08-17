#include "XamlWorkaround.h"
#include "RSSBrowserWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/RSSBrowserWindow.g.cpp")
#include "UI/Xaml/View/Windows/RSSBrowserWindow.g.cpp"
#endif

import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Input;
import winrt.Microsoft.Web.WebView2.Core;
import winrt.Windows.Foundation;
import winrt.Windows.System;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	RSSBrowserWindow::RSSBrowserWindow()
	{
		InitializeComponent();
		ExtendsContentIntoTitleBar(true);
		SetTitleBar(WindowTitleBar());
		::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Enable(*this);
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(*this);
		::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(*this);
		Closed([this](auto const&, auto const&)
		{
			::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Save(*this);
			for (auto const& tab : m_tabs)
			{
				try
				{
					if (tab->WebView) tab->WebView.Close();
				}
				catch (...)
				{
				}
			}
			m_tabs.clear();
		});
	}

	UIElement RSSBrowserWindow::BuildHeader(std::shared_ptr<RSSBrowserTab> const& tab)
	{
		StackPanel panel;
		panel.Orientation(Orientation::Horizontal);
		panel.Spacing(7);
		FontIcon icon;
		icon.Glyph(L"\uE774");
		icon.FontSize(12);
		panel.Children().Append(icon);
		tab->HeaderText = TextBlock{};
		tab->HeaderText.Text(tab->Title.empty() ? L"New tab" : tab->Title);
		tab->HeaderText.MaxWidth(220);
		tab->HeaderText.TextTrimming(TextTrimming::CharacterEllipsis);
		panel.Children().Append(tab->HeaderText);
		tab->HeaderProgress = ProgressRing{};
		tab->HeaderProgress.Width(12);
		tab->HeaderProgress.Height(12);
		tab->HeaderProgress.IsActive(true);
		panel.Children().Append(tab->HeaderProgress);
		return panel;
	}

	void RSSBrowserWindow::AddNewTab(hstring const& url, hstring const& title)
	{
		auto tab = std::make_shared<RSSBrowserTab>();
		tab->Id = L"rss-browser-" + to_hstring(m_nextId++);
		tab->Title = title.empty() ? L"Loading..." : title;
		tab->Url = url.empty() ? L"about:blank" : url;
		tab->WebView = WebView2{};
		tab->WebView.HorizontalAlignment(HorizontalAlignment::Stretch);
		tab->WebView.VerticalAlignment(VerticalAlignment::Stretch);
		tab->WebView.MinWidth(1);
		tab->WebView.MinHeight(1);
		tab->WebView.Loaded([weak = get_weak(), id = tab->Id](auto const&, auto const&)
		{
			if (auto self = weak.get())
			{
				if (auto loadedTab = self->FindTab(id))
				{
					self->InitializeWebViewAsync(std::move(loadedTab));
				}
			}
		});

		tab->Item = TabViewItem{};
		tab->Item.Tag(box_value(tab->Id));
		tab->Item.Header(BuildHeader(tab));
		tab->Item.HorizontalContentAlignment(HorizontalAlignment::Stretch);
		tab->Item.VerticalContentAlignment(VerticalAlignment::Stretch);
		// WebView2 must be the actual TabViewItem content.  An extra host plus a
		// Loaded gate can leave CoreWebView2 initialization waiting on a visual
		// that was measured before the Window acquired its HWND.
		tab->Item.Content(tab->WebView);
		m_tabs.emplace_back(tab);

		// Queue the insertion through TabView's dispatcher. The selected content is
		// realized on the following layout pass; the WebView.Loaded handler above is
		// the authoritative point at which CoreWebView2 may be created.
		auto weak = get_weak();
		auto item = tab->Item;
		if (!BrowserTabView().DispatcherQueue().TryEnqueue(
			[weak, item]()
			{
				if (auto self = weak.get())
				{
					self->BrowserTabView().TabItems().Append(item);
					self->BrowserTabView().SelectedItem(item);
				}
			}))
		{
			BrowserTabView().TabItems().Append(item);
			BrowserTabView().SelectedItem(item);
		}
	}

	void RSSBrowserWindow::BrowserHost_Loaded(IInspectable const&, RoutedEventArgs const&)
	{
		m_browserHostLoaded = true;
		SynchronizeBrowserViewport({
			static_cast<float>(BrowserHost().ActualWidth()),
			static_cast<float>(BrowserHost().ActualHeight()) });
		// The selected WebView's Loaded event starts initialization after TabView has
		// realized its content presenter.
	}

	void RSSBrowserWindow::BrowserHost_SizeChanged(
		IInspectable const&, SizeChangedEventArgs const& args)
	{
		SynchronizeBrowserViewport(args.NewSize());
	}

	void RSSBrowserWindow::SynchronizeBrowserViewport(
		winrt::Windows::Foundation::Size const& size)
	{
		if (size.Width <= 0 || size.Height <= 0) return;
		BrowserTabView().Width(size.Width);
		BrowserTabView().Height(size.Height);
	}

	winrt::fire_and_forget RSSBrowserWindow::InitializeWebViewAsync(std::shared_ptr<RSSBrowserTab> tab)
	{
		if (!tab || tab->InitializationStarted || !tab->WebView.IsLoaded()) co_return;
		tab->InitializationStarted = true;
		auto lifetime = get_strong();
		try
		{
			tab->WebView.NavigationStarting([weak = get_weak(), id = tab->Id](auto const&, auto const&)
			{
				if (auto self = weak.get())
				{
					if (auto current = self->FindTab(id); current && current->HeaderProgress)
						current->HeaderProgress.IsActive(true);
				}
			});
			tab->WebView.NavigationCompleted([weak = get_weak(), id = tab->Id](WebView2 const& sender, auto const&)
			{
				if (auto self = weak.get())
				{
					if (auto current = self->FindTab(id))
					{
						if (current->HeaderProgress) current->HeaderProgress.IsActive(false);
						try
						{
							current->Url = sender.Source().AbsoluteUri();
							auto title = sender.CoreWebView2().DocumentTitle();
							if (!title.empty()) self->UpdateTabTitle(id, title);
						}
						catch (...)
						{
						}
						self->UpdateNavigationState();
					}
				}
			});

			co_await tab->WebView.EnsureCoreWebView2Async();
			if (auto core = tab->WebView.CoreWebView2())
			{
				core.NewWindowRequested([weak = get_weak()](auto const&, auto const& args)
				{
					args.Handled(true);
					if (auto self = weak.get())
						self->AddNewTab(args.Uri(), L"Loading...");
				});
			}
			tab->WebView.Source(winrt::Windows::Foundation::Uri(tab->Url));
		}
		catch (hresult_error const& error)
		{
			// A transient realization/controller failure must not permanently block
			// initialization when the user selects this tab again.
			tab->InitializationStarted = false;
			if (tab->HeaderProgress) tab->HeaderProgress.IsActive(false);
			UpdateTabTitle(tab->Id, error.message());
		}
	}

	std::shared_ptr<RSSBrowserTab> RSSBrowserWindow::FindTab(hstring const& id) const
	{
		auto found = std::ranges::find_if(m_tabs,
										  [&](auto const& tab)
		{
			return tab->Id == id;
		});
		return found == m_tabs.end() ? nullptr : *found;
	}

	std::shared_ptr<RSSBrowserTab> RSSBrowserWindow::CurrentTab()
	{
		if (auto item = BrowserTabView().SelectedItem().try_as<TabViewItem>())
			return FindTab(unbox_value_or<hstring>(item.Tag(), L""));
		return nullptr;
	}

	void RSSBrowserWindow::CloseTab(hstring const& tabId)
	{
		auto found = std::ranges::find_if(m_tabs,
										  [&](auto const& tab)
		{
			return tab->Id == tabId;
		});
		if (found == m_tabs.end()) return;
		try
		{
			if ((*found)->WebView) (*found)->WebView.Close();
		}
		catch (...)
		{
		}
		auto items = BrowserTabView().TabItems();
		for (std::uint32_t index = 0; index < items.Size(); ++index)
		{
			if (unbox_value_or<hstring>(items.GetAt(index).as<TabViewItem>().Tag(), L"")
				== tabId)
			{
				items.RemoveAt(index);
				break;
			}
		}
		m_tabs.erase(found);
		if (m_tabs.empty()) AddNewTab(L"about:blank", L"New tab");
	}

	void RSSBrowserWindow::SwitchToTab(hstring const& tabId)
	{
		if (auto tab = FindTab(tabId))
		{
			BrowserTabView().SelectedItem(tab->Item);
			UpdateNavigationState();
		}
	}

	void RSSBrowserWindow::UpdateTabTitle(hstring const& tabId, hstring const& title)
	{
		if (auto tab = FindTab(tabId))
		{
			tab->Title = title.empty() ? L"New tab" : title;
			if (tab->HeaderText) tab->HeaderText.Text(tab->Title);
			ToolTipService::SetToolTip(tab->Item, box_value(tab->Title));
			if (tab == CurrentTab()) WindowTitleBar().Subtitle(tab->Title);
		}
	}

	void RSSBrowserWindow::UpdateNavigationState()
	{
		auto tab = CurrentTab();
		BackButton().IsEnabled(tab && tab->WebView && tab->WebView.CanGoBack());
		ForwardButton().IsEnabled(tab && tab->WebView && tab->WebView.CanGoForward());
		if (tab)
		{
			AddressBox().Text(tab->Url);
			WindowTitleBar().Subtitle(tab->Title);
		}
	}

	void RSSBrowserWindow::NavigateCurrent(hstring const& url)
	{
		if (url.empty()) return;
		try
		{
			auto tab = CurrentTab();
			if (!tab) return;
			auto normalized = url;
			std::wstring value{ normalized.c_str() };
			if (value.find(L"://") == std::wstring::npos && !value.starts_with(L"about:"))
				normalized = L"https://" + normalized;
			tab->Url = normalized;
			tab->WebView.Source(winrt::Windows::Foundation::Uri(normalized));
		}
		catch (...)
		{
		}
	}

	void RSSBrowserWindow::BrowserTabView_AddTabButtonClick(TabView const&, IInspectable const&)
	{
		AddNewTab(L"about:blank", L"New tab");
	}

	void RSSBrowserWindow::BrowserTabView_TabCloseRequested(
		TabView const&, TabViewTabCloseRequestedEventArgs const& args)
	{
		CloseTab(unbox_value_or<hstring>(args.Tab().Tag(), L""));
	}

	void RSSBrowserWindow::BrowserTabView_SelectionChanged(
		IInspectable const&, SelectionChangedEventArgs const&)
	{
		if (m_browserHostLoaded)
		{
			SynchronizeBrowserViewport({
				static_cast<float>(BrowserHost().ActualWidth()),
				static_cast<float>(BrowserHost().ActualHeight()) });
			if (auto tab = CurrentTab(); tab && tab->WebView.IsLoaded())
			{
				InitializeWebViewAsync(std::move(tab));
			}
		}
		UpdateNavigationState();
	}

	void RSSBrowserWindow::BackButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		if (auto tab = CurrentTab(); tab && tab->WebView.CanGoBack()) tab->WebView.GoBack();
	}

	void RSSBrowserWindow::ForwardButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		if (auto tab = CurrentTab(); tab && tab->WebView.CanGoForward()) tab->WebView.GoForward();
	}

	void RSSBrowserWindow::ReloadButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		if (auto tab = CurrentTab()) tab->WebView.Reload();
	}

	void RSSBrowserWindow::GoButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		NavigateCurrent(AddressBox().Text());
	}

	void RSSBrowserWindow::AddressBox_KeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& args)
	{
		if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
		{
			NavigateCurrent(AddressBox().Text());
			args.Handled(true);
		}
	}

	winrt::fire_and_forget RSSBrowserWindow::OpenExternalButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		auto tab = CurrentTab();
		if (!tab || tab->Url.empty()) co_return;
		try
		{
			co_await winrt::Windows::System::Launcher::LaunchUriAsync(
				winrt::Windows::Foundation::Uri(tab->Url));
		}
		catch (...)
		{
		}
	}
}
