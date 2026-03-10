#include "pch.h"
#include "NetworkInfo.h"

#include <algorithm>

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
        case ConnectionStatus::Disconnected: return L"断开连接 / Disconnected";
        case ConnectionStatus::Connecting: return L"连接中 / Connecting";
        case ConnectionStatus::Connected: return L"已连接 / Connected";
        case ConnectionStatus::Limited: return L"受限连接 / Limited";
        case ConnectionStatus::NoInternet: return L"无网络 / No Internet";
        default: return L"未知 / Unknown";
        }
    }

    hstring NetworkInfo::GetNATTypeString() const
    {
        switch (natType)
        {
        case NATType::Unknown: return L"未知 / Unknown";
        case NATType::Open: return L"开放 / Open";
        case NATType::FullCone: return L"完全锥型 / Full Cone";
        case NATType::RestrictedCone: return L"受限锥型 / Restricted Cone";
        case NATType::PortRestricted: return L"端口受限 / Port Restricted";
        case NATType::Symmetric: return L"对称型 / Symmetric";
        case NATType::Blocked: return L"被阻塞 / Blocked";
        default: return L"未知 / Unknown";
        }
    }

    hstring NetworkInfo::GetProtocolPriorityString() const
    {
        switch (protocolPriority)
        {
        case IPProtocolPriority::IPv4First: return L"IPv4优先 / IPv4 First";
        case IPProtocolPriority::IPv6First: return L"IPv6优先 / IPv6 First";
        case IPProtocolPriority::IPv4Only: return L"仅IPv4 / IPv4 Only";
        case IPProtocolPriority::IPv6Only: return L"仅IPv6 / IPv6 Only";
        case IPProtocolPriority::Auto: return L"自动选择 / Auto Select";
        default: return L"未知 / Unknown";
        }
    }

    hstring NetworkInfo::GetPreferredProtocolString() const
    {
        switch (preferredProtocol)
        {
        case ConnectionProtocol::Auto: return L"自动选择 / Auto Select";
        case ConnectionProtocol::TCP: return L"TCP协议 / TCP Protocol";
        case ConnectionProtocol::UDP: return L"UDP协议 / UDP Protocol";
        case ConnectionProtocol::UTP: return L"uTP协议 / uTP Protocol";
        case ConnectionProtocol::BitTorrent: return L"BitTorrent协议 / BitTorrent Protocol";
        case ConnectionProtocol::DHT: return L"DHT网络 / DHT Network";
        case ConnectionProtocol::WebRTC: return L"WebRTC协议 / WebRTC Protocol";
        case ConnectionProtocol::HTTP: return L"HTTP协议 / HTTP Protocol";
        default: return L"未知 / Unknown";
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
