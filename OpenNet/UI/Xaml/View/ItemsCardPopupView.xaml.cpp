#include "XamlWorkaround.h"
#include "ItemsCardPopupView.xaml.h"
#if __has_include("UI/Xaml/View/ItemsCardPopupView.g.cpp")
#include "UI/Xaml/View/ItemsCardPopupView.g.cpp"
#endif

#include "Core/WebUI/WebUIControl.h"
#include "UI/Xaml/View/Windows/GuideWindow.xaml.h"
#include "UI/Xaml/View/Windows/NATDetectorWindow.xaml.h"
#include "UI/Xaml/View/Windows/RuntimeStatusWindow.xaml.h"

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.WindowHelper;
import winrt.Windows.ApplicationModel.DataTransfer;
import winrt.Windows.Foundation;
import winrt.Windows.System;

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	namespace
	{
		std::string AddressForUrl(std::string address)
		{
			if (address == "0.0.0.0" || address == "::")
				return "127.0.0.1";
			if (address.find(':') != std::string::npos
				&& !(address.starts_with('[') && address.ends_with(']')))
			{
				return "[" + address + "]";
			}
			return address;
		}
	}

	hstring ItemsCardPopupView::WebUIUrl() const
	{
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		const auto address = AddressForUrl(
			database.GetString("webui_host", "address")
			.value_or("127.0.0.1"));
		const auto port =
			database.GetInt("webui_host", "port").value_or(8080);
		return to_hstring(
			"http://" + address + ":" + std::to_string(port) + "/");
	}

	void ItemsCardPopupView::RefreshStatus()
	{
		const auto url = WebUIUrl();
		WebUIUrlText().Text(url);
		WebUIStatusText().Text(
			::OpenNet::Core::WebUI::IsWebUIRunning()
			? L"Running"
			: L"Not running");
	}

	void ItemsCardPopupView::OnLoaded(IInspectable const&, RoutedEventArgs const&)
	{
		RefreshStatus();
	}

	fire_and_forget ItemsCardPopupView::OnOpenWebUIClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		co_await winrt::Windows::System::Launcher::LaunchUriAsync(
			winrt::Windows::Foundation::Uri{ WebUIUrl() });
	}

	void ItemsCardPopupView::OnCopyUrlClick(IInspectable const&, RoutedEventArgs const&)
	{
		DataPackage package;
		package.SetText(WebUIUrl());
		Clipboard::SetContent(package);
	}

	void ItemsCardPopupView::OnRuntimeStatusClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto window = winrt::make<winrt::OpenNet::UI::Xaml::View::Windows::implementation::RuntimeStatusWindow>();
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
		window.Activate();
	}

	void ItemsCardPopupView::OnNatDetectionClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto window = winrt::make<winrt::OpenNet::UI::Xaml::View::Windows::implementation::NATDetectorWindow>();
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
		window.Activate();
	}

	void ItemsCardPopupView::OnGuideClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto window = winrt::make<winrt::OpenNet::UI::Xaml::View::Windows::implementation::GuideWindow>();
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
		window.Activate();
	}

}
