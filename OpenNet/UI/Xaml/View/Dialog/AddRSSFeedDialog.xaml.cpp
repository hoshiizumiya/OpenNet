#include "XamlWorkaround.h"
#include "AddRSSFeedDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/AddRSSFeedDialog.g.cpp")
#include "UI/Xaml/View/Dialog/AddRSSFeedDialog.g.cpp"
#endif

import OpenNet.Helpers.ThemeHelper;
import winrt.Microsoft.UI.Xaml.Controls;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	AddRSSFeedDialog::AddRSSFeedDialog()
	{
		this->Style(Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	void AddRSSFeedDialog::OnPrimaryButtonClick(ContentDialog const& /*sender*/, ContentDialogButtonClickEventArgs const& args)
	{
		auto url = FeedUrlTextBox().Text();
		if (url.empty())
		{
			UrlErrorInfoBar().IsOpen(true);
			args.Cancel(true);
			return;
		}

		UrlErrorInfoBar().IsOpen(false);
		m_feedUrl = url;
		m_feedName = FeedNameTextBox().Text();
		m_feedSavePath = FeedSavePathTextBox().Text();
	}
}
