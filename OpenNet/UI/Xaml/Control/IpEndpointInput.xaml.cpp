#include "XamlWorkaround.h"
#include "IpEndpointInput.xaml.h"
#if __has_include("UI/Xaml/Control/IpEndpointInput.g.cpp")
#include "UI/Xaml/Control/IpEndpointInput.g.cpp"
#endif

import winrt.Windows.Networking;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Networking;

namespace
{
	winrt::hstring Trimmed(winrt::hstring const& value)
	{
		std::wstring_view view{ value.c_str(), value.size() };
		auto const first = view.find_first_not_of(L" \t\r\n");
		if (first == std::wstring_view::npos)
		{
			return {};
		}
		auto const last = view.find_last_not_of(L" \t\r\n");
		return winrt::hstring{ view.substr(first, last - first + 1) };
	}
}

namespace winrt::OpenNet::UI::Xaml::Control::implementation
{
	DependencyProperty IpEndpointInput::s_addressProperty =
		DependencyProperty::Register(
			L"Address",
			xaml_typename<hstring>(),
			xaml_typename<class_type>(),
			PropertyMetadata{
				box_value(hstring{ L"0.0.0.0" }),
				PropertyChangedCallback{ &IpEndpointInput::OnInputPropertyChanged } });

	DependencyProperty IpEndpointInput::s_portProperty =
		DependencyProperty::Register(
			L"Port",
			xaml_typename<std::uint32_t>(),
			xaml_typename<class_type>(),
			PropertyMetadata{
				box_value(static_cast<std::uint32_t>(0)),
				PropertyChangedCallback{ &IpEndpointInput::OnInputPropertyChanged } });

	DependencyProperty IpEndpointInput::s_allowAutomaticPortProperty =
		DependencyProperty::Register(
			L"AllowAutomaticPort",
			xaml_typename<bool>(),
			xaml_typename<class_type>(),
			PropertyMetadata{
				box_value(true),
				PropertyChangedCallback{ &IpEndpointInput::OnInputPropertyChanged } });

	DependencyProperty IpEndpointInput::s_requiredFamilyProperty =
		DependencyProperty::Register(
			L"RequiredFamily",
			xaml_typename<OpenNet::UI::Xaml::Control::IpAddressFamily>(),
			xaml_typename<class_type>(),
			PropertyMetadata{
				box_value(OpenNet::UI::Xaml::Control::IpAddressFamily::Any),
				PropertyChangedCallback{ &IpEndpointInput::OnInputPropertyChanged } });

	IpEndpointInput::IpEndpointInput()
	{
		InitializeComponent();
		m_initialized = true;
		SynchronizeEditors();
		UpdateValidation(false);
	}

	hstring IpEndpointInput::Address() const
	{
		return unbox_value_or<hstring>(GetValue(AddressProperty()), L"");
	}

	void IpEndpointInput::Address(hstring const& value)
	{
		SetValue(AddressProperty(), box_value(value));
	}

	DependencyProperty IpEndpointInput::AddressProperty()
	{
		return s_addressProperty;
	}

	std::uint32_t IpEndpointInput::Port() const
	{
		return unbox_value_or<std::uint32_t>(GetValue(PortProperty()), 0);
	}

	void IpEndpointInput::Port(std::uint32_t value)
	{
		SetValue(PortProperty(), box_value(value));
	}

	DependencyProperty IpEndpointInput::PortProperty()
	{
		return s_portProperty;
	}

	bool IpEndpointInput::AllowAutomaticPort() const
	{
		return unbox_value_or<bool>(
			GetValue(AllowAutomaticPortProperty()), true);
	}

	void IpEndpointInput::AllowAutomaticPort(bool value)
	{
		SetValue(AllowAutomaticPortProperty(), box_value(value));
	}

	DependencyProperty IpEndpointInput::AllowAutomaticPortProperty()
	{
		return s_allowAutomaticPortProperty;
	}

	OpenNet::UI::Xaml::Control::IpAddressFamily
		IpEndpointInput::RequiredFamily() const
	{
		return unbox_value_or(
			GetValue(RequiredFamilyProperty()),
			OpenNet::UI::Xaml::Control::IpAddressFamily::Any);
	}

	void IpEndpointInput::RequiredFamily(
		OpenNet::UI::Xaml::Control::IpAddressFamily value)
	{
		SetValue(RequiredFamilyProperty(), box_value(value));
	}

	DependencyProperty IpEndpointInput::RequiredFamilyProperty()
	{
		return s_requiredFamilyProperty;
	}

	bool IpEndpointInput::IsValid() const noexcept
	{
		return m_isValid;
	}

	OpenNet::UI::Xaml::Control::IpAddressFamily
		IpEndpointInput::AddressFamily() const noexcept
	{
		return m_addressFamily;
	}

	hstring IpEndpointInput::ValidationMessage() const
	{
		return m_validationMessage;
	}

	hstring IpEndpointInput::NormalizedEndpoint() const
	{
		return m_normalizedEndpoint;
	}

	bool IpEndpointInput::Validate()
	{
		UpdateValidation(false);
		return m_isValid;
	}

	event_token IpEndpointInput::ValueChanged(
		Windows::Foundation::EventHandler<
		Windows::Foundation::IInspectable> const& handler)
	{
		return m_valueChanged.add(handler);
	}

	void IpEndpointInput::ValueChanged(event_token const& token) noexcept
	{
		m_valueChanged.remove(token);
	}

	void IpEndpointInput::AddressTextBox_TextChanged(
		Windows::Foundation::IInspectable const&,
		TextChangedEventArgs const&)
	{
		if (m_synchronizing)
		{
			return;
		}
		Address(AddressTextBox().Text());
	}

	void IpEndpointInput::PortNumberBox_ValueChanged(
		NumberBox const&,
		NumberBoxValueChangedEventArgs const& args)
	{
		if (m_synchronizing)
		{
			return;
		}
		if (std::isnan(args.NewValue()) ||
			args.NewValue() < 0 ||
			args.NewValue() > 65535 ||
			std::floor(args.NewValue()) != args.NewValue())
		{
			m_portEditorValid = false;
			UpdateValidation(true);
			return;
		}

		m_portEditorValid = true;
		Port(static_cast<std::uint32_t>(args.NewValue()));
	}

	void IpEndpointInput::OnInputPropertyChanged(
		DependencyObject const& sender,
		DependencyPropertyChangedEventArgs const&)
	{
		if (auto control =
			sender.try_as<OpenNet::UI::Xaml::Control::IpEndpointInput>())
		{
			auto self = get_self<IpEndpointInput>(control);
			if (self->m_initialized && !self->m_synchronizing)
			{
				self->SynchronizeEditors();
				self->UpdateValidation(true);
			}
		}
	}

	void IpEndpointInput::SynchronizeEditors()
	{
		if (!m_initialized)
		{
			return;
		}

		m_synchronizing = true;
		AddressTextBox().Text(Address());
		PortNumberBox().Minimum(0.0);
		PortNumberBox().Value(static_cast<double>(Port()));
		m_portEditorValid = Port() <= 65535;
		m_synchronizing = false;
	}

	void IpEndpointInput::UpdateValidation(bool raiseEvent)
	{
		auto address = Trimmed(Address());
		if (address.size() >= 2 &&
			address.front() == L'[' &&
			address.back() == L']')
		{
			address = hstring{
				std::wstring_view{ address.c_str(), address.size() }
					.substr(1, address.size() - 2) };
		}

		m_isValid = false;
		m_addressFamily =
			OpenNet::UI::Xaml::Control::IpAddressFamily::Any;
		m_normalizedEndpoint = {};

		if (address.empty())
		{
			m_validationMessage = L"Enter an IPv4 or IPv6 address.";
		}
		else if (!m_portEditorValid || Port() > 65535)
		{
			m_validationMessage = L"Port must be between 0 and 65535.";
		}
		else if (!AllowAutomaticPort() && Port() == 0)
		{
			m_validationMessage = L"Port 0 is only valid in automatic mode.";
		}
		else
		{
			try
			{
				HostName host{ address };
				if (host.Type() == HostNameType::Ipv4)
				{
					m_addressFamily =
						OpenNet::UI::Xaml::Control::IpAddressFamily::IPv4;
				}
				else if (host.Type() == HostNameType::Ipv6)
				{
					m_addressFamily =
						OpenNet::UI::Xaml::Control::IpAddressFamily::IPv6;
				}
				else
				{
					m_validationMessage =
						L"Host names are not accepted; enter a numeric IP address.";
				}

				if (m_addressFamily !=
					OpenNet::UI::Xaml::Control::IpAddressFamily::Any)
				{
					auto const required = RequiredFamily();
					if (required !=
						OpenNet::UI::Xaml::Control::IpAddressFamily::Any &&
						required != m_addressFamily)
					{
						m_validationMessage =
							required ==
							OpenNet::UI::Xaml::Control::IpAddressFamily::IPv4
							? L"Enter an IPv4 address."
							: L"Enter an IPv6 address.";
					}
					else
					{
						m_isValid = true;
						std::wstring normalized;
						if (m_addressFamily ==
							OpenNet::UI::Xaml::Control::IpAddressFamily::IPv6)
						{
							normalized.push_back(L'[');
							normalized.append(address.c_str(), address.size());
							normalized.push_back(L']');
						}
						else
						{
							normalized.append(address.c_str(), address.size());
						}
						normalized.push_back(L':');
						auto const portText = to_hstring(Port());
						normalized.append(portText.c_str(), portText.size());
						m_normalizedEndpoint = hstring{ normalized };
						m_validationMessage =
							m_addressFamily ==
							OpenNet::UI::Xaml::Control::IpAddressFamily::IPv4
							? L"Valid IPv4 endpoint"
							: L"Valid IPv6 endpoint";
						if (Port() == 0)
						{
							std::wstring message{
								m_validationMessage.c_str(),
								m_validationMessage.size() };
							message.append(L" · automatic port");
							m_validationMessage = hstring{ message };
						}
					}
				}
			}
			catch (...)
			{
				m_validationMessage = L"Enter a valid numeric IPv4 or IPv6 address.";
			}
		}

		ValidPanel().Visibility(
			m_isValid ? Visibility::Visible : Visibility::Collapsed);
		InvalidPanel().Visibility(
			m_isValid ? Visibility::Collapsed : Visibility::Visible);
		ValidText().Text(m_validationMessage);
		InvalidText().Text(m_validationMessage);

		if (raiseEvent)
		{
			m_valueChanged(*this, nullptr);
		}
	}
}
