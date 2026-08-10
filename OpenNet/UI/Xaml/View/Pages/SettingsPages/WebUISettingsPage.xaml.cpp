#include "XamlWorkaround.h"
#include "WebUISettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/WebUISettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/WebUISettingsPage.g.cpp"
#endif

#include "Core/WebUI/WebUIControl.h"

import OpenNet.Core.AppSettingsDatabase;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.Windows.Globalization;
import winrt.Windows.ApplicationModel.DataTransfer;
import winrt.Windows.Foundation;
import winrt.Windows.System;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	namespace
	{
		constexpr std::string_view Category = "webui_host";

		std::string RandomApiKey()
		{
			static constexpr char Hex[] = "0123456789abcdef";
			std::random_device random;
			std::string key{ "qbt_" };
			key.reserve(32);
			for (int index = 0; index < 14; ++index)
			{
				const auto value = static_cast<unsigned char>(random());
				key.push_back(Hex[value >> 4]);
				key.push_back(Hex[value & 0x0f]);
			}
			return key;
		}

		std::string NormalizeAddressForUrl(std::string address)
		{
			if (address.find(':') != std::string::npos
				&& !(address.starts_with('[') && address.ends_with(']')))
			{
				return "[" + address + "]";
			}
			if (address == "0.0.0.0" || address == "::")
				return "127.0.0.1";
			return address;
		}
	}

	WebUISettingsPage::WebUISettingsPage()
	{
		InitializeComponent();
		Loaded([this](auto const&, auto const&)
		{
			LoadSettings();
		});
	}

	void WebUISettingsPage::LoadSettings()
	{
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		FrontendComboBox().SelectedIndex(
			database.GetString(Category.data(), "frontend")
			.value_or("qbittorrent") == "vuetorrent" ? 1 : 0);
		AddressTextBox().Text(to_hstring(
			database.GetString(Category.data(), "address")
			.value_or("127.0.0.1")));
		PortNumberBox().Value(static_cast<double>(
			database.GetInt(Category.data(), "port").value_or(8080)));
		auto const storedLocale = database.GetString(Category.data(), "locale");
		const bool followApplicationLanguage = database.GetBool(
			Category.data(), "follow_application_language")
			.value_or(!storedLocale.has_value());
		FollowApplicationLanguageToggle().IsOn(followApplicationLanguage);
		std::string applicationLocale{ "en" };
		try
		{
			auto languages = winrt::Microsoft::Windows::Globalization::
				ApplicationLanguages::Languages();
			if (languages.Size() > 0)
				applicationLocale = to_string(languages.GetAt(0));
		}
		catch (...)
		{
		}
		LocaleTextBox().Text(to_hstring(
			followApplicationLanguage
			? applicationLocale
			: storedLocale.value_or("en")));
		LocaleTextBox().IsEnabled(!followApplicationLanguage);
		UsernameTextBox().Text(to_hstring(
			database.GetString(Category.data(), "username")
			.value_or("admin")));
		PasswordInput().Password(to_hstring(
			database.GetString(Category.data(), "password")
			.value_or("adminadmin")));
		ApiKeyInput().Password(to_hstring(
			database.GetString("webui_http", "api_key").value_or("")));
		UpdateStatus();
	}

	hstring WebUISettingsPage::WebUIUrl()
	{
		auto address = NormalizeAddressForUrl(
			to_string(AddressTextBox().Text()));
		const auto port = static_cast<unsigned int>(PortNumberBox().Value());
		return to_hstring(
			"http://" + address + ":" + std::to_string(port) + "/");
	}

	void WebUISettingsPage::UpdateStatus(hstring const& message, InfoBarSeverity severity)
	{
		StatusInfoBar().Severity(severity);
		if (!message.empty())
		{
			StatusInfoBar().Message(message);
			return;
		}
		StatusInfoBar().Message(
			::OpenNet::Core::WebUI::IsWebUIRunning()
			? L"Web UI is running at " + WebUIUrl()
			: L"Web UI is not running. Save valid settings to start it.");
	}

	void WebUISettingsPage::OnSaveClick(IInspectable const&, RoutedEventArgs const&)
	{
		const auto address = to_string(AddressTextBox().Text());
		const std::string frontend = FrontendComboBox().SelectedIndex() == 1
			? "vuetorrent" : "qbittorrent";
		const auto locale = to_string(LocaleTextBox().Text());
		const bool followApplicationLanguage =
			FollowApplicationLanguageToggle().IsOn();
		const auto username = to_string(UsernameTextBox().Text());
		const auto password = to_string(PasswordInput().Password());
		const auto apiKey = to_string(ApiKeyInput().Password());
		const auto portValue = PortNumberBox().Value();

		try
		{
			if (address.empty())
				throw std::invalid_argument("Listen address cannot be empty.");
			if (!std::isfinite(portValue)
				|| portValue < 1 || portValue > 65535)
			{
				throw std::invalid_argument("Port must be between 1 and 65535.");
			}
			if (username.size() < 3
				|| username.find(':') != std::string::npos)
			{
				throw std::invalid_argument(
					"Username must contain at least 3 characters and no colon.");
			}
			if (password.size() < 6)
				throw std::invalid_argument(
					"Password must contain at least 6 characters.");
			if (!followApplicationLanguage && locale.empty())
				throw std::invalid_argument("Locale cannot be empty.");

			auto& database =
				::OpenNet::Core::AppSettingsDatabase::Instance();
			database.Initialize();
			const auto oldAddress =
				database.GetString(Category.data(), "address");
			const auto oldFrontend =
				database.GetString(Category.data(), "frontend");
			const auto oldPort =
				database.GetInt(Category.data(), "port");
			const auto oldLocale =
				database.GetString(Category.data(), "locale");
			const auto oldFollowApplicationLanguage =
				database.GetBool(Category.data(), "follow_application_language");
			const auto oldUsername =
				database.GetString(Category.data(), "username");
			const auto oldPassword =
				database.GetString(Category.data(), "password");
			const auto oldApiKey =
				database.GetString("webui_http", "api_key");

			database.SetString(Category.data(), "address", address);
			database.SetString(Category.data(), "frontend", frontend);
			database.SetInt(
				Category.data(), "port",
				static_cast<std::int64_t>(portValue));
			database.SetBool(Category.data(), "follow_application_language",
							 followApplicationLanguage);
			if (!followApplicationLanguage)
				database.SetString(Category.data(), "locale", locale);
			database.SetString(Category.data(), "username", username);
			database.SetString(Category.data(), "password", password);
			database.SetBool(Category.data(), "initialized", true);
			if (apiKey.empty())
				database.Delete("webui_http", "api_key");
			else
				database.SetString("webui_http", "api_key", apiKey);

			const bool applied =
				::OpenNet::Core::WebUI::IsWebUIRunning()
				? ::OpenNet::Core::WebUI::RestartWebUI()
				: ::OpenNet::Core::WebUI::StartWebUI();
			if (!applied)
			{
				const auto restoreString = [&database](
					const char* category, const char* key,
					std::optional<std::string> const& value)
				{
					if (value)
						database.SetString(category, key, *value);
					else
						database.Delete(category, key);
				};
				restoreString(Category.data(), "address", oldAddress);
				restoreString(Category.data(), "frontend", oldFrontend);
				if (oldPort)
					database.SetInt(Category.data(), "port", *oldPort);
				else
					database.Delete(Category.data(), "port");
				restoreString(Category.data(), "locale", oldLocale);
				if (oldFollowApplicationLanguage)
					database.SetBool(Category.data(), "follow_application_language",
									 *oldFollowApplicationLanguage);
				else
					database.Delete(Category.data(), "follow_application_language");
				restoreString(Category.data(), "username", oldUsername);
				restoreString(Category.data(), "password", oldPassword);
				restoreString("webui_http", "api_key", oldApiKey);
				::OpenNet::Core::WebUI::StartWebUI();
				throw std::runtime_error(
					"The address or port could not be opened. Previous settings were restored.");
			}

			UpdateStatus(
				L"Settings saved. Web UI restarted at " + WebUIUrl(),
				InfoBarSeverity::Success);
		}
		catch (std::exception const& exception)
		{
			UpdateStatus(
				to_hstring(exception.what()), InfoBarSeverity::Error);
		}
	}

	void WebUISettingsPage::OnFollowApplicationLanguageToggled(
		IInspectable const&, RoutedEventArgs const&)
	{
		const bool follow = FollowApplicationLanguageToggle().IsOn();
		LocaleTextBox().IsEnabled(!follow);
		if (!follow) return;
		try
		{
			auto languages = winrt::Microsoft::Windows::Globalization::
				ApplicationLanguages::Languages();
			if (languages.Size() > 0)
				LocaleTextBox().Text(languages.GetAt(0));
		}
		catch (...)
		{
		}
	}

	void WebUISettingsPage::OnGenerateApiKeyClick(IInspectable const&, RoutedEventArgs const&)
	{
		ApiKeyInput().Password(to_hstring(RandomApiKey()));
		UpdateStatus(
			L"A new API key was generated. Select “Save and restart Web UI” to apply it.",
			InfoBarSeverity::Informational);
	}

	void WebUISettingsPage::OnCopyApiKeyClick(IInspectable const&, RoutedEventArgs const&)
	{
		if (ApiKeyInput().Password().empty())
		{
			UpdateStatus(L"There is no API key to copy.", InfoBarSeverity::Warning);
			return;
		}
		DataPackage package;
		package.SetText(ApiKeyInput().Password());
		Clipboard::SetContent(package);
		UpdateStatus(L"API key copied to the clipboard.", InfoBarSeverity::Success);
	}

	void WebUISettingsPage::OnClearApiKeyClick(IInspectable const&, RoutedEventArgs const&)
	{
		ApiKeyInput().Password(L"");
		UpdateStatus(
			L"API key cleared locally. Save the settings to revoke it.",
			InfoBarSeverity::Informational);
	}

	fire_and_forget WebUISettingsPage::OnOpenWebUIClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		co_await Windows::System::Launcher::LaunchUriAsync(
			Windows::Foundation::Uri{ WebUIUrl() });
	}
}
