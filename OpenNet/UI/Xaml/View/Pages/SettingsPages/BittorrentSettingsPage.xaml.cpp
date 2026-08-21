#include "LibtorrentIncludeGuard.h"
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/settings_pack.hpp>
#include "LibtorrentIncludeRestore.h"

#include "XamlWorkaround.h"
#include "BittorrentSettingsPage.xaml.h"
#include "SettingsPageTagRegister.h"
#if __has_include("UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.g.cpp")
#include "UI/Xaml/View/Pages/SettingsPages/BittorrentSettingsPage.g.cpp"
#endif

import OpenNet.Core.P2PManager;
import winrt.Microsoft.UI.Dispatching;
import winrtplus_coroutine;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;

namespace lt = libtorrent;

namespace winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::implementation
{
	static SettingsPageTagRegister<BittorrentSettingsPage> s_tags{
		L"bittorrent", L"SettingsBitTorrentSearchTags" };
	static SettingsPageTagRegister<BittorrentSettingsPage> s_networkTags{
		L"network", L"SettingsNetworkSearchTags" };
	static SettingsPageTagRegister<BittorrentSettingsPage> s_trackerTags{
		L"tracker", L"SettingsTrackerSearchTags" };
	BittorrentSettingsPage::BittorrentSettingsPage()
	{
		InitializeComponent();

		Loaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			LoadSettings();
		});

		// Save settings when the page is unloaded (navigated away).
		// This ensures TextBox changes that only use LostFocus are persisted
		// even if the user navigates away without clicking elsewhere first.
		Unloaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			if (!m_loading)
			{
				SaveAndApply();
			}
		});
	}

	winrt::fire_and_forget BittorrentSettingsPage::LoadSettings()
	{
		auto strong = get_strong();
		auto dispatcher = DispatcherQueue();

		// Heavy I/O + libtorrent calls on background thread
		co_await winrt::resume_background();

		auto& mgr = ::OpenNet::Core::TorrentSettingsManager::Instance();
		mgr.Load();
		auto s = mgr.Get();

		// Back to UI thread to populate controls
		co_await winrtplus::resume_foreground(dispatcher);

		m_loading = true;
		PopulateFromSettings(s);
		m_loading = false;
	}

	void BittorrentSettingsPage::PopulateFromSettings(::OpenNet::Core::TorrentSettings const& s)
	{
		// Connection
		ConnectionsLimitNumberBox().Value(s.connectionsLimit);
		EnableIncomingTcpToggle().IsOn(s.enableIncomingTcp);
		EnableOutgoingTcpToggle().IsOn(s.enableOutgoingTcp);
		EnableIncomingUtpToggle().IsOn(s.enableIncomingUtp);
		EnableOutgoingUtpToggle().IsOn(s.enableOutgoingUtp);
		AllowMultipleConnectionsPerIpToggle().IsOn(s.allowMultipleConnectionsPerIp);
		AnonymousModeToggle().IsOn(s.anonymousMode);

		// Discovery
		EnableDhtToggle().IsOn(s.enableDht);
		EnableLsdToggle().IsOn(s.enableLsd);
		EnableUpnpToggle().IsOn(s.enableUpnp);
		EnableNatpmpToggle().IsOn(s.enableNatpmp);
		ApplyIpFilterToDhtToggle().IsOn(s.applyIpFilterToDht);
		UpnpLeaseDurationNumberBox().Value(s.upnpLeaseDuration);
		NatPmpGatewayTextBox().Text(winrt::to_hstring(s.natPmpGateway));
		NatPmpLeaseDurationNumberBox().Value(s.natPmpLeaseDuration);

		// Tracker
		AnnounceToAllTiersToggle().IsOn(s.announceToAllTiers);
		AnnounceToAllTrackersToggle().IsOn(s.announceToAllTrackers);
		EnableWebTorrentToggle().IsOn(s.enableWebTorrent);
		WebTorrentStunServerTextBox().Text(winrt::to_hstring(s.webTorrentStunServer));
		MaxWebTorrentOffersNumberBox().Value(s.maxWebTorrentOffers);
		WebTorrentConnectionTimeoutNumberBox().Value(s.webTorrentConnectionTimeout);
		MinWebSocketAnnounceIntervalNumberBox().Value(s.minWebSocketAnnounceInterval);

		// Limits
		QueueingEnabledToggle().IsOn(s.queueingEnabled);
		ActiveDownloadsNumberBox().Value(s.activeDownloads);
		ActiveSeedsNumberBox().Value(s.activeSeeds);
		ActiveLimitNumberBox().Value(s.activeLimit);

		// Speed limits (stored as bytes/sec; display as KB/s)
		DownloadRateLimitNumberBox().Value(s.downloadRateLimit / 1024);
		UploadRateLimitNumberBox().Value(s.uploadRateLimit / 1024);

		// Seeding
		SeedRatioLimitNumberBox().Value(s.seedingRatioLimit);
		SeedTimeLimitNumberBox().Value(s.seedingTimeLimit);

		// Peer
		PeerTimeoutNumberBox().Value(s.peerTimeout);
		HandshakeTimeoutNumberBox().Value(s.handshakeTimeout);
		CloseRedundantConnectionsToggle().IsOn(s.closeRedundantConnections);

		// Disk I/O
		AioThreadsNumberBox().Value(s.aioThreads);
		CheckingMemUsageNumberBox().Value(s.checkingMemUsage);
		DisableV1HashesForHybridToggle().IsOn(s.disableV1HashesForHybrid);
		PartFileDirectoryTextBox().Text(winrt::to_hstring(s.partFileDirectory));
		MaxTorrentDirectoryDepthNumberBox().Value(s.maxTorrentDirectoryDepth);

		// Identity
		UserAgentTextBox().Text(winrt::to_hstring(s.userAgent));

		// Encryption
		EncryptionPolicyComboBox().SelectedIndex(static_cast<int>(s.encryptionPolicy));
		PreferRc4Toggle().IsOn(s.preferRc4);

		// Proxy
		ProxyTypeComboBox().SelectedIndex(static_cast<int>(s.proxyType));
		ProxyHostnameTextBox().Text(winrt::to_hstring(s.proxyHostname));
		ProxyPortNumberBox().Value(s.proxyPort);
		ProxyUsernameTextBox().Text(winrt::to_hstring(s.proxyUsername));
		ProxyPasswordBox().Password(winrt::to_hstring(s.proxyPassword));
		ProxyPeerConnectionsToggle().IsOn(s.proxyPeerConnections);
		ProxyTrackerConnectionsToggle().IsOn(s.proxyTrackerConnections);
		ProxySendHostInConnectToggle().IsOn(s.proxySendHostInConnect);
		EnableI2pToggle().IsOn(s.enableI2p);
		I2pHostnameTextBox().Text(winrt::to_hstring(s.i2pHostname));
		I2pPortNumberBox().Value(s.i2pPort);
		AllowI2pMixedToggle().IsOn(s.allowI2pMixed);
		I2pInboundQuantityNumberBox().Value(s.i2pInboundQuantity);
		I2pOutboundQuantityNumberBox().Value(s.i2pOutboundQuantity);
		I2pInboundLengthNumberBox().Value(s.i2pInboundLength);
		I2pOutboundLengthNumberBox().Value(s.i2pOutboundLength);
		I2pInboundVarianceNumberBox().Value(s.i2pInboundLengthVariance);
		I2pOutboundVarianceNumberBox().Value(s.i2pOutboundLengthVariance);

		// Advanced libtorrent
		MaxPeerListSizeNumberBox().Value(s.maxPeerListSize);
		PeerFingerprintTextBox().Text(winrt::to_hstring(s.peerFingerprint));
		HashingThreadsNumberBox().Value(s.hashingThreads);
		FilePoolSizeNumberBox().Value(s.filePoolSize);
		DiskQueueSizeNumberBox().Value(s.diskQueueSize / 1024);
		PieceExtentAffinityToggle().IsOn(s.pieceExtentAffinity);
		UploadSuggestionsToggle().IsOn(s.uploadSuggestions);
		SendBufferWatermarkNumberBox().Value(s.sendBufferWatermark);
		SendBufferLowWatermarkNumberBox().Value(s.sendBufferLowWatermark);
		SendBufferWatermarkFactorNumberBox().Value(s.sendBufferWatermarkFactor);
		ConnectionSpeedNumberBox().Value(s.connectionSpeed);
		SeedingOutgoingConnectionsToggle().IsOn(s.seedingOutgoingConnections);
		SocketSendBufferSizeNumberBox().Value(s.socketSendBufferSize / 1024);
		SocketReceiveBufferSizeNumberBox().Value(s.socketReceiveBufferSize / 1024);
		SocketBacklogSizeNumberBox().Value(s.socketBacklogSize);
		MixedModeComboBox().SelectedIndex(s.mixedModeAlgorithm);
		HostnameCacheTtlNumberBox().Value(s.hostnameCacheTtl);
		ValidateHttpsTrackersToggle().IsOn(s.validateHttpsTrackers);
		SsrfMitigationToggle().IsOn(s.ssrfMitigation);
		BlockPrivilegedPortsToggle().IsOn(s.blockPeersOnPrivilegedPorts);
		UploadSlotsBehaviorComboBox().SelectedIndex(s.uploadSlotsBehavior);
		UploadChokingAlgorithmComboBox().SelectedIndex(s.uploadChokingAlgorithm);
		UnchokeSlotsLimitNumberBox().Value(s.unchokeSlotsLimit);
		AlertQueueSizeNumberBox().Value(s.alertQueueSize);
		DhtBootstrapNodesTextBox().Text(winrt::to_hstring(s.dhtBootstrapNodes));
		AnnounceIpTextBox().Text(winrt::to_hstring(s.announceIp));
		AnnouncePortNumberBox().Value(s.announcePort);
		MaxConcurrentHttpAnnouncesNumberBox().Value(s.maxConcurrentHttpAnnounces);
		StopTrackerTimeoutNumberBox().Value(s.stopTrackerTimeout);
		PeerTurnoverNumberBox().Value(s.peerTurnover);
		PeerTurnoverCutoffNumberBox().Value(s.peerTurnoverCutoff);
		PeerTurnoverIntervalNumberBox().Value(s.peerTurnoverInterval);
		RequestQueueSizeNumberBox().Value(s.requestQueueSize);

		// Download Defaults
		DefaultSavePathTextBox().Text(s.defaultSavePath);
		PreallocateStorageToggle().IsOn(s.preallocateStorage);
		AutoStartDownloadsToggle().IsOn(s.autoStartDownloads);
		MoveCompletedToggle().IsOn(s.moveCompletedEnabled);
		MoveCompletedPathTextBox().Text(s.moveCompletedPath);
	}

	::OpenNet::Core::TorrentSettings BittorrentSettingsPage::CollectFromUI()
	{
		// Start from current persisted settings to preserve fields not on this page
		auto s = ::OpenNet::Core::TorrentSettingsManager::Instance().Get();

		// Connection
		s.connectionsLimit = static_cast<int>(ConnectionsLimitNumberBox().Value());
		s.enableIncomingTcp = EnableIncomingTcpToggle().IsOn();
		s.enableOutgoingTcp = EnableOutgoingTcpToggle().IsOn();
		s.enableIncomingUtp = EnableIncomingUtpToggle().IsOn();
		s.enableOutgoingUtp = EnableOutgoingUtpToggle().IsOn();
		s.allowMultipleConnectionsPerIp = AllowMultipleConnectionsPerIpToggle().IsOn();
		s.anonymousMode = AnonymousModeToggle().IsOn();

		// Discovery
		s.enableDht = EnableDhtToggle().IsOn();
		s.enableLsd = EnableLsdToggle().IsOn();
		s.enableUpnp = EnableUpnpToggle().IsOn();
		s.enableNatpmp = EnableNatpmpToggle().IsOn();
		s.applyIpFilterToDht = ApplyIpFilterToDhtToggle().IsOn();
		s.natPmpGateway = winrt::to_string(NatPmpGatewayTextBox().Text());
		s.natPmpLeaseDuration = static_cast<int>(NatPmpLeaseDurationNumberBox().Value());
		s.upnpLeaseDuration = static_cast<int>(
			UpnpLeaseDurationNumberBox().Value());

		// Tracker
		s.announceToAllTiers = AnnounceToAllTiersToggle().IsOn();
		s.announceToAllTrackers = AnnounceToAllTrackersToggle().IsOn();
		s.enableWebTorrent = EnableWebTorrentToggle().IsOn();
		s.webTorrentStunServer = winrt::to_string(WebTorrentStunServerTextBox().Text());
		s.maxWebTorrentOffers = static_cast<int>(MaxWebTorrentOffersNumberBox().Value());
		s.webTorrentConnectionTimeout = static_cast<int>(WebTorrentConnectionTimeoutNumberBox().Value());
		s.minWebSocketAnnounceInterval = static_cast<int>(MinWebSocketAnnounceIntervalNumberBox().Value());

		// Limits
		s.queueingEnabled = QueueingEnabledToggle().IsOn();
		s.activeDownloads = static_cast<int>(ActiveDownloadsNumberBox().Value());
		s.activeSeeds = static_cast<int>(ActiveSeedsNumberBox().Value());
		s.activeLimit = static_cast<int>(ActiveLimitNumberBox().Value());

		// Speed limits (UI in KB/s, store as bytes/sec)
		s.downloadRateLimit = static_cast<int>(DownloadRateLimitNumberBox().Value()) * 1024;
		s.uploadRateLimit = static_cast<int>(UploadRateLimitNumberBox().Value()) * 1024;

		// Seeding
		s.seedingRatioLimit = SeedRatioLimitNumberBox().Value();
		s.seedingTimeLimit = static_cast<int>(SeedTimeLimitNumberBox().Value());

		// Peer
		s.peerTimeout = static_cast<int>(PeerTimeoutNumberBox().Value());
		s.handshakeTimeout = static_cast<int>(HandshakeTimeoutNumberBox().Value());
		s.closeRedundantConnections = CloseRedundantConnectionsToggle().IsOn();

		// Disk I/O
		s.aioThreads = static_cast<int>(AioThreadsNumberBox().Value());
		s.checkingMemUsage = static_cast<int>(CheckingMemUsageNumberBox().Value());
		s.disableV1HashesForHybrid = DisableV1HashesForHybridToggle().IsOn();
		s.partFileDirectory = winrt::to_string(PartFileDirectoryTextBox().Text());
		s.maxTorrentDirectoryDepth = static_cast<int>(MaxTorrentDirectoryDepthNumberBox().Value());

		// Identity
		s.userAgent = winrt::to_string(UserAgentTextBox().Text());

		// Encryption
		s.encryptionPolicy = static_cast<::OpenNet::Core::EncryptionPolicy>(
			EncryptionPolicyComboBox().SelectedIndex());
		s.preferRc4 = PreferRc4Toggle().IsOn();

		// Proxy
		s.proxyType = static_cast<::OpenNet::Core::ProxyType>(
			ProxyTypeComboBox().SelectedIndex());
		s.proxyHostname = winrt::to_string(ProxyHostnameTextBox().Text());
		s.proxyPort = static_cast<int>(ProxyPortNumberBox().Value());
		s.proxyUsername = winrt::to_string(ProxyUsernameTextBox().Text());
		s.proxyPassword = winrt::to_string(ProxyPasswordBox().Password());
		s.proxyPeerConnections = ProxyPeerConnectionsToggle().IsOn();
		s.proxyTrackerConnections = ProxyTrackerConnectionsToggle().IsOn();
		s.proxySendHostInConnect = ProxySendHostInConnectToggle().IsOn();
		s.enableI2p = EnableI2pToggle().IsOn();
		s.i2pHostname = winrt::to_string(I2pHostnameTextBox().Text());
		s.i2pPort = static_cast<int>(I2pPortNumberBox().Value());
		s.allowI2pMixed = AllowI2pMixedToggle().IsOn();
		s.i2pInboundQuantity = static_cast<int>(I2pInboundQuantityNumberBox().Value());
		s.i2pOutboundQuantity = static_cast<int>(I2pOutboundQuantityNumberBox().Value());
		s.i2pInboundLength = static_cast<int>(I2pInboundLengthNumberBox().Value());
		s.i2pOutboundLength = static_cast<int>(I2pOutboundLengthNumberBox().Value());
		s.i2pInboundLengthVariance = static_cast<int>(I2pInboundVarianceNumberBox().Value());
		s.i2pOutboundLengthVariance = static_cast<int>(I2pOutboundVarianceNumberBox().Value());

		// Advanced libtorrent
		s.maxPeerListSize = static_cast<int>(MaxPeerListSizeNumberBox().Value());
		s.peerFingerprint = winrt::to_string(PeerFingerprintTextBox().Text());
		s.hashingThreads = static_cast<int>(HashingThreadsNumberBox().Value());
		s.filePoolSize = static_cast<int>(FilePoolSizeNumberBox().Value());
		s.diskQueueSize = static_cast<int>(DiskQueueSizeNumberBox().Value()) * 1024;
		s.pieceExtentAffinity = PieceExtentAffinityToggle().IsOn();
		s.uploadSuggestions = UploadSuggestionsToggle().IsOn();
		s.sendBufferWatermark = static_cast<int>(SendBufferWatermarkNumberBox().Value());
		s.sendBufferLowWatermark = static_cast<int>(SendBufferLowWatermarkNumberBox().Value());
		s.sendBufferWatermarkFactor = static_cast<int>(SendBufferWatermarkFactorNumberBox().Value());
		s.connectionSpeed = static_cast<int>(ConnectionSpeedNumberBox().Value());
		s.seedingOutgoingConnections = SeedingOutgoingConnectionsToggle().IsOn();
		s.socketSendBufferSize = static_cast<int>(SocketSendBufferSizeNumberBox().Value()) * 1024;
		s.socketReceiveBufferSize = static_cast<int>(SocketReceiveBufferSizeNumberBox().Value()) * 1024;
		s.socketBacklogSize = static_cast<int>(SocketBacklogSizeNumberBox().Value());
		s.mixedModeAlgorithm = std::max(0, MixedModeComboBox().SelectedIndex());
		s.hostnameCacheTtl = static_cast<int>(HostnameCacheTtlNumberBox().Value());
		s.validateHttpsTrackers = ValidateHttpsTrackersToggle().IsOn();
		s.ssrfMitigation = SsrfMitigationToggle().IsOn();
		s.blockPeersOnPrivilegedPorts = BlockPrivilegedPortsToggle().IsOn();
		s.uploadSlotsBehavior = std::max(0, UploadSlotsBehaviorComboBox().SelectedIndex());
		s.uploadChokingAlgorithm = std::max(0, UploadChokingAlgorithmComboBox().SelectedIndex());
		s.unchokeSlotsLimit = static_cast<int>(UnchokeSlotsLimitNumberBox().Value());
		s.alertQueueSize = static_cast<int>(AlertQueueSizeNumberBox().Value());
		s.dhtBootstrapNodes = winrt::to_string(DhtBootstrapNodesTextBox().Text());
		s.announceIp = winrt::to_string(AnnounceIpTextBox().Text());
		s.announcePort = static_cast<int>(AnnouncePortNumberBox().Value());
		s.maxConcurrentHttpAnnounces = static_cast<int>(MaxConcurrentHttpAnnouncesNumberBox().Value());
		s.stopTrackerTimeout = static_cast<int>(StopTrackerTimeoutNumberBox().Value());
		s.peerTurnover = static_cast<int>(PeerTurnoverNumberBox().Value());
		s.peerTurnoverCutoff = static_cast<int>(PeerTurnoverCutoffNumberBox().Value());
		s.peerTurnoverInterval = static_cast<int>(PeerTurnoverIntervalNumberBox().Value());
		s.requestQueueSize = static_cast<int>(RequestQueueSizeNumberBox().Value());

		// Download Defaults
		s.defaultSavePath = DefaultSavePathTextBox().Text();
		s.preallocateStorage = PreallocateStorageToggle().IsOn();
		s.autoStartDownloads = AutoStartDownloadsToggle().IsOn();
		s.moveCompletedEnabled = MoveCompletedToggle().IsOn();
		s.moveCompletedPath = MoveCompletedPathTextBox().Text();

		return s;
	}

	void BittorrentSettingsPage::SaveAndApply()
	{
		auto s = CollectFromUI();

		// Persist to JSON
		::OpenNet::Core::TorrentSettingsManager::Instance().Set(s);

		// Apply to live session
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (core && core->IsRunning())
		{
			core->ReloadSettings();
		}
	}

	void BittorrentSettingsPage::OnSettingChanged(IInspectable const&, IInspectable const&)
	{
		// ComboBox.SelectedIndex and other XAML properties may raise their
		// events from InitializeComponent before every named control has
		// been connected. CollectFromUI must only run for a fully loaded
		// page, otherwise a generated control accessor can still be null.
		if (m_loading || !IsLoaded())
			return;
		SaveAndApply();
	}

}
