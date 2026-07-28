#include "XamlWorkaround.h"
#include "IPFilterSettingsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.g.cpp"
#endif

#include "Core/IPFilter/IPFilterManager.h"

import OpenNet.Core.IO.FileSystem;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.Windows.Storage.Pickers;
import winrt.Windows.Storage;
import winrt.Windows.System;
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
		entryLabel.Text(L"IP address, CIDR block, or range");
		TextBox entryBox;
		entryBox.Text(winrt::to_hstring(originalEntry));
		entryBox.MinWidth(460);

		TextBlock descriptionLabel;
		descriptionLabel.Text(L"Description (optional)");
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
		dialog.Title(winrt::box_value(L"Edit IP Filter Rule"));
		dialog.Content(editor);
		dialog.PrimaryButtonText(L"Save");
		dialog.CloseButtonText(L"Cancel");
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
		dialog.Title(winrt::box_value(L"Delete IP Filter Rule"));
		dialog.Content(winrt::box_value(
			winrt::hstring{ L"Delete this rule?\n\n" } +
			winrt::to_hstring(RuleEntry(*selected))));
		dialog.PrimaryButtonText(L"Delete");
		dialog.CloseButtonText(L"Cancel");
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
		dialog.Title(box_value(L"Clear All Rules"));
		dialog.Content(box_value(L"Are you sure you want to remove all IP filter rules? This action cannot be undone."));
		dialog.PrimaryButtonText(L"Clear All");
		dialog.CloseButtonText(L"Cancel");
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
