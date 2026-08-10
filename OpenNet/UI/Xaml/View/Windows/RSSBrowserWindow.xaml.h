#pragma once

#include "UI/Xaml/View/Windows/RSSBrowserWindow.g.h"

import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Input;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct RSSBrowserTab
	{
		winrt::hstring Id;
		winrt::hstring Title;
		winrt::hstring Url;
		winrt::Microsoft::UI::Xaml::Controls::TabViewItem Item{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::WebView2 WebView{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::TextBlock HeaderText{ nullptr };
		winrt::Microsoft::UI::Xaml::Controls::ProgressRing HeaderProgress{ nullptr };
		bool InitializationStarted{};
	};

	struct RSSBrowserWindow : RSSBrowserWindowT<RSSBrowserWindow>
	{
		RSSBrowserWindow();

		void AddNewTab(winrt::hstring const& url, winrt::hstring const& title);
		void CloseTab(winrt::hstring const& tabId);
		void SwitchToTab(winrt::hstring const& tabId);
		void UpdateTabTitle(winrt::hstring const& tabId, winrt::hstring const& title);

		void BrowserTabView_AddTabButtonClick(winrt::Microsoft::UI::Xaml::Controls::TabView const&, winrt::Windows::Foundation::IInspectable const&);
		void BrowserTabView_TabCloseRequested(winrt::Microsoft::UI::Xaml::Controls::TabView const&, winrt::Microsoft::UI::Xaml::Controls::TabViewTabCloseRequestedEventArgs const& args);
		void BrowserTabView_SelectionChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
		void BrowserHost_Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void BrowserHost_SizeChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
		void BackButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void ForwardButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void ReloadButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void GoButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void AddressBox_KeyDown(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
		winrt::fire_and_forget OpenExternalButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		std::shared_ptr<RSSBrowserTab> CurrentTab();
		std::shared_ptr<RSSBrowserTab> FindTab(winrt::hstring const& id) const;
		winrt::Microsoft::UI::Xaml::UIElement BuildHeader(std::shared_ptr<RSSBrowserTab> const& tab);
		winrt::fire_and_forget InitializeWebViewAsync(std::shared_ptr<RSSBrowserTab> tab);
		void NavigateCurrent(winrt::hstring const& url);
		void UpdateNavigationState();
		void SynchronizeBrowserViewport(winrt::Windows::Foundation::Size const& size);

		std::vector<std::shared_ptr<RSSBrowserTab>> m_tabs;
		std::uint64_t m_nextId{ 1 };
		bool m_browserHostLoaded{};
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct RSSBrowserWindow : RSSBrowserWindowT<RSSBrowserWindow, implementation::RSSBrowserWindow>
	{
	};
}
