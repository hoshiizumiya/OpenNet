#include "XamlWorkaround.h"
#include "ClientFilterSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/ClientFilterSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/ClientFilterSettingsPage.g.cpp"
#endif

import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.Windows.Storage.Pickers;
import winrtplus_coroutine;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::Windows::Storage::Pickers;
using namespace winrt::Windows::Foundation;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	namespace
	{
		constexpr std::size_t MaxVisibleRules = 1000;

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

		::OpenNet::Core::ClientMatchType MatchTypeFromIndex(int index)
		{
			if (index < 0 || index > 3)
				index = 0;
			return static_cast<::OpenNet::Core::ClientMatchType>(index);
		}

		winrt::hstring RuleDisplayText(
			::OpenNet::Core::ClientFilterRule const& rule)
		{
			auto text = "#" + std::to_string(rule.id) +
				(rule.enabled ? "  [+]  " : "  [-]  ") +
				MatchTypeName(rule.matchType) +
				(rule.caseSensitive ? " / case  " : " / ignore-case  ") +
				rule.pattern;
			if (!rule.description.empty())
				text += "  —  " + rule.description;
			text += "  · hits " + std::to_string(rule.hitCount);
			return winrt::to_hstring(text);
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
		EnableFilterToggle().IsOn(enabled);
		m_loading = false;
		m_allRules = std::move(rules);
		m_hits = std::move(hits);
		RuntimeBlocksText().Text(winrt::to_hstring(runtimeBlocks));
		RebuildRuleItems();
		RebuildHistoryItems();

		if (!initialized)
			ShowStatus(L"Client filter database could not be opened",
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

	void ClientFilterSettingsPage::ShowStatus(
		winrt::hstring const& message, InfoBarSeverity severity)
	{
		StatusInfoBar().Message(message);
		StatusInfoBar().Severity(severity);
		StatusInfoBar().IsOpen(true);
	}

	std::optional<::OpenNet::Core::ClientFilterRule>
		ClientFilterSettingsPage::SelectedRule()
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

	void ClientFilterSettingsPage::OnEnableToggled(
		IInspectable const&, RoutedEventArgs const&)
	{
		if (m_loading)
			return;
		auto const enabled = EnableFilterToggle().IsOn();
		::OpenNet::Core::ClientFilterManager::Instance().SetEnabled(enabled);
		RuntimeBlocksText().Text(L"0");
		ShowStatus(
			enabled
			? L"Client filtering enabled"
			: L"Client filtering disabled and transient blocks cleared",
			InfoBarSeverity::Informational);
	}

	void ClientFilterSettingsPage::OnAddRuleClick(
		IInspectable const&, RoutedEventArgs const&)
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
				winrt::hstring{ L"Invalid pattern: " } +
				winrt::to_hstring(error),
				InfoBarSeverity::Warning);
			return;
		}

		auto& manager = ::OpenNet::Core::ClientFilterManager::Instance();
		if (!manager.AddRule(
			pattern,
			matchType,
			CaseSensitiveToggle().IsOn(),
			TrimCopy(winrt::to_string(DescriptionTextBox().Text()))))
		{
			ShowStatus(L"The rule already exists or could not be saved",
					   InfoBarSeverity::Warning);
			return;
		}

		PatternTextBox().Text(L"");
		DescriptionTextBox().Text(L"");
		RefreshSnapshot();
		ShowStatus(L"Client filter rule added", InfoBarSeverity::Success);
	}

	void ClientFilterSettingsPage::OnTestClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto const client = TrimCopy(
			winrt::to_string(TestClientTextBox().Text()));
		if (client.empty())
		{
			TestResultText().Text(L"Enter a peer client name to test.");
			return;
		}

		auto const matched =
			::OpenNet::Core::ClientFilterManager::Instance().
			MatchClient(client);
		if (!matched)
		{
			TestResultText().Text(L"No enabled rule matches this client.");
			return;
		}

		TestResultText().Text(
			winrt::hstring{ L"Matched rule #" } +
			winrt::to_hstring(matched->id) + L": " +
			winrt::to_hstring(matched->pattern) + L" (" +
			winrt::to_hstring(MatchTypeName(matched->matchType)) + L")");
	}

	void ClientFilterSettingsPage::OnRefreshClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		RefreshSnapshot();
	}

	void ClientFilterSettingsPage::OnRuleSearchTextChanged(
		IInspectable const&, TextChangedEventArgs const&)
	{
		RebuildRuleItems();
	}

	void ClientFilterSettingsPage::OnRuleSelectionChanged(
		IInspectable const&, SelectionChangedEventArgs const&)
	{
		UpdateSelectionButtons();
	}

	void ClientFilterSettingsPage::OnToggleRuleClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto const selected = SelectedRule();
		if (!selected)
			return;
		::OpenNet::Core::ClientFilterManager::Instance().
			SetRuleEnabled(selected->id, !selected->enabled);
		RefreshSnapshot();
		ShowStatus(
			selected->enabled ? L"Rule disabled" : L"Rule enabled",
			InfoBarSeverity::Informational);
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnEditRuleClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto const selected = SelectedRule();
		if (!selected)
			co_return;

		TextBlock patternLabel;
		patternLabel.Text(L"Pattern");
		TextBox patternBox;
		patternBox.Text(winrt::to_hstring(selected->pattern));
		patternBox.MinWidth(460);

		TextBlock matchLabel;
		matchLabel.Text(L"Match type");
		ComboBox matchBox;
		matchBox.HorizontalAlignment(HorizontalAlignment::Stretch);
		matchBox.Items().Append(winrt::box_value(L"Contains"));
		matchBox.Items().Append(winrt::box_value(L"Exact"));
		matchBox.Items().Append(winrt::box_value(L"Wildcard"));
		matchBox.Items().Append(winrt::box_value(L"Regular expression"));
		matchBox.SelectedIndex(static_cast<int>(selected->matchType));

		ToggleSwitch caseToggle;
		caseToggle.Header(winrt::box_value(L"Case sensitive"));
		caseToggle.IsOn(selected->caseSensitive);

		ToggleSwitch enabledToggle;
		enabledToggle.Header(winrt::box_value(L"Enabled"));
		enabledToggle.IsOn(selected->enabled);

		TextBlock descriptionLabel;
		descriptionLabel.Text(L"Description");
		TextBox descriptionBox;
		descriptionBox.Text(winrt::to_hstring(selected->description));

		StackPanel editor;
		editor.Spacing(8);
		editor.Children().Append(patternLabel);
		editor.Children().Append(patternBox);
		editor.Children().Append(matchLabel);
		editor.Children().Append(matchBox);
		editor.Children().Append(caseToggle);
		editor.Children().Append(enabledToggle);
		editor.Children().Append(descriptionLabel);
		editor.Children().Append(descriptionBox);

		ContentDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Title(winrt::box_value(L"Edit Client Filter Rule"));
		dialog.Content(editor);
		dialog.PrimaryButtonText(L"Save");
		dialog.CloseButtonText(L"Cancel");
		dialog.DefaultButton(ContentDialogButton::Primary);
		if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;

		auto const pattern = TrimCopy(winrt::to_string(patternBox.Text()));
		auto const matchType = MatchTypeFromIndex(matchBox.SelectedIndex());
		std::string error;
		if (!::OpenNet::Core::ClientFilterManager::ValidatePattern(
			pattern, matchType, &error))
		{
			ShowStatus(
				winrt::hstring{ L"Invalid pattern: " } +
				winrt::to_hstring(error),
				InfoBarSeverity::Warning);
			co_return;
		}

		auto& manager = ::OpenNet::Core::ClientFilterManager::Instance();
		if (!manager.UpdateRule(
			selected->id,
			pattern,
			matchType,
			caseToggle.IsOn(),
			enabledToggle.IsOn(),
			TrimCopy(winrt::to_string(descriptionBox.Text()))))
		{
			ShowStatus(
				L"The rule could not be updated. It may duplicate another rule.",
				InfoBarSeverity::Warning);
			co_return;
		}

		RefreshSnapshot();
		ShowStatus(L"Rule updated", InfoBarSeverity::Success);
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnDeleteRuleClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto const selected = SelectedRule();
		if (!selected)
			co_return;

		ContentDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Title(winrt::box_value(L"Delete Client Filter Rule"));
		dialog.Content(winrt::box_value(
			winrt::hstring{ L"Delete this rule?\n\n" } +
			winrt::to_hstring(selected->pattern)));
		dialog.PrimaryButtonText(L"Delete");
		dialog.CloseButtonText(L"Cancel");
		dialog.DefaultButton(ContentDialogButton::Close);
		if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;

		::OpenNet::Core::ClientFilterManager::Instance().
			RemoveRule(selected->id);
		RefreshSnapshot();
		ShowStatus(L"Rule deleted", InfoBarSeverity::Informational);
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnImportClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();

		CheckBox replaceCheck;
		replaceCheck.Content(winrt::box_value(
			L"Replace all existing rules before importing"));
		TextBlock hint;
		hint.Text(
			L"JSON preserves every rule option. Plain text imports one "
			L"case-insensitive Contains rule per line.");
		hint.TextWrapping(TextWrapping::Wrap);
		StackPanel options;
		options.Spacing(8);
		options.Children().Append(hint);
		options.Children().Append(replaceCheck);

		ContentDialog optionsDialog;
		optionsDialog.XamlRoot(XamlRoot());
		optionsDialog.Title(winrt::box_value(L"Import Client Filter Rules"));
		optionsDialog.Content(options);
		optionsDialog.PrimaryButtonText(L"Choose File");
		optionsDialog.CloseButtonText(L"Cancel");
		optionsDialog.DefaultButton(ContentDialogButton::Primary);
		if (co_await optionsDialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;
		auto const checked = replaceCheck.IsChecked();
		auto const replaceExisting = checked && checked.Value();

		FileOpenPicker picker(
			XamlRoot().ContentIslandEnvironment().AppWindowId());
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
				winrt::to_hstring(imported) + L" rule(s) imported",
				InfoBarSeverity::Success);
		}
		else
		{
			ShowStatus(L"No new valid rules were imported",
					   InfoBarSeverity::Warning);
		}
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnExportClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		FileSavePicker picker(
			XamlRoot().ContentIslandEnvironment().AppWindowId());
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
			auto const content =
				::OpenNet::Core::ClientFilterManager::Instance().
				ExportRules();
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
			saved ? L"Rules exported" : L"Could not export rules",
			saved ? InfoBarSeverity::Success : InfoBarSeverity::Error);
	}

	winrt::fire_and_forget ClientFilterSettingsPage::OnClearRulesClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		auto strong = get_strong();
		ContentDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Title(winrt::box_value(L"Clear Client Filter Rules"));
		dialog.Content(winrt::box_value(
			L"Remove every client filter rule? This cannot be undone."));
		dialog.PrimaryButtonText(L"Clear All");
		dialog.CloseButtonText(L"Cancel");
		dialog.DefaultButton(ContentDialogButton::Close);
		if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
			co_return;

		::OpenNet::Core::ClientFilterManager::Instance().ClearRules();
		RefreshSnapshot();
		ShowStatus(L"All client filter rules cleared",
				   InfoBarSeverity::Informational);
	}

	void ClientFilterSettingsPage::OnClearHistoryClick(
		IInspectable const&, RoutedEventArgs const&)
	{
		::OpenNet::Core::ClientFilterManager::Instance().
			ClearHitHistory();
		RefreshSnapshot();
		ShowStatus(L"Hit history cleared", InfoBarSeverity::Informational);
	}
}
