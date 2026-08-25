/*
 * PROJECT:   OpenNet
 * FILE:      Core/TorrentSettings.ixx
 * PURPOSE:   Persistent settings for libtorrent session & download behaviour.
 *            Serialized as JSON to LocalFolder/torrent_settings.json.
 *
 * LICENSE:   The MIT License
 */
export module OpenNet.Core.TorrentSettings;
import std;

export namespace OpenNet::Core
{
	// ---------------------------------------------------------------
	//  Proxy type (maps to libtorrent proxy_type_t)
	// ---------------------------------------------------------------
	enum class ProxyType
	{
		None = 0,       // no proxy
		Socks4,         // SOCKS4
		Socks5,         // SOCKS5 (no auth)
		Socks5Password, // SOCKS5 with user/pass
		Http,           // HTTP CONNECT
		HttpPassword,   // HTTP CONNECT with user/pass
		I2pProxy,       // I2P SAM proxy
	};

	// ---------------------------------------------------------------
	//  Encryption policy (maps to libtorrent pe_settings)
	// ---------------------------------------------------------------
	enum class EncryptionPolicy
	{
		Forced = 0,   // Only encrypted connections
		Enabled = 1,  // Prefer encrypted, allow plaintext
		Disabled = 2, // No encryption
	};

	// ---------------------------------------------------------------
	//  TorrentSettings  – all persistent BT/download settings
	// ---------------------------------------------------------------
	struct TorrentSettings
	{
		// ----- Connection -----
		// Port 0 lets the OS choose an actually available port. Fixed ports entered
		// by the user are not silently replaced when binding fails.
		std::string listenInterfaces{ "0.0.0.0:0,[::]:0" };
		int connectionsLimit{ 9999 };
		bool enableIncomingTcp{ true };
		bool enableOutgoingTcp{ true };
		bool enableIncomingUtp{ true };
		bool enableOutgoingUtp{ true };
		bool allowMultipleConnectionsPerIp{ false };
		bool anonymousMode{ false };

		// ----- Discovery -----
		bool enableDht{ true };
		bool enableLsd{ true };
		bool enableUpnp{ true };
		bool enableNatpmp{ true };
		bool applyIpFilterToDht{ true };
		std::string natPmpGateway;
		int natPmpLeaseDuration{ 3600 };
		// 0 requests a permanent UPnP lease. Besides matching qBittorrent's
		// conservative default, this avoids an expired-timer retry storm while a
		// slow router is still answering a lease refresh request.
		int upnpLeaseDuration{ 0 };

		// ----- Tracker -----
		bool announceToAllTiers{ true };
		bool announceToAllTrackers{ false };
		std::string dhtBootstrapNodes{
			"dht.libtorrent.org:25401,"
			"dht.transmissionbt.com:6881,"
			"router.bt.ouinet.work:6881" };
		std::string announceIp;
		int announcePort{ 0 };
		int maxConcurrentHttpAnnounces{ 50 };
		int stopTrackerTimeout{ 5 };
		bool enableWebTorrent{ true };
		std::string webTorrentStunServer{ "stun.l.google.com:19302" };
		int minWebSocketAnnounceInterval{ 60 };
		int webTorrentConnectionTimeout{ 120 };
		int maxWebTorrentOffers{ 10 };

		// ----- Limits -----
		bool queueingEnabled{ false };
		int activeDownloads{ 20 };
		int activeSeeds{ 20 };
		int activeLimit{ 50 };

		// ----- Speed Limits (bytes/sec, 0 = unlimited) -----
		int downloadRateLimit{ 0 };
		int uploadRateLimit{ 0 };

		// ----- Seeding -----
		//  seedingRatioLimit: stop seeding when ratio >= this (0 = unlimited)
		//  seedingTimeLimit:  stop seeding after N minutes (0 = unlimited)
		double seedingRatioLimit{ 0.0 };
		int seedingTimeLimit{ 0 };

		// ----- Peer -----
		int peerTimeout{ 120 };
		int handshakeTimeout{ 10 };
		bool closeRedundantConnections{ true };
		int maxPeerListSize{ 4000 };
		int connectionSpeed{ 30 };
		bool seedingOutgoingConnections{ true };
		int socketSendBufferSize{ 0 };
		int socketReceiveBufferSize{ 0 };
		int socketBacklogSize{ 30 };
		int mixedModeAlgorithm{ 0 };
		int hostnameCacheTtl{ 3600 };
		bool validateHttpsTrackers{ true };
		bool ssrfMitigation{ true };
		bool blockPeersOnPrivilegedPorts{ false };
		int peerTurnover{ 4 };
		int peerTurnoverCutoff{ 90 };
		int peerTurnoverInterval{ 300 };
		int requestQueueSize{ 500 };
		int uploadSlotsBehavior{ 0 };
		int uploadChokingAlgorithm{ 1 };
		int unchokeSlotsLimit{ 20 };
		int alertQueueSize{ 1000000 };

		// ----- Disk I/O -----
		int aioThreads{ 10 };
		int hashingThreads{ 1 };
		int filePoolSize{ 100 };
		// MiB exposed by the native and Web UIs. libtorrent stores this setting
		// as a count of 16 KiB blocks, so the settings_pack boundary multiplies
		// and divides by 64.
		int checkingMemUsage{ 32 };
		// libtorrent 2.x counts bytes here. OpenNet deliberately keeps more
		// pending I/O than the old 1 MiB default so a brief storage stall does
		// not starve a high-throughput network connection.
		int diskQueueSize{ 100 * 1024 * 1024 };
		bool pieceExtentAffinity{ false };
		bool uploadSuggestions{ false };
		int sendBufferWatermark{ 500 };
		int sendBufferLowWatermark{ 10 };
		int sendBufferWatermarkFactor{ 50 };
		bool disableV1HashesForHybrid{ false };
		std::string partFileDirectory;
		int maxTorrentDirectoryDepth{ 100 };

		// ----- Encryption -----
		EncryptionPolicy encryptionPolicy{ EncryptionPolicy::Enabled };
		bool preferRc4{ false };

		// ----- Proxy -----
		ProxyType proxyType{ ProxyType::None };
		std::string proxyHostname;
		int proxyPort{ 0 };
		std::string proxyUsername;
		std::string proxyPassword;
		bool proxyPeerConnections{ true };
		bool proxyTrackerConnections{ true };
		bool proxySendHostInConnect{ false };

		// ----- I2P -----
		bool enableI2p{ false };
		std::string i2pHostname{ "127.0.0.1" };
		int i2pPort{ 7656 };
		bool allowI2pMixed{ false };
		int i2pInboundQuantity{ 3 };
		int i2pOutboundQuantity{ 3 };
		int i2pInboundLength{ 3 };
		int i2pOutboundLength{ 3 };
		int i2pInboundLengthVariance{ 0 };
		int i2pOutboundLengthVariance{ 0 };

		// ----- Identity -----
		std::string userAgent{ "libtorrent/2.1.1" };
		std::string peerFingerprint{ "-ON0100-" }; // OpenNet client ID

		// ----- Download Defaults -----
		std::wstring defaultSavePath; // empty = user's Downloads folder
		bool preallocateStorage{ false };
		bool autoStartDownloads{ true };
		// When enabled, resuming a stopped torrent verifies all pieces first.
		// The default is deliberately off because a full recheck can be costly.
		bool recheckBeforeResume{ false };
		bool moveCompletedEnabled{ false };
		std::wstring moveCompletedPath;
	};

	// ---------------------------------------------------------------
	//  TorrentSettingsManager  – load / save to SQLite + fallback from JSON
	// ---------------------------------------------------------------
	class TorrentSettingsManager
	{
	public:
		static TorrentSettingsManager& Instance();

		// Load from SQLite (fallback: legacy JSON file)
		void Load();
		// Save current settings to SQLite
		void Save();

		// Read-only access (take a copy if you hold it across await points)
		TorrentSettings Get() const;
		// Replace all settings and save
		void Set(TorrentSettings const& settings);

		// Convenience: path of the legacy JSON file (for migration)
		std::wstring FilePath() const;

	private:
		TorrentSettingsManager() = default;
		~TorrentSettingsManager() = default;
		TorrentSettingsManager(TorrentSettingsManager const&) = delete;
		TorrentSettingsManager& operator=(TorrentSettingsManager const&) = delete;

		// Internal: save to SQLite database
		void SaveToSqlite() const;
		// Internal: load from SQLite database
		bool LoadFromSqlite();

		mutable std::mutex m_mutex;
		TorrentSettings m_settings;
		std::wstring m_filePath;
		bool m_loaded{ false };
	};

} // namespace OpenNet::Core
