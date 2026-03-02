#pragma once

#include "UI/Xaml/View/Dialog/RSSFeedSettingsDialog.g.h"
#include "ViewModels/RSSViewModel.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct RSSFeedSettingsDialog : RSSFeedSettingsDialogT<RSSFeedSettingsDialog>
	{
		RSSFeedSettingsDialog();

		void OnPrimaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender,
								  winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args);

		void SetFeed(OpenNet::ViewModels::RSSFeedViewModel const& feed);

		winrt::hstring FeedTitle() const { return m_feedTitle; }
		winrt::hstring FeedUrl() const { return m_feedUrl; }
		winrt::hstring FeedSavePath() const { return m_feedSavePath; }
		int32_t UpdateIntervalMinutes() const { return m_updateIntervalMinutes; }
		bool AutoDownload() const { return m_autoDownload; }
		winrt::hstring FilterPattern() const { return m_filterPattern; }

	private:
		winrt::hstring m_feedTitle;
		winrt::hstring m_feedUrl;
		winrt::hstring m_feedSavePath;
		int32_t m_updateIntervalMinutes{ 30 };
		bool m_autoDownload{ false };
		winrt::hstring m_filterPattern;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct RSSFeedSettingsDialog : RSSFeedSettingsDialogT<RSSFeedSettingsDialog, implementation::RSSFeedSettingsDialog>
	{};
}
