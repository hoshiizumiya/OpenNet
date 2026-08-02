#pragma once

#include "UI/Xaml/View/Pages/SettingsPages/IPFilterSettingsPage.g.h"
#include "Core/IPFilter/IPFilterManager.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	struct IPFilterSettingsPage : IPFilterSettingsPageT<IPFilterSettingsPage>
	{
		IPFilterSettingsPage();

		// Used by the application-level scheduler as well as the settings page.
		static winrt::Windows::Foundation::IAsyncAction	RunSubscriptionUpdateAsync(bool force, bool notify);

		void OnEnableToggled(winrt::Windows::Foundation::IInspectable const& sender,
							 winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		void OnAddRuleClick(winrt::Windows::Foundation::IInspectable const& sender,
							winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		winrt::fire_and_forget OnImportClick(winrt::Windows::Foundation::IInspectable const& sender,
											 winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		winrt::fire_and_forget OnClearAllClick(winrt::Windows::Foundation::IInspectable const& sender,
											   winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		winrt::fire_and_forget OnOpenRulesFolderClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		winrt::fire_and_forget OnOpenRuleFileClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		void OnRefreshRulesClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		void OnRuleSearchTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);

		void OnRuleSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

		winrt::fire_and_forget OnEditRuleClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		winrt::fire_and_forget OnDeleteRuleClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

		void OnSubscriptionAutoUpdateToggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OnSubscriptionModeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OnSubscriptionIntervalChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);
		void OnAddSubscriptionClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OnSubscriptionSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
		winrt::fire_and_forget OnEditSubscriptionClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::fire_and_forget OnDeleteSubscriptionClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		winrt::fire_and_forget OnUpdateSubscriptionsClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		winrt::fire_and_forget LoadState();
		winrt::fire_and_forget RefreshRules();
		void RebuildRuleItems();
		void LoadSubscriptionState();
		void RebuildSubscriptionItems();
		void SetSubscriptionBusy(bool value);
		void ShowStatus(winrt::hstring const& message, winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity severity);
		std::optional<::OpenNet::Core::IPRule> SelectedRule();
		std::optional<::OpenNet::Core::IPFilterSubscription> SelectedSubscription();

		bool m_loading{ false };
		std::uint64_t m_rulesRefreshGeneration{};
		std::vector<::OpenNet::Core::IPRule> m_allRules;
		std::vector<::OpenNet::Core::IPRule> m_visibleRules;
		winrt::Windows::Foundation::Collections::IObservableVector<
			winrt::Windows::Foundation::IInspectable> m_ruleItems{ nullptr };
		std::vector<::OpenNet::Core::IPFilterSubscription> m_subscriptions;
		winrt::Windows::Foundation::Collections::IObservableVector<
			winrt::Windows::Foundation::IInspectable> m_subscriptionItems{ nullptr };
		static inline std::atomic_bool s_subscriptionUpdateRunning{ false };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::factory_implementation
{
	struct IPFilterSettingsPage : IPFilterSettingsPageT<IPFilterSettingsPage, implementation::IPFilterSettingsPage>
	{
	};
}
