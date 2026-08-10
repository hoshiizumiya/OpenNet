#include <time.h>

#include "XamlWorkaround.h"
#include "IPFilterSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.g.cpp"
#endif

#include "Core/IPFilter/IPFilterManager.h"
#include "UI/Xaml/View/InfoBarView.xaml.h"

import OpenNet.Core.IO.FileSystem;
import OpenNet.Core.Utils.Message;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.Windows.ApplicationModel.Resources;
import winrt.Microsoft.Windows.Storage.Pickers;
import winrt.Windows.Storage;
import winrt.Windows.System;
import winrt.Windows.Web.Http;
import winrtplus_coroutine;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Windows::Storage::Pickers;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	namespace
	{
		constexpr std::size_t MaxVisibleRules = 1000;
		constexpr std::size_t MaxSubscriptionBytes = 64 * 1024 * 1024;
		constexpr std::size_t MaxCombinedSubscriptionBytes = 128 * 1024 * 1024;

		winrt::hstring ResourceText(
			wchar_t const* key, wchar_t const* fallback)
		{
			try
			{
				auto value = winrt::Microsoft::Windows::ApplicationModel::
					Resources::ResourceLoader{}.GetString(key);
				if (!value.empty())
					return value;
			}
			catch (...)
			{
			}
			return fallback;
		}

		winrt::hstring FormatTimestamp(std::int64_t timestamp)
		{
			if (timestamp <= 0)
				return ResourceText(L"IPF_Never", L"Never");
			auto const value = static_cast<std::time_t>(timestamp);
			std::tm local{};
			localtime_s(&local, &value);
			wchar_t buffer[64]{};
			wcsftime(buffer, std::size(buffer), L"%Y-%m-%d %H:%M:%S", &local);
			return buffer;
		}

		bool IsHttpSubscriptionUrl(winrt::hstring const& value)
		{
			try
			{
				winrt::Windows::Foundation::Uri uri{ value };
				auto const scheme = uri.SchemeName();
				return (scheme == L"http" || scheme == L"https")
					&& !uri.Host().empty();
			}
			catch (...)
			{
				return false;
			}
		}

		std::string MigrateLegacySubscriptionUrl(std::string url)
		{
			// PeerBanHelper retired the temporary ghostchu-services.top BTN rule
			// host in favor of the stable pbh-btn.com domain. Preserve the path so
			// existing user subscriptions continue to update transparently.
			constexpr std::string_view legacyHost =
				"bcr.pbh-btn.ghorg.ghostchu-services.top";
			constexpr std::string_view currentHost = "bcr.pbh-btn.com";
			auto lower = url;
			std::transform(lower.begin(), lower.end(), lower.begin(),
				[](unsigned char ch)
				{
					return static_cast<char>(std::tolower(ch));
				});
			auto const position = lower.find(legacyHost);
			if (position != std::string::npos)
				url.replace(position, legacyHost.size(), currentHost);
			return url;
		}

		struct SubscriptionUpdateGuard
		{
			std::atomic_bool& value;
			~SubscriptionUpdateGuard()
			{
				value.store(false);
			}
		};

		std::filesystem::path RulesFolderPath()
		{
			return std::filesystem::path(
				winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW()) / L"Rules";
		}

		std::string TrimCopy(std::string value)
		{
			auto const first = value.find_first_not_of(" \t\r\n");
			if (first == std::string::npos)
				return {};
			auto const last = value.find_last_not_of(" \t\r\n");
			return value.substr(first, last - first + 1);
		}

		std::string LowerAscii(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(),
						   [](unsigned char ch)
			{
				return static_cast<char>(std::tolower(ch));
			});
			return value;
		}

		std::string RuleEntry(::OpenNet::Core::IPRule const& rule)
		{
			std::string first;
			std::string last;
			if (!rule.description.empty() &&
				::OpenNet::Core::IPFilterManager::ParseIPOrCIDR(
					rule.description, first, last) &&
				first == rule.firstIp && last == rule.lastIp)
			{
				return rule.description;
			}
			return rule.firstIp == rule.lastIp
				? rule.firstIp
				: rule.firstIp + "-" + rule.lastIp;
		}

		winrt::hstring RuleDisplayText(::OpenNet::Core::IPRule const& rule)
		{
			auto text = "#" + std::to_string(rule.id) + "  " + RuleEntry(rule);
			if (!rule.description.empty() && rule.description != RuleEntry(rule))
				text += "  —  " + rule.description;
			return winrt::to_hstring(text);
		}
	}

	winrt::Windows::Foundation::IAsyncAction
		IPFilterSettingsPage::RunSubscriptionUpdateAsync(
			bool force, bool notify)
	{
		if (s_subscriptionUpdateRunning.exchange(true))
			co_return;
		SubscriptionUpdateGuard guard{ s_subscriptionUpdateRunning };

		auto& manager = ::OpenNet::Core::IPFilterManager::Instance();
		manager.Initialize();
		auto const now = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		if (!force && !manager.IsSubscriptionUpdateDue(now))
			co_return;

		auto subscriptions = manager.GetSubscriptions();
		std::erase_if(subscriptions, [](auto const& item)
		{
			return !item.enabled;
		});
		if (subscriptions.empty())
		{
			if (force && notify)
			{
				winrt::OpenNet::UI::Xaml::View::implementation::InfoBarView::Show(
					ResourceText(L"IPF_NotifyTitle", L"IP filter subscriptions"),
					ResourceText(L"IPF_NoEnabledSubscriptions", L"No enabled subscription sources."),
					InfoBarSeverity::Warning,
					6000);
			}
			co_return;
		}

		std::string combinedContent;
		std::int32_t succeeded = 0;
		std::int32_t failed = 0;
		std::int32_t downloadedRules = 0;
		winrt::Windows::Web::Http::HttpClient client;
		client.DefaultRequestHeaders().UserAgent().ParseAdd(
			L"OpenNet/1.0 IPFilterSubscription");
		client.DefaultRequestHeaders().Accept().ParseAdd(
			L"text/plain, */*;q=0.8");

		for (auto subscription : subscriptions)
		{
			try
			{
				auto const migratedUrl = MigrateLegacySubscriptionUrl(subscription.url);
				if (migratedUrl != subscription.url)
				{
					manager.UpdateSubscription(
						subscription.id, migratedUrl, subscription.enabled);
					subscription.url = migratedUrl;
				}
				auto const url = winrt::to_hstring(subscription.url);
				if (!IsHttpSubscriptionUrl(url))
					throw std::invalid_argument("Only HTTP and HTTPS URLs are supported");

				auto const response = co_await client.GetAsync(
					winrt::Windows::Foundation::Uri{ url });
				if (!response.IsSuccessStatusCode())
				{
					throw std::runtime_error(
						"HTTP " + std::to_string(
							static_cast<int>(response.StatusCode())));
				}
				auto const body = co_await response.Content().ReadAsStringAsync();
				if (body.size() > MaxSubscriptionBytes)
					throw std::length_error("The subscription response exceeds 64 MiB");
				co_await winrt::resume_background();
				auto content = winrt::to_string(body);
				auto const count =
					::OpenNet::Core::IPFilterManager::CountRulesInText(content);
				if (count <= 0)
					throw std::runtime_error("The response contains no valid IP rules");
				if (combinedContent.size() + content.size()
					> MaxCombinedSubscriptionBytes)
				{
					throw std::length_error(
						"The combined subscription data exceeds 128 MiB");
				}

				if (!combinedContent.empty())
					combinedContent.push_back('\n');
				combinedContent += "# OpenNet-Source: ";
				combinedContent += subscription.url;
				combinedContent.push_back('\n');
				combinedContent += content;
				downloadedRules += count;
				++succeeded;
				manager.SetSubscriptionUpdateResult(
					subscription.id, now, true, count, "");
			}
			catch (winrt::hresult_error const& error)
			{
				++failed;
				manager.SetSubscriptionUpdateResult(
					subscription.id,
					now,
					false,
					0,
					winrt::to_string(error.message()));
			}
			catch (std::exception const& error)
			{
				++failed;
				manager.SetSubscriptionUpdateResult(
					subscription.id, now, false, 0, error.what());
			}
			catch (...)
			{
				++failed;
				manager.SetSubscriptionUpdateResult(
					subscription.id, now, false, 0, "Unknown download error");
			}
		}

		std::int32_t imported = 0;
		co_await winrt::resume_background();
		auto const replaceExisting = manager.SubscriptionReplaceExisting();
		auto const replacementSkipped =
			replaceExisting && succeeded > 0 && failed > 0;
		if (succeeded > 0 && !replacementSkipped)
		{
			imported = manager.ImportFromText(
				combinedContent, replaceExisting);
			manager.ApplyToSession();
		}

		std::wstring summary;
		if (succeeded > 0)
		{
			summary = ResourceText(
				L"IPF_UpdateSummarySuccess", L"Updated sources").c_str();
			summary += L" " + std::to_wstring(succeeded) + L"/" +
				std::to_wstring(subscriptions.size());
			summary += L" · ";
			summary += ResourceText(
				L"IPF_UpdateSummaryRules", L"valid rules").c_str();
			summary += L" " + std::to_wstring(downloadedRules);
			summary += L" · ";
			summary += ResourceText(
				L"IPF_UpdateSummaryImported", L"database changes").c_str();
			summary += L" " + std::to_wstring(imported);
			if (failed > 0)
			{
				summary += L" · ";
				summary += ResourceText(
					L"IPF_UpdateSummaryFailed", L"failed").c_str();
				summary += L" " + std::to_wstring(failed);
			}
			if (replacementSkipped)
			{
				summary += L" · ";
				summary += ResourceText(
					L"IPF_ReplacementSkipped",
					L"replacement skipped to preserve existing rules").c_str();
			}
		}
		else
		{
			summary = ResourceText(
				L"IPF_UpdateAllFailed", L"All subscription updates failed.").c_str();
		}

		manager.SetSubscriptionLastResult(
			now, winrt::to_string(winrt::hstring{ summary }));

		if (notify)
		{
			auto const severity = succeeded == 0
				? InfoBarSeverity::Error
				: failed > 0
				? InfoBarSeverity::Warning
				: InfoBarSeverity::Success;
			winrt::OpenNet::UI::Xaml::View::implementation::InfoBarView::Show(
				ResourceText(L"IPF_NotifyTitle", L"IP filter subscriptions"),
				winrt::hstring{ summary },
				severity,
				succeeded == 0 ? 0 : 8000);
		}
	}

	IPFilterSettingsPage::IPFilterSettingsPage()
	{
		InitializeComponent();
		RulesFolderPathText().Text(winrt::hstring{ RulesFolderPath().wstring() });

		Loaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			LoadState();
		});
	}

	winrt::fire_and_forget IPFilterSettingsPage::LoadState()
	{
		auto strong = get_strong();
		auto dispatcher = DispatcherQueue();

		co_await winrt::resume_background();

		auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
		mgr.Initialize();
		bool enabled = mgr.IsEnabled();
		auto rules = mgr.GetAllRules();

		co_await winrtplus::resume_foreground(dispatcher);

		m_loading = true;
		EnableFilterToggle().IsOn(enabled);
		m_allRules = std::move(rules);
		RebuildRuleItems();
		LoadSubscriptionState();
		m_loading = false;
	}

	winrt::fire_and_forget IPFilterSettingsPage::RefreshRules()
	{
		auto strong = get_strong();
		auto dispatcher = DispatcherQueue();
		auto const generation = ++m_rulesRefreshGeneration;

		co_await winrt::resume_background();
		auto rules = ::OpenNet::Core::IPFilterManager::Instance().GetAllRules();
		co_await winrtplus::resume_foreground(dispatcher);

		if (generation != m_rulesRefreshGeneration)
			co_return;
		m_allRules = std::move(rules);
		RebuildRuleItems();
	}

	void IPFilterSettingsPage::RebuildRuleItems()
	{
		auto const query = LowerAscii(
			winrt::to_string(RuleSearchTextBox().Text()));
		auto items = winrt::single_threaded_observable_vector<IInspectable>();
		m_visibleRules.clear();

		std::size_t matched = 0;
		for (auto const& rule : m_allRules)
		{
			auto searchable = LowerAscii(
				rule.firstIp + "\n" + rule.lastIp + "\n" +
				rule.description + "\n" + std::to_string(rule.id));
			if (!query.empty() && searchable.find(query) == std::string::npos)
				continue;

			++matched;
			if (m_visibleRules.size() >= MaxVisibleRules)
				continue;
			m_visibleRules.push_back(rule);
			items.Append(winrt::box_value(RuleDisplayText(rule)));
		}

		m_ruleItems = items;
		RulesList().ItemsSource(m_ruleItems);
		RuleCountText().Text(winrt::to_hstring(m_allRules.size()));
		RulesShownText().Text(
			winrt::to_hstring(m_visibleRules.size()) + L" / " +
			winrt::to_hstring(matched) + L" (" +
			winrt::to_hstring(m_allRules.size()) + L")");
		EditSelectedRuleButton().IsEnabled(false);
		DeleteSelectedRuleButton().IsEnabled(false);
	}

	void IPFilterSettingsPage::LoadSubscriptionState()
	{
		auto& manager = ::OpenNet::Core::IPFilterManager::Instance();
		auto const wasLoading = m_loading;
		m_loading = true;
		m_subscriptions = manager.GetSubscriptions();
		SubscriptionAutoUpdateToggle().IsOn(
			manager.SubscriptionAutoUpdateEnabled());
		auto const replace = manager.SubscriptionReplaceExisting();
		SubscriptionMergeRadio().IsChecked(!replace);
		SubscriptionReplaceRadio().IsChecked(replace);
		SubscriptionIntervalNumberBox().Value(
			manager.SubscriptionUpdateIntervalHours());

		auto const lastUpdate = manager.SubscriptionLastUpdate();
		auto const lastResult = manager.SubscriptionLastResult();
		std::wstring status = FormatTimestamp(lastUpdate).c_str();
		if (!lastResult.empty())
		{
			status += L" · ";
			status += winrt::to_hstring(lastResult).c_str();
		}
		SubscriptionLastResultText().Text(winrt::hstring{ status });
		RebuildSubscriptionItems();
		m_loading = wasLoading;
	}

	void IPFilterSettingsPage::RebuildSubscriptionItems()
	{
		auto items = winrt::single_threaded_observable_vector<IInspectable>();
		for (auto const& subscription : m_subscriptions)
		{
			std::wstring line = subscription.enabled ? L"● " : L"○ ";
			line += winrt::to_hstring(subscription.url).c_str();
			line += L"\n";
			if (subscription.lastUpdated <= 0)
			{
				line += ResourceText(L"IPF_NeverUpdated", L"Not updated yet").c_str();
			}
			else if (subscription.lastStatus == "success")
			{
				line += ResourceText(L"IPF_SourceSuccess", L"Success").c_str();
				line += L" · " + std::to_wstring(subscription.ruleCount) + L" ";
				line += ResourceText(L"IPF_Rules", L"rules").c_str();
				line += L" · " + std::wstring{ FormatTimestamp(
					subscription.lastUpdated).c_str() };
			}
			else
			{
				line += ResourceText(L"IPF_SourceFailed", L"Failed").c_str();
				if (!subscription.lastError.empty())
				{
					line += L" · ";
					line += winrt::to_hstring(subscription.lastError).c_str();
				}
			}
			items.Append(winrt::box_value(winrt::hstring{ line }));
		}
		m_subscriptionItems = items;
		SubscriptionList().ItemsSource(m_subscriptionItems);
		EditSubscriptionButton().IsEnabled(false);
		DeleteSubscriptionButton().IsEnabled(false);
	}

	void IPFilterSettingsPage::SetSubscriptionBusy(bool value)
	{
		UpdateSubscriptionsButton().IsEnabled(!value);
		SubscriptionUpdateProgress().IsActive(value);
		SubscriptionUpdateProgress().Visibility(
			value ? Visibility::Visible : Visibility::Collapsed);
	}

	std::optional<::OpenNet::Core::IPFilterSubscription>
		IPFilterSettingsPage::SelectedSubscription()
	{
		auto const index = SubscriptionList().SelectedIndex();
		if (index < 0 || static_cast<std::size_t>(index) >= m_subscriptions.size())
			return std::nullopt;
		return m_subscriptions[static_cast<std::size_t>(index)];
	}

	void IPFilterSettingsPage::OnSubscriptionAutoUpdateToggled(
		IInspectable const&, RoutedEventArgs const&)
	{
		if (m_loading)
			return;
		::OpenNet::Core::IPFilterManager::Instance().
			SubscriptionAutoUpdateEnabled(SubscriptionAutoUpdateToggle().IsOn());
	}

	void IPFilterSettingsPage::OnSubscriptionModeChanged(
		IInspectable const&, RoutedEventArgs const&)
	{
		if (m_loading)
			return;
		auto const checked = SubscriptionReplaceRadio().IsChecked();
		::OpenNet::Core::IPFilterManager::Instance().
			SubscriptionReplaceExisting(checked && checked.Value());
	}

	void IPFilterSettingsPage::OnSubscriptionIntervalChanged(
		IInspectable const&, NumberBoxValueChangedEventArgs const& args)
	{
		if (m_loading || !std::isfinite(args.NewValue()))
			return;
		::OpenNet::Core::IPFilterManager::Instance().
			SubscriptionUpdateIntervalHours(
				static_cast<std::int32_t>(std::round(args.NewValue())));
	}

	void IPFilterSettingsPage::OnAddSubscriptionClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		std::istringstream stream(
			winrt::to_string(SubscriptionUrlTextBox().Text()));
		std::string line;
		std::int32_t added = 0;
		std::int32_t rejected = 0;
		auto& manager = ::OpenNet::Core::IPFilterManager::Instance();
		while (std::getline(stream, line))
		{
			line = TrimCopy(std::move(line));
			if (line.empty())
				continue;
			if (!IsHttpSubscriptionUrl(winrt::to_hstring(line)))
			{
				++rejected;
				continue;
			}
			if (manager.AddSubscription(line) > 0)
				++added;
			else
				++rejected;
		}
		if (added == 0)
		{
			ShowStatus(
				ResourceText(
					L"IPF_InvalidOrDuplicateSubscriptions",
					L"No new valid subscription URLs were found."),
				InfoBarSeverity::Warning);
			return;
		}
		SubscriptionUrlTextBox().Text(L"");
		LoadSubscriptionState();
		std::wstring result = ResourceText(
			L"IPF_SubscriptionAdded", L"Subscription sources added").c_str();
		result += L": " + std::to_wstring(added);
		if (rejected > 0)
		{
			result += L" · " + std::to_wstring(rejected) + L" ";
			result += ResourceText(L"IPF_Skipped", L"skipped").c_str();
		}
		ShowStatus(winrt::hstring{ result }, InfoBarSeverity::Success);
	}

	void IPFilterSettingsPage::OnSubscriptionSelectionChanged(
		IInspectable const&, SelectionChangedEventArgs const&)
	{
		auto const selected = SelectedSubscription().has_value();
		EditSubscriptionButton().IsEnabled(selected);
		DeleteSubscriptionButton().IsEnabled(selected);
	}

	winrt::fire_and_forget IPFilterSettingsPage::OnEditSubscriptionClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto selected = SelectedSubscription();
		if (!selected)
			co_return;

		TextBox urlBox;
		urlBox.Text(winrt::to_hstring(selected->url));
		urlBox.MinWidth(480);
		CheckBox enabledBox;
		enabledBox.Content(winrt::box_value(
			ResourceText(L"IPF_SourceEnabled", L"Enable this source")));
		enabledBox.IsChecked(selected->enabled);
		StackPanel content;
		content.Spacing(10);
		content.Children().Append(urlBox);
		content.Children().Append(enabledBox);

		ContentDialog dialog;
		dialog.Style(Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		dialog.XamlRoot(XamlRoot());
		dialog.Title(winrt::box_value(
			ResourceText(L"IPF_EditSubscriptionTitle", L"Edit subscription")));
		dialog.Content(content);
		dialog.PrimaryButtonText(ResourceText(L"IPF_Save", L"Save"));
		dialog.CloseButtonText(ResourceText(L"IPF_Cancel", L"Cancel"));
		dialog.DefaultButton(ContentDialogButton::Primary);
		if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;
		if (!IsHttpSubscriptionUrl(urlBox.Text()))
		{
			ShowStatus(
				ResourceText(L"IPF_InvalidSubscriptionUrl", L"Enter a valid HTTP or HTTPS URL."),
				InfoBarSeverity::Warning);
			co_return;
		}
		auto const checked = enabledBox.IsChecked();
		if (!::OpenNet::Core::IPFilterManager::Instance().UpdateSubscription(
			selected->id,
			winrt::to_string(urlBox.Text()),
			checked && checked.Value()))
		{
			ShowStatus(
				ResourceText(L"IPF_SubscriptionUpdateFailed", L"The subscription could not be saved."),
				InfoBarSeverity::Warning);
			co_return;
		}
		LoadSubscriptionState();
	}

	winrt::fire_and_forget IPFilterSettingsPage::OnDeleteSubscriptionClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto selected = SelectedSubscription();
		if (!selected)
			co_return;

		ContentDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Title(winrt::box_value(
			ResourceText(L"IPF_DeleteSubscriptionTitle", L"Delete subscription")));
		dialog.Content(winrt::box_value(winrt::to_hstring(selected->url)));
		dialog.PrimaryButtonText(ResourceText(L"IPF_Delete", L"Delete"));
		dialog.CloseButtonText(ResourceText(L"IPF_Cancel", L"Cancel"));
		dialog.DefaultButton(ContentDialogButton::Close);
		if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;
		::OpenNet::Core::IPFilterManager::Instance().RemoveSubscription(
			selected->id);
		LoadSubscriptionState();
	}

	winrt::fire_and_forget IPFilterSettingsPage::OnUpdateSubscriptionsClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		SetSubscriptionBusy(true);
		try
		{
			co_await RunSubscriptionUpdateAsync(true, true);
			LoadSubscriptionState();
			RefreshRules();
		}
		catch (winrt::hresult_error const& error)
		{
			ShowStatus(error.message(), InfoBarSeverity::Error);
		}
		catch (...)
		{
			ShowStatus(
				ResourceText(L"IPF_UpdateUnexpectedError", L"The subscription update failed unexpectedly."),
				InfoBarSeverity::Error);
		}
		SetSubscriptionBusy(false);
	}

	void IPFilterSettingsPage::ShowStatus(winrt::hstring const& message, InfoBarSeverity severity)
	{
		StatusInfoBar().Message(message);
		StatusInfoBar().Severity(severity);
		StatusInfoBar().IsOpen(true);
	}

	void IPFilterSettingsPage::OnEnableToggled(IInspectable const&, RoutedEventArgs const&)
	{
		if (m_loading) return;

		auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
		mgr.SetEnabled(EnableFilterToggle().IsOn());
		mgr.ApplyToSession();
	}

	void IPFilterSettingsPage::OnAddRuleClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto text = winrt::to_string(ManualIpTextBox().Text());
		if (text.empty()) return;

		auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
		int added = mgr.ImportFromText(text);

		if (added > 0)
		{
			mgr.ApplyToSession();
			ManualIpTextBox().Text(L"");
			RefreshRules();
			ShowStatus(winrt::to_hstring(std::to_string(added) + " rule(s) added"),
					   InfoBarSeverity::Success);
		}
		else
		{
			ShowStatus(L"No valid IP addresses found in input",
					   InfoBarSeverity::Warning);
		}
	}

	winrt::fire_and_forget IPFilterSettingsPage::OnImportClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();

		// Create file picker using the WinUI 3 AppWindow-based API
		auto picker = FileOpenPicker(this->XamlRoot().ContentIslandEnvironment().AppWindowId());
		picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
		picker.FileTypeFilter().Append(L".txt");
		picker.FileTypeFilter().Append(L".dat");
		picker.FileTypeFilter().Append(L".p2p");
		picker.FileTypeFilter().Append(L"*");

		auto file = co_await picker.PickSingleFileAsync();
		if (!file) co_return;

		auto dispatcher = DispatcherQueue();

		// Read file on background thread
		co_await winrt::resume_background();

		std::string content;
		try
		{
			auto path = winrt::to_string(file.Path());
			std::ifstream ifs(path, std::ios::binary);
			if (ifs)
			{
				std::ostringstream oss;
				oss << ifs.rdbuf();
				content = oss.str();
			}
		}
		catch (...)
		{ /* fallthrough */
		}

		int added = 0;
		if (!content.empty())
		{
			auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
			added = mgr.ImportFromText(content);
			if (added > 0)
				mgr.ApplyToSession();
		}

		co_await winrtplus::resume_foreground(dispatcher);

		RefreshRules();
		if (added > 0)
		{
			ShowStatus(winrt::to_hstring(std::to_string(added) + " rule(s) imported from file"),
					   InfoBarSeverity::Success);
		}
		else
		{
			ShowStatus(L"No valid IP addresses found in file",
					   InfoBarSeverity::Warning);
		}
	}

	winrt::fire_and_forget IPFilterSettingsPage::OnOpenRulesFolderClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		try
		{
			auto folder = co_await winrt::Windows::Storage::StorageFolder::
				GetFolderFromPathAsync(
					winrt::hstring{ RulesFolderPath().wstring() });
			auto const launched = co_await winrt::Windows::System::Launcher::
				LaunchFolderAsync(folder);
			if (!launched)
				ShowStatus(L"Could not open the IP filter folder",
						   InfoBarSeverity::Warning);
		}
		catch (winrt::hresult_error const& error)
		{
			ShowStatus(
				winrt::hstring{ L"Could not open the IP filter folder: " } +
				error.message(), InfoBarSeverity::Error);
		}
	}

	winrt::fire_and_forget IPFilterSettingsPage::OnOpenRuleFileClick(IInspectable const& sender, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto button = sender.try_as<Button>();
		if (!button || !button.Tag())
			co_return;

		auto const fileName = winrt::unbox_value<winrt::hstring>(button.Tag());
		auto const path = RulesFolderPath() /
			std::wstring{ fileName.c_str() };
		try
		{
			auto file = co_await winrt::Windows::Storage::StorageFile::
				GetFileFromPathAsync(winrt::hstring{ path.wstring() });
			auto const launched = co_await winrt::Windows::System::Launcher::
				LaunchFileAsync(file);
			if (!launched)
				ShowStatus(winrt::hstring{ L"Could not open " } + fileName,
						   InfoBarSeverity::Warning);
		}
		catch (winrt::hresult_error const& error)
		{
			ShowStatus(
				winrt::hstring{ L"Could not open " } + fileName + L": " +
				error.message(), InfoBarSeverity::Error);
		}
	}

	void IPFilterSettingsPage::OnRefreshRulesClick(IInspectable const&, RoutedEventArgs const&)
	{
		RefreshRules();
	}

	void IPFilterSettingsPage::OnRuleSearchTextChanged(IInspectable const&, TextChangedEventArgs const&)
	{
		RebuildRuleItems();
	}

	void IPFilterSettingsPage::OnRuleSelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		auto const hasSelection = SelectedRule().has_value();
		EditSelectedRuleButton().IsEnabled(hasSelection);
		DeleteSelectedRuleButton().IsEnabled(hasSelection);
	}

	std::optional<::OpenNet::Core::IPRule> IPFilterSettingsPage::SelectedRule()
	{
		auto const index = RulesList().SelectedIndex();
		if (index < 0 || static_cast<std::size_t>(index) >= m_visibleRules.size())
			return std::nullopt;
		return m_visibleRules[static_cast<std::size_t>(index)];
	}

	winrt::fire_and_forget IPFilterSettingsPage::OnEditRuleClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto selected = SelectedRule();
		if (!selected)
			co_return;

		auto const originalEntry = RuleEntry(*selected);

		TextBlock entryLabel;
		entryLabel.Text(ResourceGetString(L"ViewIPFilterSettingsPageEntryLabel"));
		TextBox entryBox;
		entryBox.Text(winrt::to_hstring(originalEntry));
		entryBox.MinWidth(460);

		TextBlock descriptionLabel;
		descriptionLabel.Text(ResourceGetString(L"ViewIPFilterSettingsPageDescriptionOptional"));
		TextBox descriptionBox;
		if (selected->description != originalEntry)
			descriptionBox.Text(winrt::to_hstring(selected->description));

		StackPanel editor;
		editor.Spacing(8);
		editor.Children().Append(entryLabel);
		editor.Children().Append(entryBox);
		editor.Children().Append(descriptionLabel);
		editor.Children().Append(descriptionBox);

		ContentDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Title(winrt::box_value(ResourceGetString(L"ViewIPFilterSettingsPageEditRuleTitle")));
		dialog.Content(editor);
		dialog.PrimaryButtonText(ResourceGetString(L"CommonSave"));
		dialog.CloseButtonText(ResourceGetString(L"CommonCancel"));
		dialog.DefaultButton(ContentDialogButton::Primary);

		auto const result = co_await dialog.ShowAsync();
		if (result != ContentDialogResult::Primary)
			co_return;

		auto const entry = TrimCopy(winrt::to_string(entryBox.Text()));
		std::string first;
		std::string last;
		if (!::OpenNet::Core::IPFilterManager::ParseIPOrCIDR(
			entry, first, last))
		{
			ShowStatus(L"The edited rule is not a valid IP address, CIDR block, or range",
					   InfoBarSeverity::Warning);
			co_return;
		}

		auto description = TrimCopy(winrt::to_string(descriptionBox.Text()));
		if (description.empty())
			description = entry;

		auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
		if (!mgr.UpdateRule(selected->id, first, last, selected->flags, description))
		{
			ShowStatus(L"The rule could not be updated. It may duplicate another rule.",
					   InfoBarSeverity::Warning);
			co_return;
		}

		mgr.ApplyToSession();
		RefreshRules();
		ShowStatus(L"Rule updated", InfoBarSeverity::Success);
	}

	winrt::fire_and_forget IPFilterSettingsPage::OnDeleteRuleClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto selected = SelectedRule();
		if (!selected)
			co_return;

		ContentDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Title(winrt::box_value(ResourceGetString(L"ViewIPFilterSettingsPageDeleteRuleTitle")));
		dialog.Content(winrt::box_value(
			winrt::hstring{ L"Delete this rule?\n\n" } +
			winrt::to_hstring(RuleEntry(*selected))));
		dialog.PrimaryButtonText(ResourceGetString(L"CommonDelete"));
		dialog.CloseButtonText(ResourceGetString(L"CommonCancel"));
		dialog.DefaultButton(ContentDialogButton::Close);

		auto const result = co_await dialog.ShowAsync();
		if (result != ContentDialogResult::Primary)
			co_return;

		auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
		mgr.RemoveRule(selected->id);
		mgr.ApplyToSession();
		RefreshRules();
		ShowStatus(L"Rule deleted", InfoBarSeverity::Informational);
	}

	winrt::fire_and_forget IPFilterSettingsPage::OnClearAllClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();

		// Show confirmation dialog
		ContentDialog dialog;
		dialog.XamlRoot(this->XamlRoot());
		dialog.Title(box_value(ResourceGetString(L"ViewIPFilterSettingsPageClearAllRulesTitle")));
		dialog.Content(box_value(ResourceGetString(L"ViewIPFilterSettingsPageClearAllRulesMessage")));
		dialog.PrimaryButtonText(ResourceGetString(L"CommonClearAll"));
		dialog.CloseButtonText(ResourceGetString(L"CommonCancel"));
		dialog.DefaultButton(ContentDialogButton::Close);

		auto result = co_await dialog.ShowAsync();
		if (result != ContentDialogResult::Primary)
			co_return;

		auto& mgr = ::OpenNet::Core::IPFilterManager::Instance();
		mgr.ClearAllRules();
		mgr.ApplyToSession();

		RefreshRules();
		ShowStatus(L"All rules cleared", InfoBarSeverity::Informational);
	}
}
