/*
 * PROJECT:   OpenNet
 * FILE:      Core/TorrentSettings.cpp
 * PURPOSE:   Persistent settings for libtorrent – JSON serialization + settings_pack builder
 *
 * LICENSE:   The MIT License
 */
module;
#include "LibtorrentIncludeGuard.h"
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/session.hpp> // for proxy_type_t
#include "LibtorrentIncludeRestore.h"

module OpenNet.Core.TorrentSettings;

import OpenNet.Core.IO.FileSystem;
import OpenNet.Core.AppSettingsDatabase;
import winrt.Windows.Foundation;

namespace lt = libtorrent;

namespace OpenNet::Core
{
	// -------------------------------------------------------------------
	//  TorrentSettingsManager
	// -------------------------------------------------------------------

	TorrentSettingsManager& TorrentSettingsManager::Instance()
	{
		static TorrentSettingsManager s_instance;
		return s_instance;
	}

	std::wstring TorrentSettingsManager::FilePath() const
	{
		return m_filePath;
	}

	TorrentSettings TorrentSettingsManager::Get() const
	{
		std::lock_guard lk(m_mutex);
		return m_settings;
	}

	void TorrentSettingsManager::Set(TorrentSettings const& settings)
	{
		{
			std::lock_guard lk(m_mutex);
			m_settings = settings;
		}
		Save();
	}

	void TorrentSettingsManager::Load()
	{
		std::lock_guard lk(m_mutex);
		if (m_loaded)
			return;

		try
		{
			// Try loading from SQLite first
			if (!LoadFromSqlite())
			{
				SaveToSqlite();
			}

			// Resolve default save path if empty
			if (m_settings.defaultSavePath.empty())
			{
				auto downloads = winrt::OpenNet::Core::IO::FileSystem::GetDownloadsPathW().GetResults();
				if (!downloads.empty())
				{
					m_settings.defaultSavePath = downloads;
				}
				else
				{
					// NO Downloads folder? Fallback to AppData\Downloads					
					auto fallback = std::wstring(winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW()) + L"\\Downloads";
					std::filesystem::create_directories(fallback);
					m_settings.defaultSavePath = fallback;
				}
			}
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("TorrentSettingsManager::Load error: " + std::string(ex.what()) + "\n").c_str());
		}

		m_loaded = true;
	}

	void TorrentSettingsManager::Save()
	{
		std::lock_guard lk(m_mutex);
		SaveToSqlite();
	}

	// ---------------------------------------------------------------
	//  Internal: save all settings fields to SQLite AppSettingsDatabase
	// ---------------------------------------------------------------
	void TorrentSettingsManager::SaveToSqlite() const
	{
		try
		{
			auto& db = AppSettingsDatabase::Instance();
			auto const& s = m_settings;

			// Connection
			db.SetString(AppSettingsDatabase::CAT_CONNECTION, "listenInterfaces", s.listenInterfaces);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "connectionsLimit", s.connectionsLimit);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "enableIncomingTcp", s.enableIncomingTcp);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "enableOutgoingTcp", s.enableOutgoingTcp);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "enableIncomingUtp", s.enableIncomingUtp);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "enableOutgoingUtp", s.enableOutgoingUtp);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "allowMultipleConnectionsPerIp", s.allowMultipleConnectionsPerIp);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "anonymousMode", s.anonymousMode);

			// Discovery
			db.SetBool(AppSettingsDatabase::CAT_DISCOVERY, "enableDht", s.enableDht);
			db.SetBool(AppSettingsDatabase::CAT_DISCOVERY, "enableLsd", s.enableLsd);
			db.SetBool(AppSettingsDatabase::CAT_DISCOVERY, "enableUpnp", s.enableUpnp);
			db.SetBool(AppSettingsDatabase::CAT_DISCOVERY, "enableNatpmp", s.enableNatpmp);
			db.SetInt(AppSettingsDatabase::CAT_DISCOVERY, "upnpLeaseDuration", s.upnpLeaseDuration);

			// Tracker
			db.SetBool(AppSettingsDatabase::CAT_TRACKER, "announceToAllTiers", s.announceToAllTiers);
			db.SetBool(AppSettingsDatabase::CAT_TRACKER, "announceToAllTrackers", s.announceToAllTrackers);
			db.SetString(AppSettingsDatabase::CAT_TRACKER, "dhtBootstrapNodes", s.dhtBootstrapNodes);
			db.SetString(AppSettingsDatabase::CAT_TRACKER, "announceIp", s.announceIp);
			db.SetInt(AppSettingsDatabase::CAT_TRACKER, "announcePort", s.announcePort);
			db.SetInt(AppSettingsDatabase::CAT_TRACKER, "maxConcurrentHttpAnnounces", s.maxConcurrentHttpAnnounces);
			db.SetInt(AppSettingsDatabase::CAT_TRACKER, "stopTrackerTimeout", s.stopTrackerTimeout);

			// Limits (Torrent category)
			db.SetBool(AppSettingsDatabase::CAT_TORRENT, "queueingEnabled", s.queueingEnabled);
			db.SetInt(AppSettingsDatabase::CAT_TORRENT, "activeDownloads", s.activeDownloads);
			db.SetInt(AppSettingsDatabase::CAT_TORRENT, "activeSeeds", s.activeSeeds);
			db.SetInt(AppSettingsDatabase::CAT_TORRENT, "activeLimit", s.activeLimit);

			// Speed limits
			db.SetInt(AppSettingsDatabase::CAT_SPEED_LIMIT, "downloadRateLimit", s.downloadRateLimit);
			db.SetInt(AppSettingsDatabase::CAT_SPEED_LIMIT, "uploadRateLimit", s.uploadRateLimit);

			// Seeding
			db.SetDouble(AppSettingsDatabase::CAT_SEEDING, "seedingRatioLimit", s.seedingRatioLimit);
			db.SetInt(AppSettingsDatabase::CAT_SEEDING, "seedingTimeLimit", s.seedingTimeLimit);

			// Peer
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "peerTimeout", s.peerTimeout);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "handshakeTimeout", s.handshakeTimeout);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "closeRedundantConnections", s.closeRedundantConnections);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "maxPeerListSize", s.maxPeerListSize);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "connectionSpeed", s.connectionSpeed);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "seedingOutgoingConnections", s.seedingOutgoingConnections);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "socketSendBufferSize", s.socketSendBufferSize);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "socketReceiveBufferSize", s.socketReceiveBufferSize);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "socketBacklogSize", s.socketBacklogSize);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "mixedModeAlgorithm", s.mixedModeAlgorithm);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "hostnameCacheTtl", s.hostnameCacheTtl);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "validateHttpsTrackers", s.validateHttpsTrackers);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "ssrfMitigation", s.ssrfMitigation);
			db.SetBool(AppSettingsDatabase::CAT_CONNECTION, "blockPeersOnPrivilegedPorts", s.blockPeersOnPrivilegedPorts);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "peerTurnover", s.peerTurnover);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "peerTurnoverCutoff", s.peerTurnoverCutoff);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "peerTurnoverInterval", s.peerTurnoverInterval);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "requestQueueSize", s.requestQueueSize);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "uploadSlotsBehavior", s.uploadSlotsBehavior);
			db.SetInt(AppSettingsDatabase::CAT_CONNECTION, "uploadChokingAlgorithm", s.uploadChokingAlgorithm);

			// Disk I/O
			db.SetInt(AppSettingsDatabase::CAT_DISK_IO, "aioThreads", s.aioThreads);
			db.SetInt(AppSettingsDatabase::CAT_DISK_IO, "hashingThreads", s.hashingThreads);
			db.SetInt(AppSettingsDatabase::CAT_DISK_IO, "filePoolSize", s.filePoolSize);
			db.SetInt(AppSettingsDatabase::CAT_DISK_IO, "checkingMemUsage", s.checkingMemUsage);
			db.SetInt(AppSettingsDatabase::CAT_DISK_IO, "diskQueueSize", s.diskQueueSize);
			db.SetBool(AppSettingsDatabase::CAT_DISK_IO, "pieceExtentAffinity", s.pieceExtentAffinity);
			db.SetBool(AppSettingsDatabase::CAT_DISK_IO, "uploadSuggestions", s.uploadSuggestions);
			db.SetInt(AppSettingsDatabase::CAT_DISK_IO, "sendBufferWatermark", s.sendBufferWatermark);
			db.SetInt(AppSettingsDatabase::CAT_DISK_IO, "sendBufferLowWatermark", s.sendBufferLowWatermark);
			db.SetInt(AppSettingsDatabase::CAT_DISK_IO, "sendBufferWatermarkFactor", s.sendBufferWatermarkFactor);

			// Encryption
			db.SetInt(AppSettingsDatabase::CAT_ENCRYPTION, "encryptionPolicy", static_cast<int>(s.encryptionPolicy));
			db.SetBool(AppSettingsDatabase::CAT_ENCRYPTION, "preferRc4", s.preferRc4);

			// Proxy
			db.SetInt(AppSettingsDatabase::CAT_PROXY, "proxyType", static_cast<int>(s.proxyType));
			db.SetString(AppSettingsDatabase::CAT_PROXY, "proxyHostname", s.proxyHostname);
			db.SetInt(AppSettingsDatabase::CAT_PROXY, "proxyPort", s.proxyPort);
			db.SetString(AppSettingsDatabase::CAT_PROXY, "proxyUsername", s.proxyUsername);
			db.SetString(AppSettingsDatabase::CAT_PROXY, "proxyPassword", s.proxyPassword);
			db.SetBool(AppSettingsDatabase::CAT_PROXY, "proxyPeerConnections", s.proxyPeerConnections);
			db.SetBool(AppSettingsDatabase::CAT_PROXY, "proxyTrackerConnections", s.proxyTrackerConnections);

			// Identity
			db.SetString(AppSettingsDatabase::CAT_IDENTITY, "userAgent", s.userAgent);
			db.SetString(AppSettingsDatabase::CAT_IDENTITY, "peerFingerprint", s.peerFingerprint);

			// Download defaults
			db.SetStringW(AppSettingsDatabase::CAT_DOWNLOAD, "defaultSavePath", s.defaultSavePath);
			db.SetBool(AppSettingsDatabase::CAT_DOWNLOAD, "preallocateStorage", s.preallocateStorage);
			db.SetBool(AppSettingsDatabase::CAT_DOWNLOAD, "autoStartDownloads", s.autoStartDownloads);
			db.SetBool(AppSettingsDatabase::CAT_DOWNLOAD, "recheckBeforeResume", s.recheckBeforeResume);
			db.SetBool(AppSettingsDatabase::CAT_DOWNLOAD, "moveCompletedEnabled", s.moveCompletedEnabled);
			db.SetStringW(AppSettingsDatabase::CAT_DOWNLOAD, "moveCompletedPath", s.moveCompletedPath);
			db.SetInt(AppSettingsDatabase::CAT_TORRENT, "performanceDefaultsVersion", 1);
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("TorrentSettingsManager::SaveToSqlite error: " + std::string(ex.what()) + "\n").c_str());
		}
	}

	// ---------------------------------------------------------------
	//  Internal: load settings from SQLite AppSettingsDatabase
	// ---------------------------------------------------------------
	bool TorrentSettingsManager::LoadFromSqlite()
	{
		try
		{
			auto& db = AppSettingsDatabase::Instance();

			// Check if any torrent-related settings exist in the database
			auto connSettings = db.GetCategory(AppSettingsDatabase::CAT_CONNECTION);
			if (connSettings.empty())
				return false; // No settings in DB → not yet migrated

			auto& s = m_settings;

			// Connection
			s.listenInterfaces = db.GetString(AppSettingsDatabase::CAT_CONNECTION, "listenInterfaces").value_or(s.listenInterfaces);
			s.connectionsLimit = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "connectionsLimit").value_or(s.connectionsLimit));
			s.enableIncomingTcp = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "enableIncomingTcp").value_or(s.enableIncomingTcp);
			s.enableOutgoingTcp = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "enableOutgoingTcp").value_or(s.enableOutgoingTcp);
			s.enableIncomingUtp = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "enableIncomingUtp").value_or(s.enableIncomingUtp);
			s.enableOutgoingUtp = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "enableOutgoingUtp").value_or(s.enableOutgoingUtp);
			s.allowMultipleConnectionsPerIp = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "allowMultipleConnectionsPerIp").value_or(s.allowMultipleConnectionsPerIp);
			s.anonymousMode = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "anonymousMode").value_or(s.anonymousMode);

			// Discovery
			s.enableDht = db.GetBool(AppSettingsDatabase::CAT_DISCOVERY, "enableDht").value_or(s.enableDht);
			s.enableLsd = db.GetBool(AppSettingsDatabase::CAT_DISCOVERY, "enableLsd").value_or(s.enableLsd);
			s.enableUpnp = db.GetBool(AppSettingsDatabase::CAT_DISCOVERY, "enableUpnp").value_or(s.enableUpnp);
			s.enableNatpmp = db.GetBool(AppSettingsDatabase::CAT_DISCOVERY, "enableNatpmp").value_or(s.enableNatpmp);
			s.upnpLeaseDuration = static_cast<int>(db.GetInt(
				AppSettingsDatabase::CAT_DISCOVERY, "upnpLeaseDuration")
				.value_or(s.upnpLeaseDuration));

			// Tracker
			s.announceToAllTiers = db.GetBool(AppSettingsDatabase::CAT_TRACKER, "announceToAllTiers").value_or(s.announceToAllTiers);
			s.announceToAllTrackers = db.GetBool(AppSettingsDatabase::CAT_TRACKER, "announceToAllTrackers").value_or(s.announceToAllTrackers);
			s.dhtBootstrapNodes = db.GetString(AppSettingsDatabase::CAT_TRACKER, "dhtBootstrapNodes").value_or(s.dhtBootstrapNodes);
			s.announceIp = db.GetString(AppSettingsDatabase::CAT_TRACKER, "announceIp").value_or(s.announceIp);
			s.announcePort = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_TRACKER, "announcePort").value_or(s.announcePort));
			s.maxConcurrentHttpAnnounces = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_TRACKER, "maxConcurrentHttpAnnounces").value_or(s.maxConcurrentHttpAnnounces));
			s.stopTrackerTimeout = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_TRACKER, "stopTrackerTimeout").value_or(s.stopTrackerTimeout));

			// Limits
			s.activeDownloads = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_TORRENT, "activeDownloads").value_or(s.activeDownloads));
			s.activeSeeds = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_TORRENT, "activeSeeds").value_or(s.activeSeeds));
			s.activeLimit = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_TORRENT, "activeLimit").value_or(s.activeLimit));
			auto const storedQueueing = db.GetBool(
				AppSettingsDatabase::CAT_TORRENT, "queueingEnabled");
			s.queueingEnabled = storedQueueing.value_or(
				s.activeDownloads >= 0 || s.activeSeeds >= 0 || s.activeLimit >= 0);
			// Migrate the former WebUI representation, where disabling queueing
			// destroyed all three configured limits by storing -1 in each field.
			if (s.activeDownloads < 0) s.activeDownloads = 3;
			if (s.activeSeeds < 0) s.activeSeeds = 5;
			if (s.activeLimit < 0) s.activeLimit = 15;

			// Speed limits
			s.downloadRateLimit = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_SPEED_LIMIT, "downloadRateLimit").value_or(s.downloadRateLimit));
			s.uploadRateLimit = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_SPEED_LIMIT, "uploadRateLimit").value_or(s.uploadRateLimit));

			// Seeding
			s.seedingRatioLimit = db.GetDouble(AppSettingsDatabase::CAT_SEEDING, "seedingRatioLimit").value_or(s.seedingRatioLimit);
			s.seedingTimeLimit = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_SEEDING, "seedingTimeLimit").value_or(s.seedingTimeLimit));

			// Peer
			s.peerTimeout = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "peerTimeout").value_or(s.peerTimeout));
			s.handshakeTimeout = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "handshakeTimeout").value_or(s.handshakeTimeout));
			s.closeRedundantConnections = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "closeRedundantConnections").value_or(s.closeRedundantConnections);
			s.maxPeerListSize = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "maxPeerListSize").value_or(s.maxPeerListSize));
			s.connectionSpeed = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "connectionSpeed").value_or(s.connectionSpeed));
			s.seedingOutgoingConnections = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "seedingOutgoingConnections").value_or(s.seedingOutgoingConnections);
			s.socketSendBufferSize = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "socketSendBufferSize").value_or(s.socketSendBufferSize));
			s.socketReceiveBufferSize = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "socketReceiveBufferSize").value_or(s.socketReceiveBufferSize));
			s.socketBacklogSize = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "socketBacklogSize").value_or(s.socketBacklogSize));
			s.mixedModeAlgorithm = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "mixedModeAlgorithm").value_or(s.mixedModeAlgorithm));
			s.hostnameCacheTtl = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "hostnameCacheTtl").value_or(s.hostnameCacheTtl));
			s.validateHttpsTrackers = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "validateHttpsTrackers").value_or(s.validateHttpsTrackers);
			s.ssrfMitigation = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "ssrfMitigation").value_or(s.ssrfMitigation);
			s.blockPeersOnPrivilegedPorts = db.GetBool(AppSettingsDatabase::CAT_CONNECTION, "blockPeersOnPrivilegedPorts").value_or(s.blockPeersOnPrivilegedPorts);
			s.peerTurnover = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "peerTurnover").value_or(s.peerTurnover));
			s.peerTurnoverCutoff = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "peerTurnoverCutoff").value_or(s.peerTurnoverCutoff));
			s.peerTurnoverInterval = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "peerTurnoverInterval").value_or(s.peerTurnoverInterval));
			s.requestQueueSize = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "requestQueueSize").value_or(s.requestQueueSize));
			s.uploadSlotsBehavior = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "uploadSlotsBehavior").value_or(s.uploadSlotsBehavior));
			s.uploadChokingAlgorithm = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_CONNECTION, "uploadChokingAlgorithm").value_or(s.uploadChokingAlgorithm));

			// Disk I/O
			s.aioThreads = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_DISK_IO, "aioThreads").value_or(s.aioThreads));
			s.hashingThreads = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_DISK_IO, "hashingThreads").value_or(s.hashingThreads));
			s.filePoolSize = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_DISK_IO, "filePoolSize").value_or(s.filePoolSize));
			s.checkingMemUsage = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_DISK_IO, "checkingMemUsage").value_or(s.checkingMemUsage));
			s.diskQueueSize = static_cast<int>(db.GetInt(
				AppSettingsDatabase::CAT_DISK_IO, "diskQueueSize")
				.value_or(s.diskQueueSize));
			auto const performanceDefaultsVersion = db.GetInt(
				AppSettingsDatabase::CAT_TORRENT,
				"performanceDefaultsVersion").value_or(0);
			if (performanceDefaultsVersion < 1)
			{
				// One-time migration of the former built-in defaults.  The
				// version marker prevents a later explicit 1 MiB choice from
				// being rewritten on every launch.
				if (s.connectionsLimit == 200) s.connectionsLimit = 500;
				if (s.aioThreads == 4) s.aioThreads = 10;
				if (s.filePoolSize == 40) s.filePoolSize = 100;
				if (s.diskQueueSize == 1024 * 1024)
					s.diskQueueSize = 100 * 1024 * 1024;
				// Persist the migrated values before advancing the marker. Otherwise
				// a later launch would retain the old database values while skipping
				// this migration permanently.
				db.SetInt(AppSettingsDatabase::CAT_CONNECTION,
					"connectionsLimit", s.connectionsLimit);
				db.SetInt(AppSettingsDatabase::CAT_DISK_IO,
					"aioThreads", s.aioThreads);
				db.SetInt(AppSettingsDatabase::CAT_DISK_IO,
					"filePoolSize", s.filePoolSize);
				db.SetInt(AppSettingsDatabase::CAT_DISK_IO,
					"diskQueueSize", s.diskQueueSize);
				db.SetInt(
					AppSettingsDatabase::CAT_TORRENT,
					"performanceDefaultsVersion", 1);
			}
			if (performanceDefaultsVersion < 2)
			{
				// Older builds labelled this value as MiB but passed it directly as
				// libtorrent's 16 KiB block count. Migrate the former built-in 256
				// value (4 MiB effective) to qBittorrent's 32 MiB default.
				if (s.checkingMemUsage == 256) s.checkingMemUsage = 32;
				db.SetInt(AppSettingsDatabase::CAT_DISK_IO,
					"checkingMemUsage", s.checkingMemUsage);
				db.SetInt(AppSettingsDatabase::CAT_TORRENT,
					"performanceDefaultsVersion", 2);
			}
			s.pieceExtentAffinity = db.GetBool(AppSettingsDatabase::CAT_DISK_IO, "pieceExtentAffinity").value_or(s.pieceExtentAffinity);
			s.uploadSuggestions = db.GetBool(AppSettingsDatabase::CAT_DISK_IO, "uploadSuggestions").value_or(s.uploadSuggestions);
			s.sendBufferWatermark = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_DISK_IO, "sendBufferWatermark").value_or(s.sendBufferWatermark));
			s.sendBufferLowWatermark = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_DISK_IO, "sendBufferLowWatermark").value_or(s.sendBufferLowWatermark));
			s.sendBufferWatermarkFactor = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_DISK_IO, "sendBufferWatermarkFactor").value_or(s.sendBufferWatermarkFactor));

			// Encryption
			s.encryptionPolicy = static_cast<EncryptionPolicy>(db.GetInt(AppSettingsDatabase::CAT_ENCRYPTION, "encryptionPolicy").value_or(static_cast<int>(s.encryptionPolicy)));
			s.preferRc4 = db.GetBool(AppSettingsDatabase::CAT_ENCRYPTION, "preferRc4").value_or(s.preferRc4);

			// Proxy
			s.proxyType = static_cast<ProxyType>(db.GetInt(AppSettingsDatabase::CAT_PROXY, "proxyType").value_or(static_cast<int>(s.proxyType)));
			s.proxyHostname = db.GetString(AppSettingsDatabase::CAT_PROXY, "proxyHostname").value_or(s.proxyHostname);
			s.proxyPort = static_cast<int>(db.GetInt(AppSettingsDatabase::CAT_PROXY, "proxyPort").value_or(s.proxyPort));
			s.proxyUsername = db.GetString(AppSettingsDatabase::CAT_PROXY, "proxyUsername").value_or(s.proxyUsername);
			s.proxyPassword = db.GetString(AppSettingsDatabase::CAT_PROXY, "proxyPassword").value_or(s.proxyPassword);
			s.proxyPeerConnections = db.GetBool(AppSettingsDatabase::CAT_PROXY, "proxyPeerConnections").value_or(s.proxyPeerConnections);
			s.proxyTrackerConnections = db.GetBool(AppSettingsDatabase::CAT_PROXY, "proxyTrackerConnections").value_or(s.proxyTrackerConnections);

			// Identity
			s.userAgent = db.GetString(AppSettingsDatabase::CAT_IDENTITY, "userAgent").value_or(s.userAgent);
			s.peerFingerprint = db.GetString(AppSettingsDatabase::CAT_IDENTITY, "peerFingerprint").value_or(s.peerFingerprint);

			// Download defaults
			s.defaultSavePath = db.GetStringW(AppSettingsDatabase::CAT_DOWNLOAD, "defaultSavePath").value_or(s.defaultSavePath);
			s.preallocateStorage = db.GetBool(AppSettingsDatabase::CAT_DOWNLOAD, "preallocateStorage").value_or(s.preallocateStorage);
			s.autoStartDownloads = db.GetBool(AppSettingsDatabase::CAT_DOWNLOAD, "autoStartDownloads").value_or(s.autoStartDownloads);
			s.recheckBeforeResume = db.GetBool(AppSettingsDatabase::CAT_DOWNLOAD, "recheckBeforeResume").value_or(s.recheckBeforeResume);
			s.moveCompletedEnabled = db.GetBool(AppSettingsDatabase::CAT_DOWNLOAD, "moveCompletedEnabled").value_or(s.moveCompletedEnabled);
			s.moveCompletedPath = db.GetStringW(AppSettingsDatabase::CAT_DOWNLOAD, "moveCompletedPath").value_or(s.moveCompletedPath);

			return true;
		}
		catch (std::exception const& ex)
		{
			OutputDebugStringA(("TorrentSettingsManager::LoadFromSqlite error: " + std::string(ex.what()) + "\n").c_str());
			return false;
		}
	}

} // namespace OpenNet::Core

// -------------------------------------------------------------------
//  Free function: build a libtorrent settings_pack from TorrentSettings
//  This lives here so libtorrent headers stay out of TorrentSettings.h
// -------------------------------------------------------------------
namespace OpenNet::Core
{

	void ApplyTorrentSettingsToSettingsPack(
		TorrentSettings const& s,
		lt::settings_pack& pack)
	{
		// Connection
		pack.set_str(lt::settings_pack::listen_interfaces, s.listenInterfaces);
		pack.set_bool(lt::settings_pack::listen_system_port_fallback, false);
		pack.set_int(lt::settings_pack::connections_limit, s.connectionsLimit);
		pack.set_bool(lt::settings_pack::enable_incoming_tcp, s.enableIncomingTcp);
		pack.set_bool(lt::settings_pack::enable_outgoing_tcp, s.enableOutgoingTcp);
		pack.set_bool(lt::settings_pack::enable_incoming_utp, s.enableIncomingUtp);
		pack.set_bool(lt::settings_pack::enable_outgoing_utp, s.enableOutgoingUtp);
		pack.set_bool(lt::settings_pack::allow_multiple_connections_per_ip, s.allowMultipleConnectionsPerIp);
		pack.set_bool(lt::settings_pack::anonymous_mode, s.anonymousMode);

		// Discovery
		pack.set_bool(lt::settings_pack::enable_dht, s.enableDht);
		pack.set_bool(lt::settings_pack::enable_lsd, s.enableLsd);
		pack.set_bool(lt::settings_pack::enable_upnp, s.enableUpnp);
		pack.set_bool(lt::settings_pack::enable_natpmp, s.enableNatpmp);
		pack.set_int(lt::settings_pack::upnp_lease_duration,
					 std::max(0, s.upnpLeaseDuration));

		// Tracker
		pack.set_bool(lt::settings_pack::announce_to_all_tiers, s.announceToAllTiers);
		pack.set_bool(lt::settings_pack::announce_to_all_trackers, s.announceToAllTrackers);
		pack.set_str(lt::settings_pack::dht_bootstrap_nodes, s.dhtBootstrapNodes);
		pack.set_str(lt::settings_pack::announce_ip, s.announceIp);
		pack.set_int(lt::settings_pack::announce_port, s.announcePort);
		pack.set_int(lt::settings_pack::max_concurrent_http_announces, s.maxConcurrentHttpAnnounces);
		pack.set_int(lt::settings_pack::stop_tracker_timeout, s.stopTrackerTimeout);

		// Limits
		pack.set_int(lt::settings_pack::active_downloads,
			s.queueingEnabled ? s.activeDownloads : -1);
		pack.set_int(lt::settings_pack::active_seeds,
			s.queueingEnabled ? s.activeSeeds : -1);
		pack.set_int(lt::settings_pack::active_limit,
			s.queueingEnabled ? s.activeLimit : -1);

		// Speed limits
		pack.set_int(lt::settings_pack::download_rate_limit, s.downloadRateLimit);
		pack.set_int(lt::settings_pack::upload_rate_limit, s.uploadRateLimit);

		// Peer
		pack.set_int(lt::settings_pack::peer_timeout, s.peerTimeout);
		pack.set_int(lt::settings_pack::handshake_timeout, s.handshakeTimeout);
		pack.set_bool(lt::settings_pack::close_redundant_connections, s.closeRedundantConnections);
		pack.set_int(lt::settings_pack::max_peerlist_size, s.maxPeerListSize);
		pack.set_int(lt::settings_pack::connection_speed, s.connectionSpeed);
		pack.set_bool(lt::settings_pack::seeding_outgoing_connections, s.seedingOutgoingConnections);
		pack.set_int(lt::settings_pack::send_socket_buffer_size, s.socketSendBufferSize);
		pack.set_int(lt::settings_pack::recv_socket_buffer_size, s.socketReceiveBufferSize);
		pack.set_int(lt::settings_pack::listen_queue_size, s.socketBacklogSize);
		pack.set_int(lt::settings_pack::mixed_mode_algorithm, s.mixedModeAlgorithm);
		pack.set_int(lt::settings_pack::resolver_cache_timeout, s.hostnameCacheTtl);
		pack.set_bool(lt::settings_pack::validate_https_trackers, s.validateHttpsTrackers);
		pack.set_bool(lt::settings_pack::ssrf_mitigation, s.ssrfMitigation);
		pack.set_bool(lt::settings_pack::no_connect_privileged_ports, s.blockPeersOnPrivilegedPorts);
		pack.set_int(lt::settings_pack::peer_turnover, s.peerTurnover);
		pack.set_int(lt::settings_pack::peer_turnover_cutoff, s.peerTurnoverCutoff);
		pack.set_int(lt::settings_pack::peer_turnover_interval, s.peerTurnoverInterval);
		pack.set_int(lt::settings_pack::max_out_request_queue, s.requestQueueSize);
		pack.set_int(lt::settings_pack::choking_algorithm, s.uploadSlotsBehavior);
		pack.set_int(lt::settings_pack::seed_choking_algorithm, s.uploadChokingAlgorithm);

		// Disk I/O
		pack.set_int(lt::settings_pack::aio_threads, s.aioThreads);
		pack.set_int(lt::settings_pack::hashing_threads, s.hashingThreads);
		pack.set_int(lt::settings_pack::file_pool_size, s.filePoolSize);
		pack.set_int(lt::settings_pack::checking_mem_usage,
			std::max(1, s.checkingMemUsage) * 64);
		pack.set_int(lt::settings_pack::max_queued_disk_bytes, s.diskQueueSize);
		pack.set_bool(lt::settings_pack::piece_extent_affinity, s.pieceExtentAffinity);
		pack.set_int(lt::settings_pack::suggest_mode, s.uploadSuggestions
					 ? lt::settings_pack::suggest_read_cache
					 : lt::settings_pack::no_piece_suggestions);
		pack.set_int(lt::settings_pack::send_buffer_watermark, s.sendBufferWatermark * 1024);
		pack.set_int(lt::settings_pack::send_buffer_low_watermark, s.sendBufferLowWatermark * 1024);
		pack.set_int(lt::settings_pack::send_buffer_watermark_factor, s.sendBufferWatermarkFactor);

		// Encryption (pe_settings mapped to settings_pack ints)
		// In libtorrent 2.x: out_enc_policy, in_enc_policy, allowed_enc_level
		int encPolicy = 1; // enabled
		switch (s.encryptionPolicy)
		{
			case EncryptionPolicy::Forced:
				encPolicy = 0;
				break; // pe_forced
			case EncryptionPolicy::Enabled:
				encPolicy = 1;
				break; // pe_enabled
			case EncryptionPolicy::Disabled:
				encPolicy = 2;
				break; // pe_disabled
		}
		pack.set_int(lt::settings_pack::out_enc_policy, encPolicy);
		pack.set_int(lt::settings_pack::in_enc_policy, encPolicy);

		int encLevel = s.preferRc4 ? 1 /*pe_rc4*/ : 3 /*pe_both*/;
		pack.set_int(lt::settings_pack::allowed_enc_level, encLevel);
		pack.set_bool(lt::settings_pack::prefer_rc4, s.preferRc4);

		// Proxy
		pack.set_int(lt::settings_pack::proxy_type, static_cast<int>(s.proxyType));
		pack.set_str(lt::settings_pack::proxy_hostname, s.proxyHostname);
		pack.set_int(lt::settings_pack::proxy_port, s.proxyPort);
		pack.set_str(lt::settings_pack::proxy_username, s.proxyUsername);
		pack.set_str(lt::settings_pack::proxy_password, s.proxyPassword);
		pack.set_bool(lt::settings_pack::proxy_peer_connections, s.proxyPeerConnections);
		pack.set_bool(lt::settings_pack::proxy_tracker_connections, s.proxyTrackerConnections);

		// Identity
		pack.set_str(lt::settings_pack::user_agent, s.userAgent);
		pack.set_str(lt::settings_pack::peer_fingerprint, s.peerFingerprint);

		// Peer
		pack.set_int(lt::settings_pack::max_peerlist_size, s.maxPeerListSize);

		// Alert mask – include DHT category so dht_stats_alert fires for node count
		pack.set_int(lt::settings_pack::alert_mask,
					 lt::alert_category::status |
					 lt::alert_category::error |
					 lt::alert_category::storage |
					 lt::alert_category::peer |
					 lt::alert_category::connect |
					 lt::alert_category::tracker |
					 lt::alert_category::stats |
					 lt::alert_category::dht |
					 lt::alert_category::port_mapping);

		// Seeding limits are per-torrent in libtorrent; share_ratio_limit / seed_time_limit
		// are applied via session settings:
		pack.set_int(lt::settings_pack::share_ratio_limit,
					 s.seedingRatioLimit > 0 ? static_cast<int>(s.seedingRatioLimit * 100) : 0);
		pack.set_int(lt::settings_pack::seed_time_limit,
					 s.seedingTimeLimit > 0 ? s.seedingTimeLimit * 60 : 0); // minutes => seconds
	}

	TorrentSettings BuildTorrentSettingsFromPack(lt::settings_pack const& pack)
	{
		TorrentSettings s;

		s.listenInterfaces = pack.get_str(lt::settings_pack::listen_interfaces);
		s.connectionsLimit = pack.get_int(lt::settings_pack::connections_limit);
		s.enableIncomingTcp = pack.get_bool(lt::settings_pack::enable_incoming_tcp);
		s.enableOutgoingTcp = pack.get_bool(lt::settings_pack::enable_outgoing_tcp);
		s.enableIncomingUtp = pack.get_bool(lt::settings_pack::enable_incoming_utp);
		s.enableOutgoingUtp = pack.get_bool(lt::settings_pack::enable_outgoing_utp);
		s.allowMultipleConnectionsPerIp = pack.get_bool(lt::settings_pack::allow_multiple_connections_per_ip);
		s.anonymousMode = pack.get_bool(lt::settings_pack::anonymous_mode);

		s.enableDht = pack.get_bool(lt::settings_pack::enable_dht);
		s.enableLsd = pack.get_bool(lt::settings_pack::enable_lsd);
		s.enableUpnp = pack.get_bool(lt::settings_pack::enable_upnp);
		s.enableNatpmp = pack.get_bool(lt::settings_pack::enable_natpmp);
		s.upnpLeaseDuration = pack.get_int(lt::settings_pack::upnp_lease_duration);

		s.announceToAllTiers = pack.get_bool(lt::settings_pack::announce_to_all_tiers);
		s.announceToAllTrackers = pack.get_bool(lt::settings_pack::announce_to_all_trackers);
		s.dhtBootstrapNodes = pack.get_str(lt::settings_pack::dht_bootstrap_nodes);
		s.announceIp = pack.get_str(lt::settings_pack::announce_ip);
		s.announcePort = pack.get_int(lt::settings_pack::announce_port);
		s.maxConcurrentHttpAnnounces = pack.get_int(lt::settings_pack::max_concurrent_http_announces);
		s.stopTrackerTimeout = pack.get_int(lt::settings_pack::stop_tracker_timeout);

		auto const activeDownloads = pack.get_int(lt::settings_pack::active_downloads);
		auto const activeSeeds = pack.get_int(lt::settings_pack::active_seeds);
		auto const activeLimit = pack.get_int(lt::settings_pack::active_limit);
		s.queueingEnabled = activeDownloads >= 0 || activeSeeds >= 0 || activeLimit >= 0;
		if (activeDownloads >= 0) s.activeDownloads = activeDownloads;
		if (activeSeeds >= 0) s.activeSeeds = activeSeeds;
		if (activeLimit >= 0) s.activeLimit = activeLimit;

		s.downloadRateLimit = pack.get_int(lt::settings_pack::download_rate_limit);
		s.uploadRateLimit = pack.get_int(lt::settings_pack::upload_rate_limit);

		s.peerTimeout = pack.get_int(lt::settings_pack::peer_timeout);
		s.handshakeTimeout = pack.get_int(lt::settings_pack::handshake_timeout);
		s.closeRedundantConnections = pack.get_bool(lt::settings_pack::close_redundant_connections);
		s.maxPeerListSize = pack.get_int(lt::settings_pack::max_peerlist_size);
		s.connectionSpeed = pack.get_int(lt::settings_pack::connection_speed);
		s.seedingOutgoingConnections = pack.get_bool(lt::settings_pack::seeding_outgoing_connections);
		s.socketSendBufferSize = pack.get_int(lt::settings_pack::send_socket_buffer_size);
		s.socketReceiveBufferSize = pack.get_int(lt::settings_pack::recv_socket_buffer_size);
		s.socketBacklogSize = pack.get_int(lt::settings_pack::listen_queue_size);
		s.mixedModeAlgorithm = pack.get_int(lt::settings_pack::mixed_mode_algorithm);
		s.hostnameCacheTtl = pack.get_int(lt::settings_pack::resolver_cache_timeout);
		s.validateHttpsTrackers = pack.get_bool(lt::settings_pack::validate_https_trackers);
		s.ssrfMitigation = pack.get_bool(lt::settings_pack::ssrf_mitigation);
		s.blockPeersOnPrivilegedPorts = pack.get_bool(lt::settings_pack::no_connect_privileged_ports);
		s.peerTurnover = pack.get_int(lt::settings_pack::peer_turnover);
		s.peerTurnoverCutoff = pack.get_int(lt::settings_pack::peer_turnover_cutoff);
		s.peerTurnoverInterval = pack.get_int(lt::settings_pack::peer_turnover_interval);
		s.requestQueueSize = pack.get_int(lt::settings_pack::max_out_request_queue);
		s.uploadSlotsBehavior = pack.get_int(lt::settings_pack::choking_algorithm);
		s.uploadChokingAlgorithm = pack.get_int(lt::settings_pack::seed_choking_algorithm);

		s.aioThreads = pack.get_int(lt::settings_pack::aio_threads);
		s.hashingThreads = pack.get_int(lt::settings_pack::hashing_threads);
		s.filePoolSize = pack.get_int(lt::settings_pack::file_pool_size);
		s.checkingMemUsage = std::max(1,
			pack.get_int(lt::settings_pack::checking_mem_usage) / 64);
		s.diskQueueSize = pack.get_int(lt::settings_pack::max_queued_disk_bytes);
		s.pieceExtentAffinity = pack.get_bool(lt::settings_pack::piece_extent_affinity);
		s.uploadSuggestions = pack.get_int(lt::settings_pack::suggest_mode)
			== lt::settings_pack::suggest_read_cache;
		s.sendBufferWatermark = pack.get_int(lt::settings_pack::send_buffer_watermark) / 1024;
		s.sendBufferLowWatermark = pack.get_int(lt::settings_pack::send_buffer_low_watermark) / 1024;
		s.sendBufferWatermarkFactor = pack.get_int(lt::settings_pack::send_buffer_watermark_factor);

		s.userAgent = pack.get_str(lt::settings_pack::user_agent);
		s.peerFingerprint = pack.get_str(lt::settings_pack::peer_fingerprint);

		// Encryption
		int encPol = pack.get_int(lt::settings_pack::out_enc_policy);
		switch (encPol)
		{
			case 0:
				s.encryptionPolicy = EncryptionPolicy::Forced;
				break;
			case 2:
				s.encryptionPolicy = EncryptionPolicy::Disabled;
				break;
			default:
				s.encryptionPolicy = EncryptionPolicy::Enabled;
				break;
		}
		s.preferRc4 = pack.get_bool(lt::settings_pack::prefer_rc4);

		// Proxy
		s.proxyType = static_cast<ProxyType>(pack.get_int(lt::settings_pack::proxy_type));
		s.proxyHostname = pack.get_str(lt::settings_pack::proxy_hostname);
		s.proxyPort = pack.get_int(lt::settings_pack::proxy_port);
		s.proxyUsername = pack.get_str(lt::settings_pack::proxy_username);
		s.proxyPassword = pack.get_str(lt::settings_pack::proxy_password);
		s.proxyPeerConnections = pack.get_bool(lt::settings_pack::proxy_peer_connections);
		s.proxyTrackerConnections = pack.get_bool(lt::settings_pack::proxy_tracker_connections);

		// Seeding
		int ratioRaw = pack.get_int(lt::settings_pack::share_ratio_limit);
		s.seedingRatioLimit = ratioRaw > 0 ? ratioRaw / 100.0 : 0.0;
		int seedTimeSec = pack.get_int(lt::settings_pack::seed_time_limit);
		s.seedingTimeLimit = seedTimeSec > 0 ? seedTimeSec / 60 : 0;

		return s;
	}

} // namespace OpenNet::Core
