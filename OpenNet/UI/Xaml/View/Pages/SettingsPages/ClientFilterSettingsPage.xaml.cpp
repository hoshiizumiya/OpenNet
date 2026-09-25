#include "XamlWorkaround.h"
#include "ClientFilterSettingsPage.xaml.h"
#include "SettingsPageTagRegister.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/ClientFilterSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/ClientFilterSettingsPage.g.cpp"
#endif

#include <include/ScopedButtonDisabler.hpp>

import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.WinUI.LiquidGlass;
import winrt.Microsoft.Windows.Storage.Pickers;
import OpenNet.Core.Utils.Message;
import winrt.OpenNet.UI.Xaml.View.Dialog;
import winrtplus_coroutine;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::Windows::Storage::Pickers;
using namespace winrt::Windows::Foundation;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	static SettingsPageTagRegister<ClientFilterSettingsPage> s_tags{ L"clientfilter", L"SettingsClientFilterSearchTags" };
	namespace
	{
		constexpr std::size_t MaxVisibleRules = 1000;
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

		char const* MatchTypeName(::OpenNet::Core::ClientMatchType type)
		{
			switch (type)
			{
				case ::OpenNet::Core::ClientMatchType::Exact:
					return "Exact";
				case ::OpenNet::Core::ClientMatchType::Wildcard:
					return "Wildcard";
				case ::OpenNet::Core::ClientMatchType::Regex:
					return "Regex";
				case ::OpenNet::Core::ClientMatchType::Contains:
				default:
					return "Contains";
			}
		}

		winrt::hstring LocalizedMatchTypeName(::OpenNet::Core::ClientMatchType type)
		{
			switch (type)
			{
				case ::OpenNet::Core::ClientMatchType::Exact: return ResourceGetString(L"ClientFilterMatchTypeExact");
				case ::OpenNet::Core::ClientMatchType::Wildcard: return ResourceGetString(L"ClientFilterMatchTypeWildcard");
				case ::OpenNet::Core::ClientMatchType::Regex: return ResourceGetString(L"ClientFilterMatchTypeRegex");
				case ::OpenNet::Core::ClientMatchType::Contains:
				default: return ResourceGetString(L"ClientFilterMatchTypeContains");
			}
		}

		::OpenNet::Core::ClientMatchType MatchTypeFromIndex(int index)
		{
			if (index < 0 || index > 3)
				index = 0;
			return static_cast<::OpenNet::Core::ClientMatchType>(index);
		}

		winrt::hstring RuleDisplayText(
			::OpenNet::Core::ClientFilterRule const& rule)
		{
			std::wstring text{ ResourceGetString(L"ClientFilterRuleNumberPrefix").c_str() };
			text += winrt::to_hstring(rule.id).c_str();
			text += (rule.enabled
				? ResourceGetString(L"ClientFilterRuleEnabledMarker")
				: ResourceGetString(L"ClientFilterRuleDisabledMarker")).c_str();
			text += LocalizedMatchTypeName(rule.matchType).c_str();
			text += (rule.caseSensitive
				? ResourceGetString(L"ClientFilterCaseSensitive")
				: ResourceGetString(L"ClientFilterIgnoreCase")).c_str();
			text += L" ";
			text += winrt::to_hstring(rule.pattern).c_str();
			if (!rule.description.empty())
			{
				text += ResourceGetString(L"ClientFilterDescriptionPrefix").c_str();
				text += winrt::to_hstring(rule.description).c_str();
			}
			text += ResourceGetString(L"ClientFilterHitsPrefix").c_str();
			text += winrt::to_hstring(rule.hitCount).c_str();
			return winrt::hstring{ text };
		}

		std::string FormatTimestamp(std::int64_t timestamp)
		{
			if (timestamp <= 0)
				return "-";
			constexpr std::int64_t UnixToFileTimeSeconds = 11644473600LL;
			const auto ticks = static_cast<std::uint64_t>(
				timestamp + UnixToFileTimeSeconds) * 10000000ULL;
			ULARGE_INTEGER value{};
			value.QuadPart = ticks;
			FILETIME utc{
				value.LowPart,
				value.HighPart
			};
			FILETIME localFileTime{};
			SYSTEMTIME local{};
			if (!FileTimeToLocalFileTime(&utc, &localFileTime)
				|| !FileTimeToSystemTime(&localFileTime, &local))
			{
				return std::to_string(timestamp);
			}
			std::ostringstream stream;
			stream << std::setfill('0')
				<< std::setw(4) << local.wYear << '-'
				<< std::setw(2) << local.wMonth << '-'
				<< std::setw(2) << local.wDay << ' '
				<< std::setw(2) << local.wHour << ':'
				<< std::setw(2) << local.wMinute << ':'
				<< std::setw(2) << local.wSecond;
			return stream.str();
		}

		winrt::hstring HitDisplayText(
			::OpenNet::Core::ClientFilterHit const& hit)
		{
			auto text = FormatTimestamp(hit.timestamp) + "  " +
				hit.client + "  ·  " + hit.ip;
			if (!hit.taskName.empty())
				text += "  ·  " + hit.taskName;
			if (!hit.pattern.empty())
				text += "  ·  matched \"" + hit.pattern + "\"";
			return winrt::to_hstring(text);
		}
	}

	ClientFilterSettingsPage::ClientFilterSettingsPage()
	{
		InitializeComponent();

		Loaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			LoadState();
			if (!m_refreshTimer)
			{
				m_refreshTimer = DispatcherQueue().CreateTimer();
				m_refreshTimer.Interval(std::chrono::seconds(3));
				auto weak = get_weak();
				m_refreshTimer.Tick(
					[weak](auto const&, auto const&)
				{
					if (auto self = weak.get())
						self->RefreshSnapshot();
				});
			}
			m_refreshTimer.Start();
		});

		Unloaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			if (m_refreshTimer)
				m_refreshTimer.Stop();
		});
	}

	winrt::fire_and_forget ClientFilterSettingsPage::LoadState()
	{
		auto strong = get_strong();
		auto dispatcher = DispatcherQueue();
		co_await winrt::resume_background();

		auto& manager = ::OpenNet::Core::ClientFilterManager::Instance();
		auto const initialized = manager.Initialize();
		auto const enabled = manager.IsEnabled();
		auto rules = initialized
			? manager.GetRules()
			: std::vector<::OpenNet::Core::ClientFilterRule>{};
		auto hits = initialized
			? manager.GetRecentHits()
			: std::vector<::OpenNet::Core::ClientFilterHit>{};
		auto const runtimeBlocks = manager.RuntimeBlockedCount();

		co_await winrtplus::resume_foreground(dispatcher);
		m_loading = true;
		SetToggleIsOn(EnableFilterToggle(), enabled);
		SetToggleIsOn(EnableFilterGlassToggle(), enabled);
		m_loading = false;
		m_allRules = std::move(rules);
		m_hits = std::move(hits);
		RuntimeBlocksText().Text(winrt::to_hstring(runtimeBlocks));
		RebuildRuleItems();
		RebuildHistoryItems();

		if (!initialized)
			ShowStatus(ResourceGetString(L"ClientFilterDatabaseOpenFailed"),
					   InfoBarSeverity::Error);
	}

	winrt::fire_and_forget ClientFilterSettingsPage::RefreshSnapshot()
	{
		auto strong = get_strong();
		auto dispatcher = DispatcherQueue();
		auto const generation = ++m_refreshGeneration;
		co_await winrt::resume_background();

		auto& manager = ::OpenNet::Core::ClientFilterManager::Instance();
		auto rules = manager.GetRules();
		auto hits = manager.GetRecentHits();
		auto const runtimeBlocks = manager.RuntimeBlockedCount();

		co_await winrtplus::resume_foreground(dispatcher);
		if (generation != m_refreshGeneration)
			co_return;
		m_allRules = std::move(rules);
		m_hits = std::move(hits);
		RuntimeBlocksText().Text(winrt::to_hstring(runtimeBlocks));
		RebuildRuleItems();
		RebuildHistoryItems();
	}

	void ClientFilterSettingsPage::RebuildRuleItems()
	{
		auto const previousSelection = SelectedRule();
		auto const query = LowerAscii(
			winrt::to_string(RuleSearchTextBox().Text()));
		auto items = winrt::single_threaded_observable_vector<IInspectable>();
		m_visibleRules.clear();

		std::size_t matched = 0;
		int selectedIndex = -1;
		for (auto const& rule : m_allRules)
		{
			auto searchable = LowerAscii(
				rule.pattern + "\n" + rule.description + "\n" +
				MatchTypeName(rule.matchType) + "\n" +
				std::to_string(rule.id));
			if (!query.empty() &&
				searchable.find(query) == std::string::npos)
			{
				continue;
			}

			++matched;
			if (m_visibleRules.size() >= MaxVisibleRules)
				continue;
			if (previousSelection && rule.id == previousSelection->id)
				selectedIndex = static_cast<int>(m_visibleRules.size());
			m_visibleRules.push_back(rule);
			items.Append(winrt::box_value(RuleDisplayText(rule)));
		}

		m_ruleItems = items;
		RulesList().ItemsSource(m_ruleItems);
		RulesList().SelectedIndex(selectedIndex);
		RuleCountText().Text(winrt::to_hstring(m_allRules.size()));
		RulesShownText().Text(
			winrt::to_hstring(m_visibleRules.size()) + L" / " +
			winrt::to_hstring(matched) + L" (" +
			winrt::to_hstring(m_allRules.size()) + L")");
		UpdateSelectionButtons();
	}

	void ClientFilterSettingsPage::RebuildHistoryItems()
	{
		auto items = winrt::single_threaded_observable_vector<IInspectable>();
		for (auto const& hit : m_hits)
			items.Append(winrt::box_value(HitDisplayText(hit)));
		m_hitItems = items;
		HistoryList().ItemsSource(m_hitItems);
	}

	void ClientFilterSettingsPage::ShowStatus(winrt::hstring const& message, InfoBarSeverity severity)
	{
		StatusInfoBar().Message(message);
		StatusInfoBar().Severity(severity);
		StatusInfoBar().IsOpen(true);
	}

	std::optional<::OpenNet::Core::ClientFilterRule> ClientFilterSettingsPage::SelectedRule()
	{
		auto const index = RulesList().SelectedIndex();
		if (index < 0 ||
			static_cast<std::size_t>(index) >= m_visibleRules.size())
		{
			return std::nullopt;
		}
		return m_visibleRules[static_cast<std::size_t>(index)];
	}

	void ClientFilterSettingsPage::UpdateSelectionButtons()
	{
		auto const selected = SelectedRule();
		auto const hasSelection = selected.has_value();
		ToggleSelectedRuleButton().IsEnabled(hasSelection);
		EditSelectedRuleButton().IsEnabled(hasSelection);
		DeleteSelectedRuleButton().IsEnabled(hasSelection);
	}

	void ClientFilterSettingsPage::OnEnableToggled(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_loading)
			return;
		auto const enabled = ToggleIsOn(sender);
		if (!enabled) return;
		m_loading = true;
		SetToggleIsOn(EnableFilterToggle(), *enabled);
		SetToggleIsOn(EnableFilterGlassToggle(), *enabled);
		m_loading = false;
		::OpenNet::Core::ClientFilterManager::Instance().SetEnabled(*enabled);
		RuntimeBlocksText().Text(L"0");
		ShowStatus(
			*enabled
			? ResourceGetString(L"ClientFilterEnabledStatus")
			: ResourceGetString(L"ClientFilterDisabledStatus"),
			InfoBarSeverity::Informational);
	}

	void ClientFilterSettingsPage::EnableFilterToggle_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		const bool wasLoading = m_loading;
		m_loading = true;
		SetToggleIsOn(sender, ::OpenNet::Core::ClientFilterManager::Instance().IsEnabled());
		m_loading = wasLoading;
	}

	void ClientFilterSettingsPage::CaseSensitiveToggle_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		const bool wasLoading = m_loading;
		m_loading = true;
		SetToggleIsOn(sender, m_caseSensitive);
		m_loading = wasLoading;
	}

	void ClientFilterSettingsPage::CaseSensitiveToggle_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_loading) return;
		if (auto enabled = ToggleIsOn(sender))
		{
			m_caseSensitive = *enabled;
			m_loading = true;
			SetToggleIsOn(CaseSensitiveToggle(), *enabled);
			SetToggleIsOn(CaseSensitiveGlassToggle(), *enabled);
			m_loading = false;
		}
	}

	void ClientFilterSettingsPage::OnAddRuleClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto const pattern = TrimCopy(
			winrt::to_string(PatternTextBox().Text()));
		auto const matchType =
			MatchTypeFromIndex(MatchTypeComboBox().SelectedIndex());
		std::string error;
		if (!::OpenNet::Core::ClientFilterManager::ValidatePattern(
			pattern, matchType, &error))
		{
			ShowStatus(
				ResourceGetString(L"ClientFilterInvalidPatternPrefix") +
				winrt::to_hstring(error),
				InfoBarSeverity::Warning);
			return;
		}

		auto& manager = ::OpenNet::Core::ClientFilterManager::Instance();
		if (!manager.AddRule(
			pattern,
			matchType,
			m_caseSensitive,
			TrimCopy(winrt::to_string(DescriptionTextBox().Text()))))
		{
			ShowStatus(ResourceGetString(L"ClientFilterRuleSaveFailed"),
					   InfoBarSeverity::Warning);
			return;
		}

		PatternTextBox().Text(L"");
		DescriptionTextBox().Text(L"");
		RefreshSnapshot();
		ShowStatus(ResourceGetString(L"ClientFilterRuleAdded"), InfoBarSeverity::Success);
	}

	void ClientFilterSettingsPage::OnTestClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto const client = TrimCopy(
			winrt::to_string(TestClientTextBox().Text()));
		if (client.empty())
		{
			TestResultText().Text(ResourceGetString(L"ViewClientFilterSettingsPageEnterClientNameHint"));
			return;
		}

		auto const matched =
			::OpenNet::Core::ClientFilterManager::Instance().
			MatchClient(client);
		if (!matched)
		{
			TestResultText().Text(ResourceGetString(L"ViewClientFilterSettingsPageNoMatchingRule"));
			return;
		}

		TestResultText().Text(
			ResourceGetString(L"ClientFilterMatchedRulePrefix") +
			winrt::to_hstring(matched->id) + L": " +
			winrt::to_hstring(matched->pattern) + L" (" +
			LocalizedMatchTypeName(matched->matchType) + L")");
	}

	void ClientFilterSettingsPage::OnRefreshClick(IInspectable const&, RoutedEventArgs const&)
	{
		RefreshSnapshot();
	}

	void ClientFilterSettingsPage::OnRuleSearchTextChanged(IInspectable const&, TextChangedEventArgs const&)
	{
		RebuildRuleItems();
	}

	void ClientFilterSettingsPage::OnRuleSelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		UpdateSelectionButtons();
	}

	void ClientFilterSettingsPage::OnToggleRuleClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto const selected = SelectedRule();
		if (!selected)
			return;
		::OpenNet::Core::ClientFilterManager::Instance().
			SetRuleEnabled(selected->id, !selected->enabled);
		RefreshSnapshot();
		ShowStatus(
			ResourceGetString(selected->enabled ? L"ClientFilterRuleDisabled" : L"ClientFilterRuleEnabled"),
			InfoBarSeverity::Informational);
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnEditRuleClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto const selected = SelectedRule();
		if (!selected)
			co_return;

		winrt::OpenNet::UI::Xaml::View::Dialog::FilterEditorDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.ConfigureClientRule(ResourceGetString(L"ViewClientFilterSettingsPageEditRuleTitle"), winrt::to_hstring(selected->pattern), static_cast<int>(selected->matchType), selected->caseSensitive, selected->enabled, winrt::to_hstring(selected->description), ResourceGetString(L"CommonSave"), ResourceGetString(L"CommonCancel"));
		if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;

		auto const pattern = TrimCopy(winrt::to_string(dialog.Pattern()));
		auto const matchType = MatchTypeFromIndex(dialog.MatchType());
		std::string error;
		if (!::OpenNet::Core::ClientFilterManager::ValidatePattern(
			pattern, matchType, &error))
		{
			ShowStatus(
				ResourceGetString(L"ClientFilterInvalidPatternPrefix") +
				winrt::to_hstring(error),
				InfoBarSeverity::Warning);
			co_return;
		}

		auto& manager = ::OpenNet::Core::ClientFilterManager::Instance();
		if (!manager.UpdateRule(
			selected->id,
			pattern,
			matchType,
			dialog.CaseSensitive(),
			dialog.Enabled(),
			TrimCopy(winrt::to_string(dialog.Description()))))
		{
			ShowStatus(
				ResourceGetString(L"ClientFilterRuleUpdateFailed"),
				InfoBarSeverity::Warning);
			co_return;
		}

		RefreshSnapshot();
		ShowStatus(ResourceGetString(L"ClientFilterRuleUpdated"), InfoBarSeverity::Success);
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnDeleteRuleClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto const selected = SelectedRule();
		if (!selected)
			co_return;

		winrt::OpenNet::UI::Xaml::View::Dialog::ConfirmationDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Configure(ResourceGetString(L"ViewClientFilterSettingsPageDeleteRuleTitle"), ResourceGetString(L"CF_DeleteRulePrompt"), winrt::to_hstring(selected->pattern), ResourceGetString(L"CommonDelete"), ResourceGetString(L"CommonCancel"), true, false, {});
		if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;

		::OpenNet::Core::ClientFilterManager::Instance().
			RemoveRule(selected->id);
		RefreshSnapshot();
		ShowStatus(ResourceGetString(L"ClientFilterRuleDeleted"), InfoBarSeverity::Informational);
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnImportClick(IInspectable const& sender, RoutedEventArgs const&)
	{
		ScopedButtonDisabler disabler{ sender };
		auto strong = get_strong();

		winrt::OpenNet::UI::Xaml::View::Dialog::FilterEditorDialog optionsDialog;
		optionsDialog.XamlRoot(XamlRoot());
		optionsDialog.ConfigureClientImport(ResourceGetString(L"ViewClientFilterSettingsPageImportRulesTitle"), ResourceGetString(L"CF_ImportHint.Text"), false, ResourceGetString(L"ViewClientFilterSettingsPageChooseFile"), ResourceGetString(L"CommonCancel"));
		if (co_await optionsDialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;
		auto const replaceExisting = optionsDialog.ReplaceExisting();

		FileOpenPicker picker(XamlRoot().ContentIslandEnvironment().AppWindowId());
		picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
		picker.FileTypeFilter().Append(L".json");
		picker.FileTypeFilter().Append(L".txt");
		picker.FileTypeFilter().Append(L"*");
		auto file = co_await picker.PickSingleFileAsync();
		if (!file)
			co_return;

		auto dispatcher = DispatcherQueue();
		co_await winrt::resume_background();
		std::string content;
		try
		{
			std::ifstream stream(
				std::filesystem::path{ file.Path().c_str() },
				std::ios::binary);
			std::ostringstream buffer;
			buffer << stream.rdbuf();
			content = buffer.str();
		}
		catch (...)
		{
		}

		auto imported = content.empty()
			? 0
			: ::OpenNet::Core::ClientFilterManager::Instance().
			ImportRules(content, replaceExisting);
		co_await winrtplus::resume_foreground(dispatcher);

		RefreshSnapshot();
		if (imported > 0)
		{
			ShowStatus(
				ResourceGetString(L"ClientFilterRulesImportedPrefix") + winrt::to_hstring(imported) + ResourceGetString(L"ClientFilterRulesImportedSuffix"),
				InfoBarSeverity::Success);
		}
		else
		{
			ShowStatus(ResourceGetString(L"ClientFilterNoValidRulesImported"),
					   InfoBarSeverity::Warning);
		}
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnExportClick(IInspectable const& sender, RoutedEventArgs const&)
	{
		ScopedButtonDisabler disabler{ sender };
		auto strong = get_strong();

		FileSavePicker picker(XamlRoot().ContentIslandEnvironment().AppWindowId());
		picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
		picker.SuggestedFileName(L"client-filter-rules.json");
		picker.DefaultFileExtension(L".json");
		auto fileTypes = winrt::single_threaded_vector<hstring>();
		fileTypes.Append(L".json");
		picker.FileTypeChoices().Insert(L"JSON document", fileTypes);

		auto file = co_await picker.PickSaveFileAsync();
		if (!file)
			co_return;

		auto dispatcher = DispatcherQueue();
		co_await winrt::resume_background();
		bool saved = false;
		try
		{
			auto const content = ::OpenNet::Core::ClientFilterManager::Instance().ExportRules();
			std::ofstream stream(
				std::filesystem::path{ file.Path().c_str() },
				std::ios::binary | std::ios::trunc);
			stream.write(
				content.data(),
				static_cast<std::streamsize>(content.size()));
			saved = stream.good();
		}
		catch (...)
		{
		}
		co_await winrtplus::resume_foreground(dispatcher);

		ShowStatus(
			saved ? ResourceGetString(L"ClientFilterRulesExported") : ResourceGetString(L"ClientFilterExportFailed"),
			saved ? InfoBarSeverity::Success : InfoBarSeverity::Error);
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnClearRulesClick(IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		winrt::OpenNet::UI::Xaml::View::Dialog::ConfirmationDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Configure(ResourceGetString(L"ViewClientFilterSettingsPageClearRulesTitle"), ResourceGetString(L"CF_ClearRulesMessage.Text"), {}, ResourceGetString(L"CommonClearAll"), ResourceGetString(L"CommonCancel"), true, false, {});
		if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;

		::OpenNet::Core::ClientFilterManager::Instance().ClearRules();
		RefreshSnapshot();
		ShowStatus(ResourceGetString(L"ClientFilterRulesCleared"), InfoBarSeverity::Informational);
	}

	void ClientFilterSettingsPage::OnClearHistoryClick(IInspectable const&, RoutedEventArgs const&)
	{
		::OpenNet::Core::ClientFilterManager::Instance().ClearHitHistory();
		RefreshSnapshot();
		ShowStatus(ResourceGetString(L"ClientFilterHitHistoryCleared"), InfoBarSeverity::Informational);
	}
}
