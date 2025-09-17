#include "pch.h"
#include "SettingsViewModel.h"

#include <winrt/Windows.Data.Json.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

namespace winrt::OpenNet::ViewModels::implementation
{
    // Summary: 构造函数，初始化默认设置和集合
    SettingsViewModel::SettingsViewModel()
        : m_userName(L"Guest")
        , m_deviceName(L"This PC")
        , m_defaultSavePath(L"")
        , m_startWithWindows(false)
        , m_minimizeToTray(true)
        , m_showNotifications(true)
        , m_currentLanguage(Language::Auto)
        , m_listenPort(DEFAULT_LISTEN_PORT)
        , m_enableUPnP(true)
        , m_enableIPv6(true)
        , m_connectionTimeout(DEFAULT_CONNECTION_TIMEOUT_MS)
        , m_maxConcurrentConnections(DEFAULT_MAX_CONNECTIONS)
        , m_protocolPriority(Models::IPProtocolPriority::Auto)
        , m_enableIPv4(true)
        , m_preferredProtocol(Models::ConnectionProtocol::Auto)
        , m_enableBitTorrent(true)
        , m_enableDHT(true)
        , m_enableUTP(true)
        , m_enablePeerExchange(true)
        , m_enableLocalServiceDiscovery(true)
        , m_enableWebRTC(false)
        , m_enableICE(false)
        , m_enableDataChannels(false)
        , m_forceEncryption(false)
        , m_encryptionMethod(L"AES-256")
        , m_allowPlaintext(false)
        , m_minPort(49152)
        , m_maxPort(65535)
        , m_randomPort(true)
        , m_enableFirewallDetection(true)
        , m_autoConfigureFirewall(false)
        , m_enableProxyDetection(false)
        , m_enableVPNDetection(false)
        , m_networkDetectionTimeout(30)
        , m_portScanTimeout(5)
        , m_maxFileSize(DEFAULT_MAX_FILE_SIZE)
        , m_chunkSize(DEFAULT_CHUNK_SIZE)
        , m_bandwidthLimit(0)
        , m_enableCompression(true)
        , m_enableEncryption(true)
        , m_enableResume(true)
        , m_verifyIntegrity(true)
        , m_encryptionLevel(EncryptionLevel::Basic)
        , m_requireAuthentication(false)
        , m_allowAnonymousConnections(true)
        , m_shareDeviceInfo(false)
        , m_shareNetworkInfo(false)
        , m_allowRemoteAccess(false)
        , m_enableDebugLogging(false)
        , m_logRetentionDays(DEFAULT_LOG_RETENTION_DAYS)
        , m_enableTelemetry(false)
        , m_currentTheme(Theme::Auto)
        , m_hasUnsavedChanges(false)
    {
        m_availableLanguages = single_threaded_vector<hstring>();
        m_availableLanguages.Append(L"Auto");
        m_availableLanguages.Append(L"中文");
        m_availableLanguages.Append(L"English");

        m_customPorts = single_threaded_observable_vector<hstring>();
        m_blockedPorts = single_threaded_observable_vector<hstring>();
        // no local m_stunServers; use NetworkDetector via STUNServers()
        m_turnServers = single_threaded_observable_vector<hstring>();
        m_dhtBootstrapNodes = single_threaded_observable_vector<hstring>();
    }

    // Summary: 析构函数
    SettingsViewModel::~SettingsViewModel() = default;

    // Summary: 初始化（加载设置和初始化命令）
    IAsyncAction SettingsViewModel::InitializeAsync()
    {
        InitializeDefaultValues();
        InitializeCommands();
        co_await LoadSettingsAsync();
    }

    // Summary: 异步加载设置
    IAsyncAction SettingsViewModel::LoadSettingsAsync()
    {
        // TODO: 从存储加载设置
        co_return;
    }

    // Summary: 异步保存设置
    IAsyncAction SettingsViewModel::SaveSettingsAsync()
    {
        co_await SaveSettingsInternalAsync();
    }

    // Summary: 重置为默认设置
    IAsyncAction SettingsViewModel::ResetToDefaultsAsync()
    {
        co_await ResetSettingsInternalAsync();
    }

    // Summary: 内部保存实现（占位）
    IAsyncAction SettingsViewModel::SaveSettingsInternalAsync()
    {
        // TODO: 序列化设置写入文件
        m_hasUnsavedChanges = false;
        co_return;
    }

    // Summary: 内部重置实现（占位）
    IAsyncAction SettingsViewModel::ResetSettingsInternalAsync()
    {
        InitializeDefaultValues();
        m_hasUnsavedChanges = false;
        co_return;
    }

    // Summary: 选择保存路径（占位）
    IAsyncAction SettingsViewModel::SelectSavePathAsync() { co_return; }
    IAsyncAction SettingsViewModel::SelectLogPathAsync() { co_return; }
    IAsyncAction SettingsViewModel::ImportSettingsAsync() { co_return; }
    IAsyncAction SettingsViewModel::ExportSettingsAsync() { co_return; }
    IAsyncAction SettingsViewModel::AddSTUNServerAsync() { co_return; }
    IAsyncAction SettingsViewModel::RemoveSTUNServerAsync(Windows::Foundation::IInspectable const&) { co_return; }
    IAsyncAction SettingsViewModel::TestNetworkAsync() { co_return; }
    IAsyncAction SettingsViewModel::ClearLogsAsync() { co_return; }

    // Summary: 初始化命令（占位）
    void SettingsViewModel::InitializeCommands() { /* TODO: 创建 RelayCommand 并绑定 */ }

    // Summary: 更新命令可执行状态（占位）
    void SettingsViewModel::UpdateCommands() { /* TODO */ }

    // Summary: 初始化默认值
    void SettingsViewModel::InitializeDefaultValues() { /* 已在构造中设置 */ }

    // Summary: 设置变更时回调（占位）
    void SettingsViewModel::OnSettingChanged() { m_hasUnsavedChanges = true; }

    // Summary: 格式化字节数
    std::wstring SettingsViewModel::FormatBytes(uint64_t bytes) const
    {
        const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
        int unit = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && unit < 4) { size /= 1024.0; ++unit; }
        wchar_t buf[64]{};
        swprintf(buf, 64, unit == 0 ? L"%llu %s" : L"%.2f %s", unit == 0 ? bytes : size, units[unit]);
        return buf;
    }

    // Summary: 格式化速度（字节/秒 -> 文本）
    std::wstring SettingsViewModel::FormatSpeed(double bytesPerSecond) const
    {
        const wchar_t* units[] = { L"bps", L"Kbps", L"Mbps", L"Gbps" };
        int unit = 0;
        while (bytesPerSecond >= 1000.0 && unit < 3) { bytesPerSecond /= 1000.0; ++unit; }
        wchar_t buf[64]{};
        swprintf(buf, 64, L"%.2f %s", bytesPerSecond, units[unit]);
        return buf;
    }

    // Summary: 序列化设置（占位）
    Windows::Foundation::IAsyncOperation<Windows::Data::Json::JsonObject> SettingsViewModel::SerializeSettingsAsync()
    {
        co_return Windows::Data::Json::JsonObject{}; // TODO
    }

    // Summary: 反序列化设置（占位）
    Windows::Foundation::IAsyncAction SettingsViewModel::DeserializeSettingsAsync(Windows::Data::Json::JsonObject const&)
    {
        co_return; // TODO
    }
}
