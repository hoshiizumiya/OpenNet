#pragma once
#include "ViewModels/SettingsViewModel.g.h"

// Use lightweight mixin instead of deriving from a WinRT runtimeclass to avoid multiple inheritance conflicts
#include "ViewModels/ObservableMixin.h"
#include "Core/NetworkDetector.h"
#include "../Models/NetworkInfo.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Data.Json.h>
#include <string>
#include <vector>

namespace winrt::OpenNet::ViewModels::implementation
{
	// 设置视图模型 / Settings View Model
	// Inherit ObservableMixin to get SetProperty and property change support
	struct SettingsViewModel : SettingsViewModelT<SettingsViewModel>, ::OpenNet::ViewModels::ObservableMixin<SettingsViewModel>
	{
	public:
		SettingsViewModel();
		~SettingsViewModel();

		// Make base helper methods visible for unqualified lookup
		using ::OpenNet::ViewModels::ObservableMixin<SettingsViewModel>::SetProperty;
		using ::OpenNet::ViewModels::ObservableMixin<SettingsViewModel>::RaisePropertyChanged;

		// 常规设置 / General Settings
		std::wstring UserName() const
		{
			return m_userName;
		}
		void UserName(std::wstring_view value)
		{
			SetProperty(m_userName, std::wstring(value), L"UserName");
		}

		std::wstring DeviceName() const
		{
			return m_deviceName;
		}
		void DeviceName(std::wstring_view value)
		{
			SetProperty(m_deviceName, std::wstring(value), L"DeviceName");
		}

		std::wstring DefaultSavePath() const
		{
			return m_defaultSavePath;
		}
		void DefaultSavePath(std::wstring_view value)
		{
			SetProperty(m_defaultSavePath, std::wstring(value), L"DefaultSavePath");
		}

		bool StartWithWindows() const
		{
			return m_startWithWindows;
		}
		void StartWithWindows(bool value)
		{
			SetProperty(m_startWithWindows, value, L"StartWithWindows");
		}

		bool MinimizeToTray() const
		{
			return m_minimizeToTray;
		}
		void MinimizeToTray(bool value)
		{
			SetProperty(m_minimizeToTray, value, L"MinimizeToTray");
		}

		bool ShowNotifications() const
		{
			return m_showNotifications;
		}
		void ShowNotifications(bool value)
		{
			SetProperty(m_showNotifications, value, L"ShowNotifications");
		}

		// 语言设置 / Language Settings
		enum class Language
		{
			Auto,                          // 自动 / Auto
			Chinese,                       // 中文 / Chinese
			English                        // 英文 / English
		};

		Language CurrentLanguage() const
		{
			return m_currentLanguage;
		}
		void CurrentLanguage(Language value)
		{
			if (SetProperty(m_currentLanguage, value, L"CurrentLanguage"))
			{
				RaisePropertyChanged(L"CurrentLanguageText");
			}
		}

		std::wstring CurrentLanguageText() const; // implemented in cpp (localized)

		winrt::Windows::Foundation::Collections::IVector<winrt::hstring> AvailableLanguages() const
		{
			return m_availableLanguages;
		}

		// 网络设置 / Network Settings
		uint16_t ListenPort() const
		{
			return m_listenPort;
		}
		void ListenPort(uint16_t value)
		{
			SetProperty(m_listenPort, value, L"ListenPort");
		}

		bool EnableUPnP() const
		{
			return m_enableUPnP;
		}
		void EnableUPnP(bool value)
		{
			SetProperty(m_enableUPnP, value, L"EnableUPnP");
		}

		bool EnableIPv6() const
		{
			return m_enableIPv6;
		}
		void EnableIPv6(bool value)
		{
			SetProperty(m_enableIPv6, value, L"EnableIPv6");
		}

		uint32_t ConnectionTimeout() const
		{
			return m_connectionTimeout;
		}
		void ConnectionTimeout(uint32_t value)
		{
			SetProperty(m_connectionTimeout, value, L"ConnectionTimeout");
		}

		uint32_t MaxConcurrentConnections() const
		{
			return m_maxConcurrentConnections;
		}
		void MaxConcurrentConnections(uint32_t value)
		{
			SetProperty(m_maxConcurrentConnections, value, L"MaxConcurrentConnections");
		}

		// 新增的IPv4/IPv6和协议设置 / New IPv4/IPv6 and Protocol Settings
		Models::IPProtocolPriority ProtocolPriority() const
		{
			return m_protocolPriority;
		}
		void ProtocolPriority(Models::IPProtocolPriority value)
		{
			if (SetProperty(m_protocolPriority, value, L"ProtocolPriority"))
			{
				RaisePropertyChanged(L"ProtocolPriorityText");
				OnSettingChanged();
			}
		}

		std::wstring ProtocolPriorityText() const; // implemented in cpp (localized)

		bool EnableIPv4() const
		{
			return m_enableIPv4;
		}
		void EnableIPv4(bool value)
		{
			SetProperty(m_enableIPv4, value, L"EnableIPv4"); OnSettingChanged();
		}

		// 连接协议配置 / Connection Protocol Configuration
		Models::ConnectionProtocol PreferredProtocol() const
		{
			return m_preferredProtocol;
		}
		void PreferredProtocol(Models::ConnectionProtocol value)
		{
			if (SetProperty(m_preferredProtocol, value, L"PreferredProtocol"))
			{
				RaisePropertyChanged(L"PreferredProtocolText");
				OnSettingChanged();
			}
		}

		std::wstring PreferredProtocolText() const; // implemented in cpp (localized)

		// STUN服务器设置 / STUN Server Settings
		winrt::Windows::Foundation::Collections::IVector<winrt::hstring> STUNServers() const
		{
			return m_networkDetector.GetRecommendedSTUNServers();
		}

		std::wstring CustomSTUNServer() const
		{
			return m_customSTUNServer;
		}
		void CustomSTUNServer(std::wstring_view value)
		{
			SetProperty(m_customSTUNServer, std::wstring(value), L"CustomSTUNServer");
		}

		// 传输设置 / Transfer Settings
		uint64_t MaxFileSize() const
		{
			return m_maxFileSize;
		}
		void MaxFileSize(uint64_t value)
		{
			if (SetProperty(m_maxFileSize, value, L"MaxFileSize"))
			{
				RaisePropertyChanged(L"MaxFileSizeText");
			}
		}

		std::wstring MaxFileSizeText() const
		{
			return FormatBytes(m_maxFileSize);
		}

		uint32_t ChunkSize() const
		{
			return m_chunkSize;
		}
		void ChunkSize(uint32_t value)
		{
			if (SetProperty(m_chunkSize, value, L"ChunkSize"))
			{
				RaisePropertyChanged(L"ChunkSizeText");
			}
		}

		std::wstring ChunkSizeText() const
		{
			return FormatBytes(m_chunkSize);
		}

		uint64_t BandwidthLimit() const
		{
			return m_bandwidthLimit;
		}
		void BandwidthLimit(uint64_t value)
		{
			if (SetProperty(m_bandwidthLimit, value, L"BandwidthLimit"))
			{
				RaisePropertyChanged(L"BandwidthLimitText");
			}
		}

		std::wstring BandwidthLimitText() const
		{
			return m_bandwidthLimit > 0 ? FormatSpeed(static_cast<double>(m_bandwidthLimit)) : L"无限制 / Unlimited";
		}

		bool EnableCompression() const
		{
			return m_enableCompression;
		}
		void EnableCompression(bool value)
		{
			SetProperty(m_enableCompression, value, L"EnableCompression");
		}

		bool EnableEncryption() const
		{
			return m_enableEncryption;
		}
		void EnableEncryption(bool value)
		{
			SetProperty(m_enableEncryption, value, L"EnableEncryption");
		}

		bool EnableResume() const
		{
			return m_enableResume;
		}
		void EnableResume(bool value)
		{
			SetProperty(m_enableResume, value, L"EnableResume");
		}

		bool VerifyIntegrity() const
		{
			return m_verifyIntegrity;
		}
		void VerifyIntegrity(bool value)
		{
			SetProperty(m_verifyIntegrity, value, L"VerifyIntegrity");
		}

		// 安全设置 / Security Settings
		enum class EncryptionLevel
		{
			None,                          // 无加密 / No Encryption
			Basic,                         // 基础加密 / Basic Encryption
			Strong                         // 强加密 / Strong Encryption
		};

		EncryptionLevel CurrentEncryptionLevel() const
		{
			return m_encryptionLevel;
		}
		void CurrentEncryptionLevel(EncryptionLevel value)
		{
			if (SetProperty(m_encryptionLevel, value, L"CurrentEncryptionLevel"))
			{
				RaisePropertyChanged(L"EncryptionLevelText");
			}
		}

		std::wstring EncryptionLevelText() const; // implemented in cpp (localized)

		bool RequireAuthentication() const
		{
			return m_requireAuthentication;
		}
		void RequireAuthentication(bool value)
		{
			SetProperty(m_requireAuthentication, value, L"RequireAuthentication");
		}

		bool AllowAnonymousConnections() const
		{
			return m_allowAnonymousConnections;
		}
		void AllowAnonymousConnections(bool value)
		{
			SetProperty(m_allowAnonymousConnections, value, L"AllowAnonymousConnections");
		}

		// 隐私设置 / Privacy Settings
		bool ShareDeviceInfo() const
		{
			return m_shareDeviceInfo;
		}
		void ShareDeviceInfo(bool value)
		{
			SetProperty(m_shareDeviceInfo, value, L"ShareDeviceInfo");
		}

		bool ShareNetworkInfo() const
		{
			return m_shareNetworkInfo;
		}
		void ShareNetworkInfo(bool value)
		{
			SetProperty(m_shareNetworkInfo, value, L"ShareNetworkInfo");
		}

		bool AllowRemoteAccess() const
		{
			return m_allowRemoteAccess;
		}
		void AllowRemoteAccess(bool value)
		{
			SetProperty(m_allowRemoteAccess, value, L"AllowRemoteAccess");
		}

		// 高级设置 / Advanced Settings
		bool EnableDebugLogging() const
		{
			return m_enableDebugLogging;
		}
		void EnableDebugLogging(bool value)
		{
			SetProperty(m_enableDebugLogging, value, L"EnableDebugLogging");
		}

		uint32_t LogRetentionDays() const
		{
			return m_logRetentionDays;
		}
		void LogRetentionDays(uint32_t value)
		{
			SetProperty(m_logRetentionDays, value, L"LogRetentionDays");
		}

		std::wstring LogPath() const
		{
			return m_logPath;
		}
		void LogPath(std::wstring_view value)
		{
			SetProperty(m_logPath, std::wstring(value), L"LogPath");
		}

		bool EnableTelemetry() const
		{
			return m_enableTelemetry;
		}
		void EnableTelemetry(bool value)
		{
			SetProperty(m_enableTelemetry, value, L"EnableTelemetry");
		}

		// 设置状态 / Settings Status
		bool HasUnsavedChanges() const
		{
			return m_hasUnsavedChanges;
		}
		void HasUnsavedChanges(bool value)
		{
			if (SetProperty(m_hasUnsavedChanges, value, L"HasUnsavedChanges"))
			{
				UpdateCommands();
			}
		}

		std::wstring LastSaved() const
		{
			return m_lastSaved;
		}
		void LastSaved(std::wstring_view value)
		{
			SetProperty(m_lastSaved, std::wstring(value), L"LastSaved");
		}

		// 命令 / Commands (getters)
		winrt::Microsoft::UI::Xaml::Input::ICommand SaveSettingsCommand() const
		{
			return m_saveSettingsCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand ResetSettingsCommand() const
		{
			return m_resetSettingsCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand ImportSettingsCommand() const
		{
			return m_importSettingsCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand ExportSettingsCommand() const
		{
			return m_exportSettingsCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand SelectSavePathCommand() const
		{
			return m_selectSavePathCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand SelectLogPathCommand() const
		{
			return m_selectLogPathCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand AddSTUNServerCommand() const
		{
			return m_addSTUNServerCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand RemoveSTUNServerCommand() const
		{
			return m_removeSTUNServerCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand TestNetworkCommand() const
		{
			return m_testNetworkCommand;
		}
		winrt::Microsoft::UI::Xaml::Input::ICommand ClearLogsCommand() const
		{
			return m_clearLogsCommand;
		}

		// 公共方法 / Public Methods
		winrt::Windows::Foundation::IAsyncAction InitializeAsync();
		winrt::Windows::Foundation::IAsyncAction LoadSettingsAsync();
		winrt::Windows::Foundation::IAsyncAction SaveSettingsAsync();
		winrt::Windows::Foundation::IAsyncAction ResetToDefaultsAsync();

	private:
		// 命令实现 / Command Implementations
		winrt::Windows::Foundation::IAsyncAction SaveSettingsInternalAsync();
		winrt::Windows::Foundation::IAsyncAction ResetSettingsInternalAsync();
		winrt::Windows::Foundation::IAsyncAction ImportSettingsAsync();
		winrt::Windows::Foundation::IAsyncAction ExportSettingsAsync();
		winrt::Windows::Foundation::IAsyncAction SelectSavePathAsync();
		winrt::Windows::Foundation::IAsyncAction SelectLogPathAsync();
		winrt::Windows::Foundation::IAsyncAction AddSTUNServerAsync();
		winrt::Windows::Foundation::IAsyncAction RemoveSTUNServerAsync(winrt::Windows::Foundation::IInspectable const& parameter);
		winrt::Windows::Foundation::IAsyncAction TestNetworkAsync();
		winrt::Windows::Foundation::IAsyncAction ClearLogsAsync();

		// 私有方法 / Private Methods
		void InitializeCommands();
		void UpdateCommands();
		void InitializeDefaultValues();
		void OnSettingChanged();
		std::wstring FormatBytes(uint64_t bytes) const;
		std::wstring FormatSpeed(double bytesPerSecond) const;

		// 设置序列化 / Settings Serialization
		winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Data::Json::JsonObject> SerializeSettingsAsync();
		winrt::Windows::Foundation::IAsyncAction DeserializeSettingsAsync(winrt::Windows::Data::Json::JsonObject const& settings);

	private:
		// 常规设置 / General Settings
		std::wstring m_userName;
		std::wstring m_deviceName;
		std::wstring m_defaultSavePath;
		bool m_startWithWindows;
		bool m_minimizeToTray;
		bool m_showNotifications;

		// 语言设置 / Language Settings
		Language m_currentLanguage;
		winrt::Windows::Foundation::Collections::IVector<winrt::hstring> m_availableLanguages;

		// 网络设置 / Network Settings
		uint16_t m_listenPort;
		bool m_enableUPnP;
		bool m_enableIPv6;
		uint32_t m_connectionTimeout;
		uint32_t m_maxConcurrentConnections;

		// 新增的IPv4/IPv6和协议设置 / New IPv4/IPv6 and Protocol Settings
		Models::IPProtocolPriority m_protocolPriority;
		bool m_enableIPv4;
		Models::ConnectionProtocol m_preferredProtocol;

		// BitTorrent协议设置 / BitTorrent Protocol Settings
		bool m_enableBitTorrent;
		bool m_enableDHT;
		bool m_enableUTP;
		bool m_enablePeerExchange;
		bool m_enableLocalServiceDiscovery;

		// WebRTC协议设置 / WebRTC Protocol Settings
		bool m_enableWebRTC;
		bool m_enableICE;
		bool m_enableDataChannels;

		// 增强的加密设置 / Enhanced Encryption Settings
		bool m_forceEncryption;
		std::wstring m_encryptionMethod;
		bool m_allowPlaintext;

		// 增强的端口设置 / Enhanced Port Settings
		uint16_t m_minPort;
		uint16_t m_maxPort;
		bool m_randomPort;
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_customPorts;
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_blockedPorts;

		// 防火墙设置 / Firewall Settings
		bool m_enableFirewallDetection;
		bool m_autoConfigureFirewall;

		// 扩展的服务器设置 / Extended Server Settings
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_turnServers;
		std::wstring m_customTURNServer;
		winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> m_dhtBootstrapNodes;
		std::wstring m_customDHTNode;

		// STUN服务器设置 / STUN Server storage (custom entry only)
		std::wstring m_customSTUNServer;

		// 依赖组件 / Dependencies
		::OpenNet::Core::NetworkDetector m_networkDetector;

		// 高级网络设置 / Advanced Network Settings
		bool m_enableProxyDetection;
		bool m_enableVPNDetection;
		uint32_t m_networkDetectionTimeout;
		uint32_t m_portScanTimeout;

		// 传输设置 / Transfer Settings
		uint64_t m_maxFileSize;
		uint32_t m_chunkSize;
		uint64_t m_bandwidthLimit;
		bool m_enableCompression;
		bool m_enableEncryption;
		bool m_enableResume;
		bool m_verifyIntegrity;

		// 安全设置 / Security Settings
		EncryptionLevel m_encryptionLevel;
		bool m_requireAuthentication;
		bool m_allowAnonymousConnections;

		// 隐私设置 / Privacy Settings
		bool m_shareDeviceInfo;
		bool m_shareNetworkInfo;
		bool m_allowRemoteAccess;

		// 高级设置 / Advanced Settings
		bool m_enableDebugLogging;
		uint32_t m_logRetentionDays;
		std::wstring m_logPath;
		bool m_enableTelemetry;

		// 设置状态 / Settings Status
		bool m_hasUnsavedChanges;
		std::wstring m_lastSaved;

		// 命令 / Commands (backing fields)
		winrt::Microsoft::UI::Xaml::Input::ICommand m_saveSettingsCommand{};
		winrt::Microsoft::UI::Xaml::Input::ICommand m_resetSettingsCommand{};
		winrt::Microsoft::UI::Xaml::Input::ICommand m_importSettingsCommand{};
		winrt::Microsoft::UI::Xaml::Input::ICommand m_exportSettingsCommand{};
		winrt::Microsoft::UI::Xaml::Input::ICommand m_selectSavePathCommand{};
		winrt::Microsoft::UI::Xaml::Input::ICommand m_selectLogPathCommand{};
		winrt::Microsoft::UI::Xaml::Input::ICommand m_addSTUNServerCommand{};
		winrt::Microsoft::UI::Xaml::Input::ICommand m_removeSTUNServerCommand{};
		winrt::Microsoft::UI::Xaml::Input::ICommand m_testNetworkCommand{};
		winrt::Microsoft::UI::Xaml::Input::ICommand m_clearLogsCommand{};

		// 常量 / Constants
		static constexpr uint16_t DEFAULT_LISTEN_PORT = 8888;
		static constexpr uint32_t DEFAULT_CONNECTION_TIMEOUT_MS = 30000;
		static constexpr uint32_t DEFAULT_MAX_CONNECTIONS = 10;
		static constexpr uint64_t DEFAULT_MAX_FILE_SIZE = 10ULL * 1024 * 1024 * 1024; // 10GB
		static constexpr uint32_t DEFAULT_CHUNK_SIZE = 1024 * 1024; // 1MB
		static constexpr uint32_t DEFAULT_LOG_RETENTION_DAYS = 30;
	};
}