#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/ClientFilterSettingsPage.g.h"
#include "Core/ClientFilter/ClientFilterManager.h"

import winrt.Microsoft.UI.Dispatching;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	struct ClientFilterSettingsPage :
		ClientFilterSettingsPageT<ClientFilterSettingsPage>
	{
		ClientFilterSettingsPage();

		void OnEnableToggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OnAddRuleClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OnTestClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OnRefreshClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OnRuleSearchTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);
		void OnRuleSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
		void OnToggleRuleClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::fire_and_forget OnEditRuleClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::fire_and_forget OnDeleteRuleClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::fire_and_forget OnImportClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::fire_and_forget OnExportClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::fire_and_forget OnClearRulesClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OnClearHistoryClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		winrt::fire_and_forget LoadState();
		winrt::fire_and_forget RefreshSnapshot();
		void RebuildRuleItems();
		void RebuildHistoryItems();
		void UpdateSelectionButtons();
		void ShowStatus(
			winrt::hstring const& message,
			winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity);
		std::optional<::OpenNet::Core::ClientFilterRule> SelectedRule();

		bool m_loading{};
		std::uint64_t m_refreshGeneration{};
		std::vector<::OpenNet::Core::ClientFilterRule> m_allRules;
		std::vector<::OpenNet::Core::ClientFilterRule> m_visibleRules;
		std::vector<::OpenNet::Core::ClientFilterHit> m_hits;
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_ruleItems{ nullptr };
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_hitItems{ nullptr };
		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct ClientFilterSettingsPage :
		ClientFilterSettingsPageT<ClientFilterSettingsPage,
		implementation::ClientFilterSettingsPage>
	{
	};
}
