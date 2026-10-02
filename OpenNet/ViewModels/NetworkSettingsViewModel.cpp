#include "XamlWorkaround.h"
#include "NetworkSettingsViewModel.h"
#include "Core/PortCheckSettings.h"
#include "ViewModels/NetworkSettingsViewModel.g.cpp"

import OpenNet.Core.AppSettingsDatabase;

using namespace winrt;

namespace Models = winrt::OpenNet::Models::implementation; // IPProtocolPriority/ConnectionProtocol are here

namespace winrt::OpenNet::ViewModels::implementation
{
	// Summary: 构造函数，初始化默认设置
	NetworkSettingsViewModel::NetworkSettingsViewModel()
		: m_ipv4Enabled(true)
		, m_ipv6Enabled(true)
		, m_protocolPriority(Models::IPProtocolPriority::Auto)
		, m_preferredProtocol(Models::ConnectionProtocol::Auto)
		, m_encryptionEnabled(true)
		, m_listenPort(0)
		, m_firewallEnabled(false)
	{
		LoadSettings();
	}

	void NetworkSettingsViewModel::PortCheckIntervalMinutes(double value)
	{
		namespace settings = ::OpenNet::Core::PortCheckSettings;
		if (!std::isfinite(value))
		{
			RaisePropertyChanged(L"PortCheckIntervalMinutes");
			return;
		}
		auto const minutes = static_cast<std::int64_t>(std::clamp(std::floor(value),
			static_cast<double>(settings::MinimumIntervalMinutes), static_cast<double>(settings::MaximumIntervalMinutes)));
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		database.SetInt(settings::Category, settings::IntervalMinutesKey, minutes);
		SetProperty(m_portCheckIntervalMinutes, static_cast<double>(minutes), L"PortCheckIntervalMinutes");
		if (value != static_cast<double>(minutes)) RaisePropertyChanged(L"PortCheckIntervalMinutes");
	}

	// Summary: 初始化，加载设置
	void NetworkSettingsViewModel::Initialize()
	{
		LoadSettings();
	}

	// Summary: 保存当前设置
	void NetworkSettingsViewModel::SaveSettings()
	{
		// TODO: 将设置持久化到本地（文件/注册表/应用设置）
	}

	// Summary: 加载已保存的设置
	void NetworkSettingsViewModel::LoadSettings()
	{
		namespace settings = ::OpenNet::Core::PortCheckSettings;
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		auto const minutes = settings::NormalizeIntervalMinutes(database.GetInt(
			settings::Category, settings::IntervalMinutesKey).value_or(settings::DefaultIntervalMinutes));
		SetProperty(m_portCheckIntervalMinutes, static_cast<double>(minutes), L"PortCheckIntervalMinutes");
		// TODO: 从本地源加载设置到成员变量
	}
}
