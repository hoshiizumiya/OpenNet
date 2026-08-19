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

	void RSSFeedSettingsDialog::DialogLoaded(::winrt::Windows::Foundation::IInspectable const&, ::winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		ItemDoubleClickActionBox().SelectedIndex(static_cast<int32_t>(::OpenNet::Core::AppSettingsDatabase::Instance().GetInt(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "item_double_click_action", 0)));
		IsShowWebViewPreviewFlyout().IsOn(static_cast<bool>(::OpenNet::Core::AppSettingsDatabase::Instance().GetBool(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "enable_webview_preview", false)));
	}

	void RSSFeedSettingsDialog::SetFeed(OpenNet::ViewModels::RSSFeedViewModel const& feed)
	{
		if (!feed) return;

		SettingsTitleTextBox().Text(feed.Title());
		SettingsUrlTextBox().Text(feed.Url());
		SettingsSavePathTextBox().Text(feed.SavePath());
		SettingsIntervalNumberBox().Value(feed.UpdateIntervalMinutes());
		SettingsAutoDownloadToggle().IsOn(feed.AutoDownload());
		SettingsFilterTextBox().Text(feed.FilterPattern());
	}

	// TODO: Fix
	void RSSFeedSettingsDialog::OnPrimaryButtonClick(ContentDialog const& /*sender*/, ContentDialogButtonClickEventArgs const& /*args*/)
	{
		m_feedTitle = SettingsTitleTextBox().Text();
		m_feedUrl = SettingsUrlTextBox().Text();
		m_feedSavePath = SettingsSavePathTextBox().Text();
		m_updateIntervalMinutes = static_cast<int32_t>(SettingsIntervalNumberBox().Value());
		m_autoDownload = SettingsAutoDownloadToggle().IsOn();
		m_filterPattern = SettingsFilterTextBox().Text();
		m_enableWebViewPreview = IsShowWebViewPreviewFlyout().IsOn();
	}

	void RSSFeedSettingsDialog::ItemDoubleClickActionBox_SelectionChanged(IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
	{
		::OpenNet::Core::AppSettingsDatabase::Instance().SetInt(::OpenNet::Core::AppSettingsDatabase::CAT_RSS, "item_double_click_action", ItemDoubleClickActionBox().SelectedIndex());
	}

}
