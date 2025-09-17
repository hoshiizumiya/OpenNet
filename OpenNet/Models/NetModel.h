#pragma once
#include "Models/NetModel.g.h"
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::OpenNet::Models::implementation
{
    struct NetModel : NetModelT<NetModel>
    {
        NetModel();

        // Properties projected from IDL
        winrt::Windows::Foundation::Collections::IVectorView<winrt::OpenNet::Models::ConnectionProtocol> SupportedProtocols();
        winrt::OpenNet::Models::ConnectionProtocol PreferredProtocol();
        void PreferredProtocol(winrt::OpenNet::Models::ConnectionProtocol const& value);

    private:
        winrt::Windows::Foundation::Collections::IVector<winrt::OpenNet::Models::ConnectionProtocol> m_protocols{ nullptr };
        winrt::OpenNet::Models::ConnectionProtocol m_preferred{ winrt::OpenNet::Models::ConnectionProtocol::Auto };
    };
}

namespace winrt::OpenNet::Models::factory_implementation
{
    struct NetModel : NetModelT<NetModel, implementation::NetModel> {};
}
