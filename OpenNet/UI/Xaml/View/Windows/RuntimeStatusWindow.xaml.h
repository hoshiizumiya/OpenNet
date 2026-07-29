#pragma once

#include "UI/Xaml/View/Windows/RuntimeStatusWindow.g.h"

import winrt.Microsoft.UI.Dispatching;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct RuntimeStatusWindow : RuntimeStatusWindowT<RuntimeStatusWindow>
	{
		RuntimeStatusWindow();
		void InitializeComponent();
		void RefreshButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void CopyButton_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		struct StatusRow
		{
			std::wstring name;
			std::wstring value;
		};

		struct StatusSection
		{
			std::wstring title;
			std::vector<StatusRow> rows;
		};

		void RefreshReport();
		void RebuildStatusTree();
		winrt::hstring BuildReport();
		winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_refreshTimer{ nullptr };
		std::vector<StatusSection> m_statusSections;
		winrt::hstring m_lastReport;
		std::uint64_t m_previousKernelTime{};
		std::uint64_t m_previousUserTime{};
		std::uint64_t m_previousWallTime{};
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct RuntimeStatusWindow : RuntimeStatusWindowT<		RuntimeStatusWindow, implementation::RuntimeStatusWindow>
	{
	};
}
