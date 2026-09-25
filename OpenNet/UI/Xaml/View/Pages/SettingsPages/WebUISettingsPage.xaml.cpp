#include "XamlWorkaround.h"
#include "WebUISettingsPage.xaml.h"
#include "SettingsPageTagRegister.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/WebUISettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/WebUISettingsPage.g.cpp"
#endif

#include "Core/WebUI/WebUIControl.h"

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Utils.Message;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.WinUI.LiquidGlass;
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
	static SettingsPageTagRegister<WebUISettingsPage> s_tags{
		L"webui", L"SettingsWebUISearchTags" };
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

		std::optional<bool> ToggleIsOn(IInspectable const& sender)
		{
			if (!sender) return std::nullopt;
			if (auto standard = sender.try_as<ToggleSwitch>()) return standard.IsOn();
			if (auto glass = sender.try_as<WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) return glass.IsOn();
			return std::nullopt;
		}

		void SetToggleIsOn(IInspectable const& target, bool value)
		{
			if (!target) return;
			if (auto standard = target.try_as<ToggleSwitch>()) standard.IsOn(value);
			if (auto glass = target.try_as<WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) glass.IsOn(value);
		}
	}

	WebUISettingsPage::WebUISettingsPage()
	{
		InitializeComponent();
		Loaded([weak = get_weak()](auto const&, auto const&)
		{
			if (auto self = weak.get())
			{
				self->LoadSettings();
			}
		});
		Unloaded([weak = get_weak()](auto const&, auto const&)
		{
			if (auto self = weak.get(); self && self->m_applyTimer && self->m_applyTimer.IsRunning())
			{
				self->m_applyTimer.Stop();
				self->ApplySettings();
			}
		});
		m_applyTimer = DispatcherQueue().CreateTimer();
		m_applyTimer.Interval(std::chrono::milliseconds(700));
		m_applyTimer.IsRepeating(false);
		m_applyTimer.Tick([weak = get_weak()](auto const&, auto const&)
		{
			if (auto self = weak.get()) self->ApplySettings();
		});
		auto changed = [weak = get_weak()](auto const&, auto const&)
		{
			if (auto self = weak.get()) self->ScheduleApply();
		};
		FrontendComboBox().SelectionChanged(changed);
		AddressTextBox().TextChanged(changed);
		PortNumberBox().ValueChanged(changed);
		LocaleTextBox().TextChanged(changed);
		UsernameTextBox().TextChanged(changed);
		PasswordInput().PasswordChanged(changed);
		ViewModel().PropertyChanged([weak = get_weak()](auto const&, auto const& args)
		{
			if (auto self = weak.get())
			{
				auto const propertyName = args.PropertyName();
				if (propertyName == L"ApiKey" || propertyName == L"Enabled")
					self->ScheduleApply();
			}
		});
		SessionTimeoutNumberBox().ValueChanged(changed);
		SessionCountLimitNumberBox().ValueChanged(changed);
		MaxAuthFailuresNumberBox().ValueChanged(changed);
		BanDurationNumberBox().ValueChanged(changed);
		DomainListTextBox().TextChanged(changed);
	}

	void WebUISettingsPage::ScheduleApply()
	{
		if (m_initializing || !m_applyTimer) return;
		m_applyTimer.Stop();
		m_applyTimer.Start();
	}

	void WebUISettingsPage::LoadSettings()
	{
		m_initializing = true;
		if (m_applyTimer) m_applyTimer.Stop();
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		ViewModel().Load();
		FrontendComboBox().SelectedIndex(
			database.GetString(Category.data(), "frontend")
			.value_or("qbittorrent") == "vuetorrent" ? 1 : 0);
		AddressTextBox().Text(to_hstring(
			database.GetString(Category.data(), "address")
			.value_or("127.0.0.1")));
		PortNumberBox().Value(static_cast<double>(
			database.GetInt(Category.data(), "port").value_or(8080)));
		auto const storedLocale = database.GetString(Category.data(), "locale");
		m_followApplicationLanguage = database.GetBool(
			Category.data(), "follow_application_language")
			.value_or(!storedLocale.has_value());
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
			m_followApplicationLanguage
			? applicationLocale
			: storedLocale.value_or("en")));
		LocaleTextBox().IsEnabled(!m_followApplicationLanguage);
		UsernameTextBox().Text(to_hstring(database.GetString(Category.data(), "username").value_or("admin")));
		PasswordInput().Password(to_hstring(database.GetString(Category.data(), "password").value_or("adminadmin")));
		m_bypassLocalAuth = database.GetBool(Category.data(), "bypass_authentication_for_localhost").value_or(false);
		SessionTimeoutNumberBox().Value(static_cast<double>(database.GetInt(Category.data(), "session_timeout_seconds").value_or(3600)));
		SessionCountLimitNumberBox().Value(static_cast<double>(database.GetInt(Category.data(), "session_count_limit").value_or(10)));
		MaxAuthFailuresNumberBox().Value(static_cast<double>(database.GetInt(Category.data(), "maximum_authentication_failures").value_or(5)));
		BanDurationNumberBox().Value(static_cast<double>(database.GetInt(Category.data(), "ban_duration_seconds").value_or(3600)));
		m_csrfProtection = database.GetBool(Category.data(), "csrf_protection").value_or(true);
		m_hostValidation = database.GetBool(Category.data(), "host_header_validation").value_or(true);
		m_secureCookie = database.GetBool(Category.data(), "secure_cookie").value_or(false);
		SyncMaterialToggles();
		DomainListTextBox().Text(to_hstring(database.GetString(Category.data(), "domain_list").value_or("*")));
		UpdateStatus();
		m_initializing = false;
	}

	void WebUISettingsPage::SyncMaterialToggles()
	{
		SetToggleIsOn(FollowApplicationLanguageToggle(), m_followApplicationLanguage);
		SetToggleIsOn(FollowApplicationLanguageGlassToggle(), m_followApplicationLanguage);
		SetToggleIsOn(BypassLocalAuthToggle(), m_bypassLocalAuth);
		SetToggleIsOn(BypassLocalAuthGlassToggle(), m_bypassLocalAuth);
		SetToggleIsOn(CsrfProtectionToggle(), m_csrfProtection);
		SetToggleIsOn(CsrfProtectionGlassToggle(), m_csrfProtection);
		SetToggleIsOn(HostValidationToggle(), m_hostValidation);
		SetToggleIsOn(HostValidationGlassToggle(), m_hostValidation);
		SetToggleIsOn(SecureCookieToggle(), m_secureCookie);
		SetToggleIsOn(SecureCookieGlassToggle(), m_secureCookie);
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
			? ResourceGetString(L"WebUISettingsRunningAt") + WebUIUrl()
			: ResourceGetString(L"WebUISettingsChangesApplyAutomatically"));
	}

	void WebUISettingsPage::ApplySettings()
	{
		const auto address = to_string(AddressTextBox().Text());
		const std::string frontend = FrontendComboBox().SelectedIndex() == 1
			? "vuetorrent" : "qbittorrent";
		const auto locale = to_string(LocaleTextBox().Text());
		const bool followApplicationLanguage = m_followApplicationLanguage;
		const auto username = to_string(UsernameTextBox().Text());
		const auto password = to_string(PasswordInput().Password());
		const auto apiKey = to_string(ViewModel().ApiKey());
		const auto portValue = PortNumberBox().Value();
		const bool enabled = ViewModel().Enabled();
		const bool bypassLocalAuth = m_bypassLocalAuth;
		const bool csrfProtection = m_csrfProtection;
		const bool hostValidation = m_hostValidation;
		const bool secureCookie = m_secureCookie;
		const auto domainList = to_string(DomainListTextBox().Text());
		const auto sessionTimeout = SessionTimeoutNumberBox().Value();
		const auto sessionLimit = SessionCountLimitNumberBox().Value();
		const auto maximumFailures = MaxAuthFailuresNumberBox().Value();
		const auto banDuration = BanDurationNumberBox().Value();

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
			const auto validInteger = [](double value, double minimum, double maximum)
			{
				return std::isfinite(value) && value >= minimum && value <= maximum && std::floor(value) == value;
			};
			if (!validInteger(sessionTimeout, 0, 86400) || !validInteger(sessionLimit, 0, 1000) || !validInteger(maximumFailures, 0, 100) || !validInteger(banDuration, 1, 86400))
				throw std::invalid_argument("Web UI session or authentication limits are invalid.");
			if (hostValidation && domainList.empty()) throw std::invalid_argument("At least one allowed server domain is required when Host validation is enabled.");

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
			const auto oldEnabled = database.GetBool(Category.data(), "enabled");
			const auto oldBypassLocalAuth = database.GetBool(Category.data(), "bypass_authentication_for_localhost");
			const auto oldCsrfProtection = database.GetBool(Category.data(), "csrf_protection");
			const auto oldHostValidation = database.GetBool(Category.data(), "host_header_validation");
			const auto oldSecureCookie = database.GetBool(Category.data(), "secure_cookie");
			const auto oldDomainList = database.GetString(Category.data(), "domain_list");
			const auto oldSessionTimeout = database.GetInt(Category.data(), "session_timeout_seconds");
			const auto oldSessionLimit = database.GetInt(Category.data(), "session_count_limit");
			const auto oldMaximumFailures = database.GetInt(Category.data(), "maximum_authentication_failures");
			const auto oldBanDuration = database.GetInt(Category.data(), "ban_duration_seconds");

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
			database.SetBool(Category.data(), "enabled", enabled);
			database.SetBool(Category.data(), "bypass_authentication_for_localhost", bypassLocalAuth);
			database.SetBool(Category.data(), "csrf_protection", csrfProtection);
			database.SetBool(Category.data(), "host_header_validation", hostValidation);
			database.SetBool(Category.data(), "secure_cookie", secureCookie);
			database.SetString(Category.data(), "domain_list", domainList);
			database.SetInt(Category.data(), "session_timeout_seconds", static_cast<std::int64_t>(sessionTimeout));
			database.SetInt(Category.data(), "session_count_limit", static_cast<std::int64_t>(sessionLimit));
			database.SetInt(Category.data(), "maximum_authentication_failures", static_cast<std::int64_t>(maximumFailures));
			database.SetInt(Category.data(), "ban_duration_seconds", static_cast<std::int64_t>(banDuration));
			if (apiKey.empty())
				database.Delete("webui_http", "api_key");
			else
				database.SetString("webui_http", "api_key", apiKey);

			bool applied = true;
			if (!enabled)
				::OpenNet::Core::WebUI::StopWebUI();
			else
				applied = ::OpenNet::Core::WebUI::IsWebUIRunning() ? ::OpenNet::Core::WebUI::RestartWebUI() : ::OpenNet::Core::WebUI::StartWebUI();
			ViewModel().RefreshRuntimeState();
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
				const auto restoreBool = [&database](const char* key, std::optional<bool> const& value)
				{
					if (value) database.SetBool(Category.data(), key, *value); else database.Delete(Category.data(), key);
				};
				const auto restoreInt = [&database](const char* key, std::optional<std::int64_t> const& value)
				{
					if (value) database.SetInt(Category.data(), key, *value); else database.Delete(Category.data(), key);
				};
				restoreBool("enabled", oldEnabled);
				restoreBool("bypass_authentication_for_localhost", oldBypassLocalAuth);
				restoreBool("csrf_protection", oldCsrfProtection);
				restoreBool("host_header_validation", oldHostValidation);
				restoreBool("secure_cookie", oldSecureCookie);
				restoreString(Category.data(), "domain_list", oldDomainList);
				restoreInt("session_timeout_seconds", oldSessionTimeout);
				restoreInt("session_count_limit", oldSessionLimit);
				restoreInt("maximum_authentication_failures", oldMaximumFailures);
				restoreInt("ban_duration_seconds", oldBanDuration);
				bool rollbackApplied = true;
				if (oldEnabled.value_or(true))
				{
					if (!::OpenNet::Core::WebUI::IsWebUIRunning())
						rollbackApplied = ::OpenNet::Core::WebUI::StartWebUI();
				}
				else
				{
					::OpenNet::Core::WebUI::StopWebUI();
				}
				const bool wasInitializing = m_initializing;
				m_initializing = true;
				ViewModel().Enabled(oldEnabled.value_or(true));
				ViewModel().ApiKey(to_hstring(oldApiKey.value_or("")));
				ViewModel().RefreshRuntimeState();
				m_initializing = wasInitializing;
				if (!rollbackApplied)
					OutputDebugStringA("WebUISettingsPage: failed to restart the restored Web UI configuration.\n");
				throw std::runtime_error(winrt::to_string(
					ResourceGetString(L"WebUISettingsAddressPortFailedRestored")));
			}

			UpdateStatus(enabled
				? ResourceGetString(L"WebUISettingsChangesAppliedRunning") + WebUIUrl()
				: ResourceGetString(L"WebUISettingsChangesAppliedDisabled"),
				InfoBarSeverity::Success);
		}
		catch (std::exception const& exception)
		{
			UpdateStatus(
				to_hstring(exception.what()), InfoBarSeverity::Error);
		}
	}

	void WebUISettingsPage::OnMaterialToggleLoaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		auto const name = sender.as<FrameworkElement>().Name();
		const bool wasInitializing = m_initializing;
		m_initializing = true;
		if (name == L"EnableWebUIToggle" || name == L"EnableWebUIGlassToggle") SetToggleIsOn(sender, ViewModel().Enabled());
		else if (name == L"FollowApplicationLanguageToggle" || name == L"FollowApplicationLanguageGlassToggle") SetToggleIsOn(sender, m_followApplicationLanguage);
		else if (name == L"BypassLocalAuthToggle" || name == L"BypassLocalAuthGlassToggle") SetToggleIsOn(sender, m_bypassLocalAuth);
		else if (name == L"CsrfProtectionToggle" || name == L"CsrfProtectionGlassToggle") SetToggleIsOn(sender, m_csrfProtection);
		else if (name == L"HostValidationToggle" || name == L"HostValidationGlassToggle") SetToggleIsOn(sender, m_hostValidation);
		else if (name == L"SecureCookieToggle" || name == L"SecureCookieGlassToggle") SetToggleIsOn(sender, m_secureCookie);
		m_initializing = wasInitializing;
	}

	void WebUISettingsPage::OnMaterialToggleChanged(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_initializing) return;
		auto const value = ToggleIsOn(sender);
		if (!value) return;
		auto const name = sender.as<FrameworkElement>().Name();
		bool followChanged = false;
		if (name == L"EnableWebUIToggle" || name == L"EnableWebUIGlassToggle") ViewModel().Enabled(*value);
		else if (name == L"FollowApplicationLanguageToggle" || name == L"FollowApplicationLanguageGlassToggle")
		{
			m_followApplicationLanguage = *value;
			followChanged = true;
		}
		else if (name == L"BypassLocalAuthToggle" || name == L"BypassLocalAuthGlassToggle") m_bypassLocalAuth = *value;
		else if (name == L"CsrfProtectionToggle" || name == L"CsrfProtectionGlassToggle") m_csrfProtection = *value;
		else if (name == L"HostValidationToggle" || name == L"HostValidationGlassToggle") m_hostValidation = *value;
		else if (name == L"SecureCookieToggle" || name == L"SecureCookieGlassToggle") m_secureCookie = *value;
		else return;
		m_initializing = true;
		SyncMaterialToggles();
		m_initializing = false;
		if (followChanged) UpdateFollowLanguageUi();
		ScheduleApply();
	}


	void WebUISettingsPage::UpdateFollowLanguageUi()
	{
		LocaleTextBox().IsEnabled(!m_followApplicationLanguage);
		if (m_followApplicationLanguage)
		{
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
	}

	void WebUISettingsPage::OnGenerateApiKeyClick(IInspectable const&, RoutedEventArgs const&)
	{
		ViewModel().ApiKey(to_hstring(RandomApiKey()));
		UpdateStatus(ResourceGetString(L"WebUISettingsApiKeyApplying"), InfoBarSeverity::Informational);
	}

	void WebUISettingsPage::OnCopyApiKeyClick(IInspectable const&, RoutedEventArgs const&)
	{
		if (!ViewModel().HasApiKey())
		{
			UpdateStatus(ResourceGetString(L"WebUISettingsNoApiKeyToCopy"), InfoBarSeverity::Warning);
			return;
		}
		DataPackage package;
		package.SetText(ViewModel().ApiKey());
		Clipboard::SetContent(package);
		UpdateStatus(ResourceGetString(L"WebUISettingsApiKeyCopied"), InfoBarSeverity::Success);
	}

	void WebUISettingsPage::OnClearApiKeyClick(IInspectable const&, RoutedEventArgs const&)
	{
		ViewModel().ApiKey(L"");
		UpdateStatus(ResourceGetString(L"WebUISettingsApiKeyRevocationApplying"), InfoBarSeverity::Informational);
	}

	fire_and_forget WebUISettingsPage::OnOpenWebUIClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		if (m_applyTimer && m_applyTimer.IsRunning())
		{
			m_applyTimer.Stop();
			ApplySettings();
		}
		ViewModel().RefreshRuntimeState();
		if (!ViewModel().IsRunning())
		{
			UpdateStatus(ResourceGetString(L"WebUISettingsNotRunningCheckSettings"), InfoBarSeverity::Warning);
			co_return;
		}
		co_await Windows::System::Launcher::LaunchUriAsync(
			Windows::Foundation::Uri{ WebUIUrl() });
	}
}
