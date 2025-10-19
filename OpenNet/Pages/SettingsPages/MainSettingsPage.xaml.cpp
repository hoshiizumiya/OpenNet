#include "pch.h"
#include "MainSettingsPage.xaml.h"
#if __has_include("Pages/SettingsPages/MainSettingsPage.g.cpp")
#include "Pages/SettingsPages/MainSettingsPage.g.cpp"
#endif

#include "Folder.h"
#include "SettingsPage.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
	MainSettingsPage* MainSettingsPage::s_current{ nullptr };

	MainSettingsPage::MainSettingsPage()
	{
		InitializeComponent();
		DataContext() = *this;
		s_current = this;

		// Navigate to SettingsPage by default
		SettingsFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::SettingsPages::SettingsPage>());

		// Populate MainSettingsPageBar with a single Folder item using localized resource if available
		auto items = single_threaded_observable_vector<winrt::OpenNet::Pages::SettingsPages::Folder>();
		auto folder = winrt::OpenNet::Pages::SettingsPages::Folder();

		// Try to get resource string, fallback to L"Settings"
		//auto rl = Microsoft::Windows::ApplicationModel::Resources::ResourceLoader();
		//auto name = rl.GetString(L"settings");
		//if (name.empty()) name = L"Settings";
		//folder.Name(name);
		folder.Name(L"Settings");

		items.Append(folder);
		MainSettingsPageBar().ItemsSource(items);

		// Hook up ItemClicked to handle breadcrumb trimming if needed
		MainSettingsPageBar().ItemClicked({ this, &MainSettingsPage::SettingsBar_ItemClicked });
	}

	MainSettingsPage* MainSettingsPage::Current()
	{
		return s_current;
	}

	void MainSettingsPage::UpdateSettingsBarItems(winrt::Windows::Foundation::Collections::IObservableVector<winrt::OpenNet::Pages::SettingsPages::Folder> const& items)
	{
		MainSettingsPageBar().ItemsSource(items);
	}

	void MainSettingsPage::SettingsBar_ItemClicked(BreadcrumbBar const& /*sender*/, BreadcrumbBarItemClickedEventArgs const& args)
	{
		// Trim items after clicked index
		auto itemsObj = MainSettingsPageBar().ItemsSource();
		auto vec = itemsObj.try_as<IVector<IInspectable>>();
		if (!vec) return;
		int32_t count = static_cast<int32_t>(vec.Size());
		for (int32_t i = count - 1; i >= args.Index() + 1; --i)
		{
			vec.RemoveAtEnd();
		}

		// Navigate back to SettingsPage when breadcrumb trimmed
		SettingsFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::SettingsPages::SettingsPage>());
		if (auto page = SettingsPage::Current())
		{
			page->AboutFrame().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
		}
	}

}
