#pragma once
#include "UI/Xaml/View/Pages/NatToolsPage.g.h"
#include "Core/NetworkDetector.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
    struct NatToolsPage : NatToolsPageT<NatToolsPage>
    {
        NatToolsPage();

        winrt::Windows::Foundation::IAsyncAction DetectNat_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        winrt::Windows::Foundation::IAsyncAction TestPort_Click(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        ::OpenNet::Core::NetworkDetector m_detector;

        // Perform individual STUN binding test and return mapped IP:port and latency
        struct StunTestResult
        {
            bool success{false};
            winrt::hstring mappedAddress;
            int latencyMs{0};
        };
        winrt::Windows::Foundation::IAsyncAction PerformStunTest(
            winrt::hstring const& server, uint16_t port, std::shared_ptr<StunTestResult> outResult);
        void UpdateLibtorrentStatus();
    };
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
    struct NatToolsPage : NatToolsPageT<NatToolsPage, implementation::NatToolsPage>
    {
    };
}
