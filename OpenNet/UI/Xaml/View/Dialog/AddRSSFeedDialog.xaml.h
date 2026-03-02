#pragma once

#include "UI/Xaml/View/Dialog/AddRSSFeedDialog.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct AddRSSFeedDialog : AddRSSFeedDialogT<AddRSSFeedDialog>
	{
		AddRSSFeedDialog();

		void OnPrimaryButtonClick(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& sender,
								  winrt::Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& args);

		winrt::hstring FeedUrl() const { return m_feedUrl; }
		winrt::hstring FeedName() const { return m_feedName; }
		winrt::hstring FeedSavePath() const { return m_feedSavePath; }

	private:
		winrt::hstring m_feedUrl;
		winrt::hstring m_feedName;
		winrt::hstring m_feedSavePath;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct AddRSSFeedDialog : AddRSSFeedDialogT<AddRSSFeedDialog, implementation::AddRSSFeedDialog>
	{};
}
