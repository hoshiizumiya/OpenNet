#pragma once

#include "Service/Notification/InfoBarOptions.g.h"

import OpenNet.ViewModels.ObservableMixin;

namespace winrt::OpenNet::Service::Notification::implementation
{
	struct InfoBarOptions : InfoBarOptionsT<InfoBarOptions>, ::OpenNet::ViewModels::ObservableMixin<InfoBarOptions>
	{
		InfoBarOptions() = default;

		// Make mixin helpers visible
		using ::OpenNet::ViewModels::ObservableMixin<InfoBarOptions>::SetProperty;
		using ::OpenNet::ViewModels::ObservableMixin<InfoBarOptions>::RaisePropertyChanged;

		winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity Severity() const noexcept;
		void Severity(winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity value) noexcept;
		winrt::hstring Title() const;
		void Title(winrt::hstring const& value);
		winrt::hstring Message() const;
		void Message(winrt::hstring const& value);
		winrt::Windows::Foundation::IInspectable Content() const;
		void Content(winrt::Windows::Foundation::IInspectable const& value);
		winrt::Windows::Foundation::IInspectable ActionButtonContent() const;
		void ActionButtonContent(winrt::Windows::Foundation::IInspectable const& value);
		winrt::Microsoft::UI::Xaml::Input::ICommand ActionButtonCommand() const;
		void ActionButtonCommand(winrt::Microsoft::UI::Xaml::Input::ICommand const& value);
		std::uint32_t MilliSecondsDelay() const noexcept;
		void MilliSecondsDelay(std::uint32_t value) noexcept;
		bool IsTextSelectionEnabled() const noexcept;

	private:
		winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity m_severity{ winrt::Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational };
		winrt::hstring m_title;
		winrt::hstring m_message;
		winrt::Windows::Foundation::IInspectable m_content{ nullptr };
		winrt::Windows::Foundation::IInspectable m_actionButtonContent{ nullptr };
		winrt::Microsoft::UI::Xaml::Input::ICommand m_actionButtonCommand{ nullptr };
		std::uint32_t m_milliSecondsDelay{ 6000 };
	};
}

namespace winrt::OpenNet::Service::Notification::factory_implementation
{
	struct InfoBarOptions :	InfoBarOptionsT<InfoBarOptions, implementation::InfoBarOptions>
	{
	};
}
