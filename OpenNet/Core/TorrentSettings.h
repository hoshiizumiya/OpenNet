/*
 * PROJECT:   OpenNet
 * FILE:      Core/TorrentSettings.h
 * PURPOSE:   Persistent settings for libtorrent session & download behaviour.
 *            Serialized as JSON to LocalFolder/torrent_settings.json.
 *
 * LICENSE:   The MIT License
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace OpenNet::Core
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
		std::string listenInterfaces{ "0.0.0.0:6881,[::]:6881" };
		int connectionsLimit{ 200 };
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

		// ----- Tracker -----
		bool announceToAllTiers{ false };
		bool announceToAllTrackers{ false };

		// ----- Limits -----
		int activeDownloads{ 3 };
		int activeSeeds{ 5 };
		int activeLimit{ 15 };

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

		// ----- Disk I/O -----
		int aioThreads{ 4 };
		int checkingMemUsage{ 256 };

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

		// ----- Identity -----
		std::string userAgent{ "libtorrent/2.0.11" };
		std::string peerFingerprint{ "-ON0100-" }; // OpenNet client ID

		// ----- Download Defaults -----
		std::wstring defaultSavePath; // empty = user's Downloads folder
		bool preallocateStorage{ false };
		bool autoStartDownloads{ true };
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

	// ---------------------------------------------------------------
	//  Free functions – map TorrentSettings <-> libtorrent settings_pack
	//  (implementations in TorrentSettings.cpp)
	// ---------------------------------------------------------------
} // namespace OpenNet::Core

// Forward-declare libtorrent type to avoid pulling in heavy headers.
namespace libtorrent
{
	struct settings_pack;
}

namespace OpenNet::Core
{
	void ApplyTorrentSettingsToSettingsPack(
		TorrentSettings const& settings,
		libtorrent::settings_pack& pack);

	TorrentSettings BuildTorrentSettingsFromPack(
		libtorrent::settings_pack const& pack);
} // namespace OpenNet::Core
