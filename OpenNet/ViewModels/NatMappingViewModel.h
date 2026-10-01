#pragma once

#include "ViewModels/NatMappingViewModel.g.h"
#include <cstdint>

import OpenNet.ViewModels.ObservableMixin;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml.Input;

namespace winrt::OpenNet::ViewModels::implementation
{
    struct NatMappingViewModel : NatMappingViewModelT<NatMappingViewModel>,
        ::OpenNet::ViewModels::ObservableMixin<NatMappingViewModel>
    {
        NatMappingViewModel();
        ~NatMappingViewModel();

        double MappingPort() const noexcept { return m_mappingPort; }
        void MappingPort(double value) { SetProperty(m_mappingPort, value, L"MappingPort"); }
        double TargetPort() const noexcept { return m_targetPort; }
        void TargetPort(double value) { SetProperty(m_targetPort, value, L"TargetPort"); }
        winrt::hstring StunHost() const { return m_stunHost; }
        void StunHost(winrt::hstring const& value) { SetProperty(m_stunHost, value, L"StunHost"); }
        double StunPort() const noexcept { return m_stunPort; }
        void StunPort(double value) { SetProperty(m_stunPort, value, L"StunPort"); }
        double ProbePort() const noexcept { return m_probePort; }
        void ProbePort(double value) { SetProperty(m_probePort, value, L"ProbePort"); }
        winrt::hstring Status() const { return m_status; }
        winrt::hstring PublicEndpoint() const { return m_publicEndpoint; }
        winrt::hstring FirewallStatus() const { return m_firewallStatus; }
        bool IsBusy() const noexcept { return m_isBusy; }
        bool IsRunning() const noexcept { return m_isRunning; }
        winrt::Microsoft::UI::Xaml::Input::ICommand StartCommand() const { return m_startCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand StopCommand() const { return m_stopCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand AllowFirewallCommand() const { return m_allowFirewallCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand RemoveFirewallCommand() const { return m_removeFirewallCommand; }
        winrt::hstring TcpLocalAddress() const { return m_tcpLocalAddress; }
        void TcpLocalAddress(winrt::hstring const& value) { SetProperty(m_tcpLocalAddress, value, L"TcpLocalAddress"); }
        double TcpMappingPort() const noexcept { return m_tcpMappingPort; }
        void TcpMappingPort(double value) { SetProperty(m_tcpMappingPort, value, L"TcpMappingPort"); }
        double TcpTargetPort() const noexcept { return m_tcpTargetPort; }
        void TcpTargetPort(double value) { SetProperty(m_tcpTargetPort, value, L"TcpTargetPort"); }
        winrt::hstring TcpKeepaliveHost() const { return m_tcpKeepaliveHost; }
        void TcpKeepaliveHost(winrt::hstring const& value) { SetProperty(m_tcpKeepaliveHost, value, L"TcpKeepaliveHost"); }
        double TcpKeepalivePort() const noexcept { return m_tcpKeepalivePort; }
        void TcpKeepalivePort(double value) { SetProperty(m_tcpKeepalivePort, value, L"TcpKeepalivePort"); }
        winrt::hstring TcpStunHost() const { return m_tcpStunHost; }
        void TcpStunHost(winrt::hstring const& value) { SetProperty(m_tcpStunHost, value, L"TcpStunHost"); }
        double TcpStunPort() const noexcept { return m_tcpStunPort; }
        void TcpStunPort(double value) { SetProperty(m_tcpStunPort, value, L"TcpStunPort"); }
        winrt::hstring TcpStatus() const { return m_tcpStatus; }
        winrt::hstring TcpPublicEndpoint() const { return m_tcpPublicEndpoint; }
        bool IsTcpBusy() const noexcept { return m_isTcpBusy; }
        bool IsTcpRunning() const noexcept { return m_isTcpRunning; }
        winrt::Microsoft::UI::Xaml::Input::ICommand StartTcpCommand() const { return m_startTcpCommand; }
        winrt::Microsoft::UI::Xaml::Input::ICommand StopTcpCommand() const { return m_stopTcpCommand; }
        void Refresh();

    private:
        winrt::fire_and_forget ChangeMappingAsync(bool start, std::uint16_t target, bool followTorrentTarget = false);
        winrt::fire_and_forget ChangeTcpMappingAsync(bool start);
        bool m_isBusy{};
        double m_mappingPort{ 0 };
        double m_targetPort{ 0 };
        winrt::hstring m_stunHost{ L"turn.cloudflare.com" };
        double m_stunPort{ 3478 };
        double m_probePort{ 6881 };
        std::uint16_t m_lastBoundPort{};
        bool m_isRunning{};
        winrt::hstring m_status;
        winrt::hstring m_publicEndpoint;
        winrt::hstring m_firewallStatus;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_startCommand{ nullptr };
        winrt::Microsoft::UI::Xaml::Input::ICommand m_stopCommand{ nullptr };
        winrt::Microsoft::UI::Xaml::Input::ICommand m_allowFirewallCommand{ nullptr };
        winrt::Microsoft::UI::Xaml::Input::ICommand m_removeFirewallCommand{ nullptr };
        bool m_isTcpBusy{};
        bool m_isTcpRunning{};
        winrt::hstring m_tcpLocalAddress;
        double m_tcpMappingPort{};
        double m_tcpTargetPort{ 8080 };
        winrt::hstring m_tcpKeepaliveHost{ L"www.cloudflare.com" };
        double m_tcpKeepalivePort{ 80 };
        winrt::hstring m_tcpStunHost{ L"turn.cloudflare.com" };
        double m_tcpStunPort{ 3478 };
        winrt::hstring m_tcpStatus;
        winrt::hstring m_tcpPublicEndpoint;
        winrt::Microsoft::UI::Xaml::Input::ICommand m_startTcpCommand{ nullptr };
        winrt::Microsoft::UI::Xaml::Input::ICommand m_stopTcpCommand{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
        winrt::event_token m_tickToken{};
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct NatMappingViewModel : NatMappingViewModelT<NatMappingViewModel, implementation::NatMappingViewModel> {};
}
