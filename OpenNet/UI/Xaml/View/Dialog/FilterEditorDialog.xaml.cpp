#include "XamlWorkaround.h"
#include "FilterEditorDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/FilterEditorDialog.g.cpp")
#include "UI/Xaml/View/Dialog/FilterEditorDialog.g.cpp"
#endif

import OpenNet.Helpers.ThemeHelper;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	FilterEditorDialog::FilterEditorDialog()
	{
		Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
		RequestedTheme(::OpenNet::Helpers::ThemeHelper::RootTheme());
	}

	void FilterEditorDialog::ShowPanel(FrameworkElement const& panel)
	{
		ClientRulePanel().Visibility(Visibility::Collapsed);
		ClientImportPanel().Visibility(Visibility::Collapsed);
		SubscriptionPanel().Visibility(Visibility::Collapsed);
		IpRulePanel().Visibility(Visibility::Collapsed);
		panel.Visibility(Visibility::Visible);
	}

	void FilterEditorDialog::ConfigureClientRule(hstring const& title, hstring const& pattern, std::int32_t const matchType, bool const caseSensitive, bool const enabled, hstring const& description, hstring const& primaryButtonText, hstring const& closeButtonText)
	{
		ShowPanel(ClientRulePanel());
		Title(box_value(title));
		PrimaryButtonText(primaryButtonText);
		CloseButtonText(closeButtonText);
		PatternTextBox().Text(pattern);
		MatchTypeComboBox().SelectedIndex(std::clamp(matchType, 0, 3));
		CaseSensitiveToggle().IsOn(caseSensitive);
		EnabledToggle().IsOn(enabled);
		DescriptionTextBox().Text(description);
	}

	void FilterEditorDialog::ConfigureClientImport(hstring const& title, hstring const& hint, bool const replaceExisting, hstring const& primaryButtonText, hstring const& closeButtonText)
	{
		ShowPanel(ClientImportPanel());
		Title(box_value(title));
		PrimaryButtonText(primaryButtonText);
		CloseButtonText(closeButtonText);
		ImportHintText().Text(hint);
		ReplaceExistingCheckBox().IsChecked(replaceExisting);
	}

	void FilterEditorDialog::ConfigureSubscription(hstring const& title, hstring const& url, bool const enabled, hstring const& primaryButtonText, hstring const& closeButtonText)
	{
		ShowPanel(SubscriptionPanel());
		Title(box_value(title));
		PrimaryButtonText(primaryButtonText);
		CloseButtonText(closeButtonText);
		SubscriptionUrlTextBox().Text(url);
		SubscriptionEnabledCheckBox().IsChecked(enabled);
	}

	void FilterEditorDialog::ConfigureIpRule(hstring const& title, hstring const& entry, hstring const& description, hstring const& primaryButtonText, hstring const& closeButtonText)
	{
		ShowPanel(IpRulePanel());
		Title(box_value(title));
		PrimaryButtonText(primaryButtonText);
		CloseButtonText(closeButtonText);
		RuleEntryTextBox().Text(entry);
		RuleDescriptionTextBox().Text(description);
	}

	hstring FilterEditorDialog::Pattern()
	{
		return PatternTextBox().Text();
	}
	std::int32_t FilterEditorDialog::MatchType()
	{
		return MatchTypeComboBox().SelectedIndex();
	}
	bool FilterEditorDialog::CaseSensitive()
	{
		return CaseSensitiveToggle().IsOn();
	}
	bool FilterEditorDialog::Enabled()
	{
		if (SubscriptionPanel().Visibility() != Visibility::Visible) return EnabledToggle().IsOn();
		auto const value = SubscriptionEnabledCheckBox().IsChecked();
		return value && value.Value();
	}
	hstring FilterEditorDialog::Description()
	{
		return IpRulePanel().Visibility() == Visibility::Visible ? RuleDescriptionTextBox().Text() : DescriptionTextBox().Text();
	}
	bool FilterEditorDialog::ReplaceExisting()
	{
		auto const value = ReplaceExistingCheckBox().IsChecked();
		return value && value.Value();
	}
	hstring FilterEditorDialog::Url()
	{
		return SubscriptionUrlTextBox().Text();
	}
	hstring FilterEditorDialog::RuleEntry()
	{
		return RuleEntryTextBox().Text();
	}
}
