#pragma once

#include "UI/Xaml/View/Dialog/FilterEditorDialog.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Dialog::implementation
{
	struct FilterEditorDialog : FilterEditorDialogT<FilterEditorDialog>
	{
		FilterEditorDialog();
		void ConfigureClientRule(winrt::hstring const& title, winrt::hstring const& pattern, std::int32_t matchType, bool caseSensitive, bool enabled, winrt::hstring const& description, winrt::hstring const& primaryButtonText, winrt::hstring const& closeButtonText);
		void ConfigureClientImport(winrt::hstring const& title, winrt::hstring const& hint, bool replaceExisting, winrt::hstring const& primaryButtonText, winrt::hstring const& closeButtonText);
		void ConfigureSubscription(winrt::hstring const& title, winrt::hstring const& url, bool enabled, winrt::hstring const& primaryButtonText, winrt::hstring const& closeButtonText);
		void ConfigureIpRule(winrt::hstring const& title, winrt::hstring const& entry, winrt::hstring const& description, winrt::hstring const& primaryButtonText, winrt::hstring const& closeButtonText);
		winrt::hstring Pattern();
		std::int32_t MatchType();
		bool CaseSensitive();
		bool Enabled();
		winrt::hstring Description();
		bool ReplaceExisting();
		winrt::hstring Url();
		winrt::hstring RuleEntry();

	private:
		void ShowPanel(winrt::Microsoft::UI::Xaml::FrameworkElement const& panel);
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Dialog::factory_implementation
{
	struct FilterEditorDialog : FilterEditorDialogT<FilterEditorDialog, implementation::FilterEditorDialog>
	{
	};
}
