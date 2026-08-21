#include "XamlWorkaround.h"
#include "RSSFeedSettingsDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/RSSFeedSettingsDialog.g.cpp")
#include "UI/Xaml/View/Dialog/RSSFeedSettingsDialog.g.cpp"
#endif

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.ThemeHelper;
import winrt.Microsoft.UI.Xaml.Controls;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
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
		SettingsEnabledToggle().IsOn(feed.Enabled());
		SettingsAutoDownloadToggle().IsOn(feed.AutoDownload());
		SettingsFilterTextBox().Text(feed.FilterPattern());

		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		m_itemDoubleClickAction = static_cast<int32_t>(database.GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "item_double_click_action", 0));
		m_enableWebViewPreview = database.GetBool(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "enable_webview_preview", false).value_or(false);
		ItemDoubleClickActionBox().SelectedIndex(m_itemDoubleClickAction);
		IsShowWebViewPreviewFlyout().IsOn(m_enableWebViewPreview);
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
		m_enabled = SettingsEnabledToggle().IsOn();
		m_autoDownload = SettingsAutoDownloadToggle().IsOn();
		m_filterPattern = SettingsFilterTextBox().Text();
		m_itemDoubleClickAction = std::clamp(ItemDoubleClickActionBox().SelectedIndex(), 0, 1);
		m_enableWebViewPreview = IsShowWebViewPreviewFlyout().IsOn();
	}
}
