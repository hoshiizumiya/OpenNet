#pragma once

// Implementation-only adapter shared by the main libtorrent session and the
// metadata preview session. The Settings type is intentionally templated so
// this header does not expose libtorrent through the exported C++ modules.
namespace OpenNet::Core::Torrent::Detail
{
	template<typename Settings>
	void ApplyNetworkSettings(Settings const& settings, libtorrent::settings_pack& pack)
	{
		using libtorrent::settings_pack;

		pack.set_str(settings_pack::listen_interfaces, settings.listenInterfaces);
		pack.set_bool(settings_pack::listen_system_port_fallback, false);
		pack.set_bool(settings_pack::enable_incoming_tcp, settings.enableIncomingTcp);
		pack.set_bool(settings_pack::enable_outgoing_tcp, settings.enableOutgoingTcp);
		pack.set_bool(settings_pack::enable_incoming_utp, settings.enableIncomingUtp);
		pack.set_bool(settings_pack::enable_outgoing_utp, settings.enableOutgoingUtp);
		pack.set_bool(settings_pack::allow_multiple_connections_per_ip, settings.allowMultipleConnectionsPerIp);
		pack.set_bool(settings_pack::anonymous_mode, settings.anonymousMode);

		pack.set_bool(settings_pack::enable_dht, settings.enableDht);
		pack.set_bool(settings_pack::enable_lsd, settings.enableLsd);
		pack.set_bool(settings_pack::apply_filter_to_dht, settings.applyIpFilterToDht);
		pack.set_str(settings_pack::dht_bootstrap_nodes, settings.dhtBootstrapNodes);
		pack.set_bool(settings_pack::announce_to_all_tiers, settings.announceToAllTiers);
		pack.set_bool(settings_pack::announce_to_all_trackers, settings.announceToAllTrackers);
		pack.set_str(settings_pack::announce_ip, settings.announceIp);
		pack.set_int(settings_pack::announce_port, settings.announcePort);
		pack.set_int(settings_pack::max_concurrent_http_announces, settings.maxConcurrentHttpAnnounces);
		pack.set_int(settings_pack::stop_tracker_timeout, settings.stopTrackerTimeout);
		pack.set_str(settings_pack::webtorrent_stun_server, settings.webTorrentStunServer);
		pack.set_int(settings_pack::min_websocket_announce_interval, settings.minWebSocketAnnounceInterval);
		pack.set_int(settings_pack::webtorrent_connection_timeout, settings.webTorrentConnectionTimeout);
		pack.set_int(settings_pack::max_webtorrent_offers,
					 settings.enableWebTorrent
					 ? (std::max)(1, settings.maxWebTorrentOffers)
					 : 0);

		int encryptionPolicy = settings_pack::pe_enabled;
		switch (static_cast<int>(settings.encryptionPolicy))
		{
			case 0:
				encryptionPolicy = settings_pack::pe_forced;
				break;
			case 2:
				encryptionPolicy = settings_pack::pe_disabled;
				break;
			default:
				break;
		}
		pack.set_int(settings_pack::out_enc_policy, encryptionPolicy);
		pack.set_int(settings_pack::in_enc_policy, encryptionPolicy);
		// "Prefer RC4" still permits both payload encodings; preference is a
		// separate setting. pe_plaintext must never be used as pe_rc4.
		pack.set_int(settings_pack::allowed_enc_level, settings_pack::pe_both);
		pack.set_bool(settings_pack::prefer_rc4, settings.preferRc4);

		pack.set_int(settings_pack::proxy_type, static_cast<int>(settings.proxyType));
		pack.set_str(settings_pack::proxy_hostname, settings.proxyHostname);
		pack.set_int(settings_pack::proxy_port, settings.proxyPort);
		pack.set_str(settings_pack::proxy_username, settings.proxyUsername);
		pack.set_str(settings_pack::proxy_password, settings.proxyPassword);
		pack.set_bool(settings_pack::proxy_peer_connections, settings.proxyPeerConnections);
		pack.set_bool(settings_pack::proxy_tracker_connections, settings.proxyTrackerConnections);
		pack.set_bool(settings_pack::proxy_send_host_in_connect, settings.proxySendHostInConnect);
		pack.set_str(settings_pack::i2p_hostname,
					 settings.enableI2p ? settings.i2pHostname : "");
		pack.set_int(settings_pack::i2p_port, settings.i2pPort);
		pack.set_bool(settings_pack::allow_i2p_mixed, settings.allowI2pMixed);
		pack.set_int(settings_pack::i2p_inbound_quantity, settings.i2pInboundQuantity);
		pack.set_int(settings_pack::i2p_outbound_quantity, settings.i2pOutboundQuantity);
		pack.set_int(settings_pack::i2p_inbound_length, settings.i2pInboundLength);
		pack.set_int(settings_pack::i2p_outbound_length, settings.i2pOutboundLength);
		pack.set_int(settings_pack::i2p_inbound_length_variance, settings.i2pInboundLengthVariance);
		pack.set_int(settings_pack::i2p_outbound_length_variance, settings.i2pOutboundLengthVariance);

		pack.set_str(settings_pack::user_agent, settings.userAgent);
		pack.set_str(settings_pack::peer_fingerprint, settings.peerFingerprint);
	}
}
