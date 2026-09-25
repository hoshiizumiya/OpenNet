#include "XamlWorkaround.h"
#include "FilterEditorDialog.xaml.h"
#if __has_include("UI/Xaml/View/Dialog/FilterEditorDialog.g.cpp")
#include "UI/Xaml/View/Dialog/FilterEditorDialog.g.cpp"
#endif

import OpenNet.Helpers.ThemeHelper;
import winrt.WinUI.LiquidGlass;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	namespace
	{
		std::optional<bool> ToggleIsOn(IInspectable const& sender)
		{
			if (auto native = sender.try_as<Microsoft::UI::Xaml::Controls::ToggleSwitch>()) return native.IsOn();
			if (auto glass = sender.try_as<WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) return glass.IsOn();
			return std::nullopt;
		}

		void SetToggleIsOn(IInspectable const& target, bool value)
		{
			if (!target) return;
			if (auto native = target.try_as<Microsoft::UI::Xaml::Controls::ToggleSwitch>()) native.IsOn(value);
			if (auto glass = target.try_as<WinUI::LiquidGlass::LiquidGlassToggleSwitch>()) glass.IsOn(value);
		}
	}

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
		m_caseSensitive = caseSensitive;
		m_enabled = enabled;
		m_configuring = true;
		SetToggleIsOn(CaseSensitiveToggle(), caseSensitive);
		SetToggleIsOn(CaseSensitiveGlassToggle(), caseSensitive);
		SetToggleIsOn(EnabledToggle(), enabled);
		SetToggleIsOn(EnabledGlassToggle(), enabled);
		m_configuring = false;
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
		return m_caseSensitive;
	}
	bool FilterEditorDialog::Enabled()
	{
		if (SubscriptionPanel().Visibility() != Visibility::Visible) return m_enabled;
		auto const value = SubscriptionEnabledCheckBox().IsChecked();
		return value && value.Value();
	}
	void FilterEditorDialog::CaseSensitiveToggle_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		m_configuring = true;
		SetToggleIsOn(sender, m_caseSensitive);
		m_configuring = false;
	}
	void FilterEditorDialog::CaseSensitiveToggle_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_configuring) return;
		if (auto value = ToggleIsOn(sender))
		{
			m_caseSensitive = *value;
			m_configuring = true;
			SetToggleIsOn(CaseSensitiveToggle(), *value);
			SetToggleIsOn(CaseSensitiveGlassToggle(), *value);
			m_configuring = false;
		}
	}
	void FilterEditorDialog::EnabledToggle_Loaded(IInspectable const& sender, RoutedEventArgs const&)
	{
		m_configuring = true;
		SetToggleIsOn(sender, m_enabled);
		m_configuring = false;
	}
	void FilterEditorDialog::EnabledToggle_Changed(IInspectable const& sender, RoutedEventArgs const&)
	{
		if (m_configuring) return;
		if (auto value = ToggleIsOn(sender))
		{
			m_enabled = *value;
			m_configuring = true;
			SetToggleIsOn(EnabledToggle(), *value);
			SetToggleIsOn(EnabledGlassToggle(), *value);
			m_configuring = false;
		}
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
