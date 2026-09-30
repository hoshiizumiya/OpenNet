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
        void Refresh();

    private:
        winrt::fire_and_forget ChangeMappingAsync(bool start, std::uint16_t target);
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
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
        winrt::event_token m_tickToken{};
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct NatMappingViewModel : NatMappingViewModelT<NatMappingViewModel, implementation::NatMappingViewModel> {};
}
