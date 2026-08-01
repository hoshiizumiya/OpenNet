#pragma once

#include "UI/Xaml/Control/IpEndpointInput.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	struct IpEndpointInput : IpEndpointInputT<IpEndpointInput>
	{
		IpEndpointInput();

		winrt::hstring Address() const;
		void Address(winrt::hstring const& value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty AddressProperty();

		std::uint32_t Port() const;
		void Port(std::uint32_t value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty PortProperty();

		bool AllowAutomaticPort() const;
		void AllowAutomaticPort(bool value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty			AllowAutomaticPortProperty();

		OpenNet::UI::Xaml::Control::IpAddressFamily RequiredFamily() const;
		void RequiredFamily(
			OpenNet::UI::Xaml::Control::IpAddressFamily value);
		static winrt::Microsoft::UI::Xaml::DependencyProperty
			RequiredFamilyProperty();

		bool IsValid() const noexcept;
		OpenNet::UI::Xaml::Control::IpAddressFamily
			AddressFamily() const noexcept;
		winrt::hstring ValidationMessage() const;
		winrt::hstring NormalizedEndpoint() const;
		bool Validate();

		winrt::event_token ValueChanged(winrt::Windows::Foundation::EventHandler<			winrt::Windows::Foundation::IInspectable> const& handler);
		void ValueChanged(winrt::event_token const& token) noexcept;

		void AddressTextBox_TextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
		void PortNumberBox_ValueChanged(winrt::Microsoft::UI::Xaml::Controls::NumberBox const&, winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const&);

	private:
		static void OnInputPropertyChanged(winrt::Microsoft::UI::Xaml::DependencyObject const& sender, winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const&);
		void SynchronizeEditors();
		void UpdateValidation(bool raiseEvent);

		static winrt::Microsoft::UI::Xaml::DependencyProperty s_addressProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_portProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_allowAutomaticPortProperty;
		static winrt::Microsoft::UI::Xaml::DependencyProperty s_requiredFamilyProperty;

		winrt::event<winrt::Windows::Foundation::EventHandler<			winrt::Windows::Foundation::IInspectable>> m_valueChanged;
		winrt::hstring m_validationMessage;
		winrt::hstring m_normalizedEndpoint;
		OpenNet::UI::Xaml::Control::IpAddressFamily m_addressFamily{
			OpenNet::UI::Xaml::Control::IpAddressFamily::Any };
		bool m_isValid{};
		bool m_initialized{};
		bool m_synchronizing{};
		bool m_portEditorValid{ true };
	};
}

namespace winrt::OpenNet::UI::Xaml::Control::factory_implementation
{
	struct IpEndpointInput : IpEndpointInputT<IpEndpointInput, implementation::IpEndpointInput>
	{
	};
}
