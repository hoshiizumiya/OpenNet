#include "XamlWorkaround.h"
#include "UI/Xaml/Control/Theme/MaterialControlBridge.h"
#include "RSSFeedSettingsDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/RSSFeedSettingsDialog.g.cpp")
#include "UI/Xaml/View/Dialog/RSSFeedSettingsDialog.g.cpp"
#endif

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.ThemeHelper;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.WinUI.LiquidGlass;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	namespace
	{
		std::optional<bool> ToggleIsOn(IInspectable const& sender)
		{
			if (!sender) return std::nullopt;
			if (auto native = sender.try_as<ToggleSwitch>()) return native.IsOn();
			return std::nullopt;
		}

		void SetToggleIsOn(IInspectable const& target, bool value)
		{
			if (!target) return;
			if (auto native = target.try_as<ToggleSwitch>()) native.IsOn(value);
		}
	}

	RSSFeedSettingsDialog::RSSFeedSettingsDialog()
	{
		this->Style(Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	void RSSFeedSettingsDialog::SetFeed(OpenNet::ViewModels::RSSFeedViewModel const& feed)
	{
		if (!feed) return;

		SettingsTitleTextBox().Text(feed.Title());
		SettingsUrlTextBox().Text(feed.Url());
		SettingsSavePathTextBox().Text(feed.SavePath());
		SettingsIntervalNumberBox().Value(feed.UpdateIntervalMinutes());
		m_enabled = feed.Enabled();
		m_autoDownload = feed.AutoDownload();
		m_loadingSwitch = true;
		SetToggleIsOn(SettingsEnabledToggle(), m_enabled);
		SetToggleIsOn(SettingsAutoDownloadToggle(), m_autoDownload);
		m_loadingSwitch = false;
		SettingsFilterTextBox().Text(feed.FilterPattern());

		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		m_itemDoubleClickAction = static_cast<int32_t>(database.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "item_double_click_action", 0));
		m_enableWebViewPreview = database.GetBool(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "enable_webview_preview", false).value_or(false);
		ItemDoubleClickActionBox().SelectedIndex(m_itemDoubleClickAction);
		m_loadingSwitch = true;
		SetToggleIsOn(IsShowWebViewPreviewFlyout(), m_enableWebViewPreview);
		m_loadingSwitch = false;
	}

	void RSSFeedSettingsDialog::OnPrimaryButtonClick(ContentDialog const&, ContentDialogButtonClickEventArgs const& args)
	{
		auto const url = SettingsUrlTextBox().Text();
		auto const interval = SettingsIntervalNumberBox().Value();
		std::wstring normalizedUrl{ url };
		std::ranges::transform(normalizedUrl, normalizedUrl.begin(), [](wchar_t const value)
		{
			return static_cast<wchar_t>(std::towlower(value));
		});
		auto const validScheme = normalizedUrl.starts_with(L"http://") || normalizedUrl.starts_with(L"https://");
		if (url.empty() || !validScheme || !std::isfinite(interval) || interval < 5.0 || interval > 1440.0)
		{
			SettingsValidationInfoBar().IsOpen(true);
			args.Cancel(true);
			return;
		}

		SettingsValidationInfoBar().IsOpen(false);
		m_feedTitle = SettingsTitleTextBox().Text();
		m_feedUrl = url;
		m_feedSavePath = SettingsSavePathTextBox().Text();
		m_updateIntervalMinutes = static_cast<int32_t>(interval);
		m_filterPattern = SettingsFilterTextBox().Text();
		m_itemDoubleClickAction = std::clamp(ItemDoubleClickActionBox().SelectedIndex(), 0, 1);
	}

	void RSSFeedSettingsDialog::SettingsEnabledToggle_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		m_loadingSwitch = true;
		SetToggleIsOn(sender, m_enabled);
		m_loadingSwitch = false;
	}

	void RSSFeedSettingsDialog::SettingsEnabledToggle_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_loadingSwitch) return;
		if (auto value = ToggleIsOn(sender))
		{
			m_enabled = *value;
			m_loadingSwitch = true;
			SetToggleIsOn(SettingsEnabledToggle(), *value);
			m_loadingSwitch = false;
		}
	}

	void RSSFeedSettingsDialog::SettingsAutoDownloadToggle_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		m_loadingSwitch = true;
		SetToggleIsOn(sender, m_autoDownload);
		m_loadingSwitch = false;
	}

	void RSSFeedSettingsDialog::SettingsAutoDownloadToggle_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_loadingSwitch) return;
		if (auto value = ToggleIsOn(sender))
		{
			m_autoDownload = *value;
			m_loadingSwitch = true;
			SetToggleIsOn(SettingsAutoDownloadToggle(), *value);
			m_loadingSwitch = false;
		}
	}

	void RSSFeedSettingsDialog::WebViewPreviewToggle_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		m_loadingSwitch = true;
		SetToggleIsOn(sender, m_enableWebViewPreview);
		m_loadingSwitch = false;
	}

	void RSSFeedSettingsDialog::WebViewPreviewToggle_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_loadingSwitch) return;
		if (auto value = ToggleIsOn(sender))
		{
			m_enableWebViewPreview = *value;
			m_loadingSwitch = true;
			SetToggleIsOn(IsShowWebViewPreviewFlyout(), *value);
			m_loadingSwitch = false;
		}
	}
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
    void RSSFeedSettingsDialog::MaterialLoaded_50(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
    {
        ::OpenNet::UI::EnsureMaterialHostBridge(sender.as<winrt::Microsoft::UI::Xaml::DependencyObject>());
        SettingsEnabledToggle_Loaded(SettingsEnabledToggle(), args);
    }
    void RSSFeedSettingsDialog::MaterialLoaded_51(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
    {
        ::OpenNet::UI::EnsureMaterialHostBridge(sender.as<winrt::Microsoft::UI::Xaml::DependencyObject>());
        SettingsAutoDownloadToggle_Loaded(SettingsAutoDownloadToggle(), args);
    }
    void RSSFeedSettingsDialog::MaterialLoaded_54(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
    {
        ::OpenNet::UI::EnsureMaterialHostBridge(sender.as<winrt::Microsoft::UI::Xaml::DependencyObject>());
        WebViewPreviewToggle_Loaded(IsShowWebViewPreviewFlyout(), args);
    }
}
