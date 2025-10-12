#include "pch.h"
#include "SettingsPage.xaml.h"
#if __has_include("Pages/SettingsPages/SettingsPage.g.cpp")
#include "Pages/SettingsPages/SettingsPage.g.cpp"
#endif
#include "MainSettingsPage.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Imaging;
using namespace Microsoft::UI::Xaml::Media::Animation;
using namespace Windows::Globalization;
using namespace Microsoft::Windows::ApplicationModel::Resources;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::OpenNet::Pages::SettingsPages::implementation
{
	SettingsPage::SettingsPage()
	{
		InitializeComponent();
	}

	void SettingsPage::Aboutp_Click(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		// Navigate AboutFrame in this page
		AboutFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::SettingsPages::AboutPage>());
		AboutFrame().Visibility(Visibility::Visible);

		// Build items as observable vector of Folder runtimeclass (defined in IDL)
		auto items = single_threaded_observable_vector<winrt::OpenNet::Pages::SettingsPages::Folder>();
		auto folder1 = winrt::OpenNet::Pages::SettingsPages::Folder();
		auto folder2 = winrt::OpenNet::Pages::SettingsPages::Folder();

		// Try to get localized strings, fallback to literals
		try
		{
			ResourceLoader rl;
			auto nameSettings = rl.GetString(L"settings");
			auto nameAbout = rl.GetString(L"about");
			if (nameSettings.empty()) nameSettings = L"Settings";
			if (nameAbout.empty()) nameAbout = L"About";
			folder1.Name(nameSettings);
			folder2.Name(nameAbout);
		}
		catch (...) {
			folder1.Name(L"Settings");
			folder2.Name(L"About");
		}

		items.Append(folder1);
		items.Append(folder2);

		// Update SettingsBar through the public helper on MainSettingsPage (runtime projection)
		if (auto current = MainSettingsPage::Current)
		{
			current.UpdateSettingsBarItems(items);

			// Navigate the SettingsFrame on the main settings page to AboutPage with slide-from-right transition
			auto transition = SlideNavigationTransitionInfo();
			transition.Effect(SlideNavigationTransitionEffect::FromRight);
			// Navigate: parameter = empty IInspectable
			current.SettingsFrame().Navigate(xaml_typename<winrt::OpenNet::Pages::SettingsPages::AboutPage>(), winrt::Windows::Foundation::IInspectable{}, transition);
		}
	}

	void SettingsPage::SPSettings_Click(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		// Placeholder for serial port settings click
	}

	void SettingsPage::OpenToml_click(winrt::Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
	{
		// Placeholder: opening TOML file handled by platform-specific code in original project.
	}

	void SettingsPage::SoftLanguageCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args)
	{
		try
		{
			auto combo = sender.try_as<ComboBox>();
			if (!combo) return;

			auto idx = combo.SelectedIndex();
			if (idx < 0) return;

			// Map to language tag: index 0 = Auto, 1 = zh-CN (Chinese), 2 = en-US (English)
			if (idx == 0)
			{
				ApplicationLanguages::PrimaryLanguageOverride(L""); // follow system
			}
			else if (idx == 1)
			{
				ApplicationLanguages::PrimaryLanguageOverride(L"zh-CN");
			}
			else if (idx == 2)
			{
				ApplicationLanguages::PrimaryLanguageOverride(L"en-US");
			}

			// Force reload of resources: create ResourceLoader instance which will use PrimaryLanguageOverride
			try
			{
				ResourceLoader rl;
				(void)rl.GetString(L"Lang_Auto");
			}
			catch (...) {
			}

			// Update DataContext to trigger bindings refresh
			try
			{
				auto dc = this->DataContext();
				this->DataContext(nullptr);
				this->DataContext(dc);
			}
			catch (...) {
			}
		}
		catch (...) {
		}
	}

	void SettingsPage::SoftBackgroundCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
	{
		// No-op placeholder
	}

	void SettingsPage::StartPageCombobox_SelectionChanged(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&)
	{
		// Start page selection handling (not implemented)
	}

	void SettingsPage::SetDesktopBackground()
	{
		// Safe no-op implementation: if DesktopBackgroundImage exists and has valid Source, leave it.
		try
		{
			auto img = DesktopBackgroundImage();
			if (img && !img.Source())
			{
				// leave empty; real implementation can set a BitmapImage here
			}
		}
		catch (...) {
		}
	}
}
