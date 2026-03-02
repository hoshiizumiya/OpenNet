#include "pch.h"
#include "RSSFeedSettingsDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/RSSFeedSettingsDialog.g.cpp")
#include "UI/Xaml/View/Dialog/RSSFeedSettingsDialog.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include "Helpers/ThemeHelper.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	RSSFeedSettingsDialog::RSSFeedSettingsDialog()
	{
		InitializeComponent();
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
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

	void RSSFeedSettingsDialog::OnPrimaryButtonClick(ContentDialog const& /*sender*/, ContentDialogButtonClickEventArgs const& /*args*/)
	{
		m_feedTitle = SettingsTitleTextBox().Text();
		m_feedUrl = SettingsUrlTextBox().Text();
		m_feedSavePath = SettingsSavePathTextBox().Text();
		m_updateIntervalMinutes = static_cast<int32_t>(SettingsIntervalNumberBox().Value());
		m_autoDownload = SettingsAutoDownloadToggle().IsOn();
		m_filterPattern = SettingsFilterTextBox().Text();
	}
}
