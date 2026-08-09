#include "XamlWorkaround.h"
#include "NetworkInfo.h"

import OpenNet.Core.Utils.Message;

using namespace winrt;
using namespace Windows::Foundation;

namespace winrt::OpenNet::Models::implementation
{
	// NetworkInfo ctor
	NetworkInfo::NetworkInfo()
	{
		// 默认支持的协议 / default supported protocols
		supportedProtocols = {
			winrt::OpenNet::Models::ConnectionProtocol::TCP,
			winrt::OpenNet::Models::ConnectionProtocol::UDP,
			winrt::OpenNet::Models::ConnectionProtocol::HTTP
		};
		lastUpdated = winrt::clock::now();
		connectedTime = {};
	}

	// Helpers
	hstring NetworkInfo::GetStatusString() const
	{
		switch (status)
		{
			case ConnectionStatus::Disconnected: return ResourceGetString(L"ModelNetworkInfoStatusDisconnected");
			case ConnectionStatus::Connecting: return ResourceGetString(L"ModelNetworkInfoStatusConnecting");
			case ConnectionStatus::Connected: return ResourceGetString(L"ModelNetworkInfoStatusConnected");
			case ConnectionStatus::Limited: return ResourceGetString(L"ModelNetworkInfoStatusLimited");
			case ConnectionStatus::NoInternet: return ResourceGetString(L"ModelNetworkInfoStatusNoInternet");
			default: return ResourceGetString(L"CommonUnknown");
		}
	}

	hstring NetworkInfo::GetNATTypeString() const
	{
		switch (natType)
		{
			case NATType::Unknown: return ResourceGetString(L"CommonUnknown");
			case NATType::Open: return ResourceGetString(L"CoreNetworkDetectorNATOpenInternet");
			case NATType::FullCone: return ResourceGetString(L"CoreNetworkDetectorNATFullCone");
			case NATType::RestrictedCone: return ResourceGetString(L"CoreNetworkDetectorNATRestrictedCone");
			case NATType::PortRestricted: return ResourceGetString(L"CoreNetworkDetectorNATPortRestrictedCone");
			case NATType::Symmetric: return ResourceGetString(L"CoreNetworkDetectorNATSymmetric");
			case NATType::Blocked: return ResourceGetString(L"ModelNetworkInfoNATBlocked");
			default: return ResourceGetString(L"CommonUnknown");
		}
	}

	hstring NetworkInfo::GetProtocolPriorityString() const
	{
		switch (protocolPriority)
		{
			case IPProtocolPriority::IPv4First: return ResourceGetString(L"Protocol_IPv4First");
			case IPProtocolPriority::IPv6First: return ResourceGetString(L"Protocol_IPv6First");
			case IPProtocolPriority::IPv4Only: return ResourceGetString(L"Protocol_IPv4Only");
			case IPProtocolPriority::IPv6Only: return ResourceGetString(L"Protocol_IPv6Only");
			case IPProtocolPriority::Auto: return ResourceGetString(L"Protocol_Auto");
			default: return ResourceGetString(L"CommonUnknown");
		}
	}

	hstring NetworkInfo::GetPreferredProtocolString() const
	{
		switch (preferredProtocol)
		{
			case ConnectionProtocol::Auto: return ResourceGetString(L"ConnProtocol_Auto");
			case ConnectionProtocol::TCP: return ResourceGetString(L"ConnProtocol_TCP");
			case ConnectionProtocol::UDP: return ResourceGetString(L"ConnProtocol_UDP");
			case ConnectionProtocol::UTP: return ResourceGetString(L"ConnProtocol_UTP");
			case ConnectionProtocol::BitTorrent: return ResourceGetString(L"ConnProtocol_BitTorrent");
			case ConnectionProtocol::DHT: return ResourceGetString(L"ConnProtocol_DHT");
			case ConnectionProtocol::WebRTC: return ResourceGetString(L"ConnProtocol_WebRTC");
			case ConnectionProtocol::HTTP: return ResourceGetString(L"ConnProtocol_HTTP");
			default: return ResourceGetString(L"CommonUnknown");
		}
	}

	double NetworkInfo::GetNetworkScore() const
	{
		double score = 100.0;
		if (ipv4Status.available) score += 10.0;
		if (ipv6Status.available) score += 15.0;
		if (ipv4Status.hasPublicIP) score += 5.0;
		if (ipv6Status.hasGlobalAddress) score += 10.0;
		if (latency > 200) score -= 30;
		else if (latency > 100) score -= 15;
		else if (latency > 50) score -= 5;
		score -= packetLoss * 100.0;
		if (signalStrength < 0.5) score -= 20; else if (signalStrength < 0.7) score -= 10;
		switch (natType)
		{
			case NATType::Open: score += 20; break;
			case NATType::FullCone: score += 15; break;
			case NATType::RestrictedCone: score += 10; break;
			case NATType::PortRestricted: score += 5; break;
			case NATType::Symmetric: score -= 5; break;
			case NATType::Blocked: score -= 20; break;
			default: break;
		}
		if (upnpAvailable) score += 10;
		if (pmpAvailable) score += 5;
		if (iceSupported) score += 15;
		return score > 0.0 ? score : 0.0;
	}

	hstring NetworkInfo::GetBestIPAddress() const
	{
		switch (protocolPriority)
		{
			case IPProtocolPriority::IPv6First:
				if (ipv6Status.available && !ipv6Status.globalIP.empty()) return ipv6Status.globalIP;
				if (ipv4Status.available && !ipv4Status.localIP.empty()) return ipv4Status.localIP;
				break;
			case IPProtocolPriority::IPv4First:
				if (ipv4Status.available && !ipv4Status.localIP.empty()) return ipv4Status.localIP;
				if (ipv6Status.available && !ipv6Status.globalIP.empty()) return ipv6Status.globalIP;
				break;
			case IPProtocolPriority::IPv4Only:
				if (ipv4Status.available && !ipv4Status.localIP.empty()) return ipv4Status.localIP;
				break;
			case IPProtocolPriority::IPv6Only:
				if (ipv6Status.available && !ipv6Status.globalIP.empty()) return ipv6Status.globalIP;
				break;
			case IPProtocolPriority::Auto:
				if (ipv6Status.available && ipv6Status.hasGlobalAddress && !ipv6Status.globalIP.empty()) return ipv6Status.globalIP;
				if (ipv4Status.available && ipv4Status.hasPublicIP && !ipv4Status.publicIP.empty()) return ipv4Status.publicIP;
				if (ipv4Status.available && !ipv4Status.localIP.empty()) return ipv4Status.localIP;
				if (ipv6Status.available && !ipv6Status.linkLocalIP.empty()) return ipv6Status.linkLocalIP;
				break;
		}
		return L"";
	}

	bool NetworkInfo::SupportsProtocol(winrt::OpenNet::Models::ConnectionProtocol protocol) const
	{
		return std::find(supportedProtocols.begin(), supportedProtocols.end(), protocol) != supportedProtocols.end();
	}

	void NetworkInfo::AddSupportedProtocol(winrt::OpenNet::Models::ConnectionProtocol protocol)
	{
		if (!SupportsProtocol(protocol)) supportedProtocols.push_back(protocol);
	}

}
