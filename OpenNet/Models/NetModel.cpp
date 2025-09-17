#include "pch.h"
#include "NetModel.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

namespace winrt::OpenNet::Models::implementation
{
    NetModel::NetModel()
    {
        m_protocols = single_threaded_vector<winrt::OpenNet::Models::ConnectionProtocol>();
        // Populate supported protocols - minimal set for now
        m_protocols.Append(winrt::OpenNet::Models::ConnectionProtocol::Auto);
        m_protocols.Append(winrt::OpenNet::Models::ConnectionProtocol::TCP);
        m_protocols.Append(winrt::OpenNet::Models::ConnectionProtocol::UDP);
        m_protocols.Append(winrt::OpenNet::Models::ConnectionProtocol::UTP);
        m_protocols.Append(winrt::OpenNet::Models::ConnectionProtocol::BitTorrent);
        m_protocols.Append(winrt::OpenNet::Models::ConnectionProtocol::DHT);
        m_protocols.Append(winrt::OpenNet::Models::ConnectionProtocol::WebRTC);
        m_protocols.Append(winrt::OpenNet::Models::ConnectionProtocol::HTTP);

        m_preferred = winrt::OpenNet::Models::ConnectionProtocol::Auto;
    }

    IVectorView<winrt::OpenNet::Models::ConnectionProtocol> NetModel::SupportedProtocols()
    {
        return m_protocols.GetView();
    }

    winrt::OpenNet::Models::ConnectionProtocol NetModel::PreferredProtocol()
    {
        return m_preferred;
    }

    void NetModel::PreferredProtocol(winrt::OpenNet::Models::ConnectionProtocol const& value)
    {
        m_preferred = value;
    }
}
