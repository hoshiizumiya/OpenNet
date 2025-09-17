#include "pch.h"
#include "NetworkSettingsViewModel.h"
#include "ViewModels/NetworkSettingsViewModel.g.cpp"

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
        , m_listenPort(10996)
        , m_firewallEnabled(false)
    {
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
        // TODO: 从本地源加载设置到成员变量
    }
}
