#pragma once

#include "ViewModels/ObservableMixin.h"
#include "ViewModels/NetworkSettingsViewModel.g.h"
#include "../Models/NetworkInfo.h"

namespace winrt::OpenNet::ViewModels::implementation
{
	// 网络设置视图模型 / Network Settings View Model
	struct NetworkSettingsViewModel : NetworkSettingsViewModelT<NetworkSettingsViewModel>, ::OpenNet::ViewModels::ObservableMixin<NetworkSettingsViewModel>
	{
		NetworkSettingsViewModel();

		// IPv4/IPv6 配置 / IPv4/IPv6 Configuration
		bool IPv4Enabled() const { return m_ipv4Enabled; }
		void IPv4Enabled(bool value) { SetProperty(m_ipv4Enabled, value, L"IPv4Enabled"); }

		bool IPv6Enabled() const { return m_ipv6Enabled; }
		void IPv6Enabled(bool value) { SetProperty(m_ipv6Enabled, value, L"IPv6Enabled"); }

		// 协议配置 / Protocol Configuration
		winrt::OpenNet::Models::IPProtocolPriority ProtocolPriority() const { return m_protocolPriority; }
		void ProtocolPriority(winrt::OpenNet::Models::IPProtocolPriority value) { SetProperty(m_protocolPriority, value, L"ProtocolPriority"); }

		winrt::OpenNet::Models::ConnectionProtocol PreferredProtocol() const { return m_preferredProtocol; }
		void PreferredProtocol(winrt::OpenNet::Models::ConnectionProtocol value) { SetProperty(m_preferredProtocol, value, L"PreferredProtocol"); }

		// 加密设置 / Encryption Settings
		bool EncryptionEnabled() const { return m_encryptionEnabled; }
		void EncryptionEnabled(bool value) { SetProperty(m_encryptionEnabled, value, L"EncryptionEnabled"); }

		// 端口配置 / Port Configuration
		uint16_t ListenPort() const { return m_listenPort; }
		void ListenPort(uint16_t value) { SetProperty(m_listenPort, value, L"ListenPort"); }

		// 防火墙状态 / Firewall Status
		bool FirewallEnabled() const { return m_firewallEnabled; }

		// 基本方法 / Basic Methods
		void Initialize();
		void SaveSettings();
		void LoadSettings();

	private:
		bool m_ipv4Enabled{ true };
		bool m_ipv6Enabled{ true };
		winrt::OpenNet::Models::IPProtocolPriority m_protocolPriority{ winrt::OpenNet::Models::IPProtocolPriority::Auto };
		winrt::OpenNet::Models::ConnectionProtocol m_preferredProtocol{ winrt::OpenNet::Models::ConnectionProtocol::Auto };
		bool m_encryptionEnabled{ true };
		uint16_t m_listenPort{ 10996 };
		bool m_firewallEnabled{ false };
	};
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
	struct NetworkSettingsViewModel : NetworkSettingsViewModelT<NetworkSettingsViewModel, implementation::NetworkSettingsViewModel>
	{
	};
}
