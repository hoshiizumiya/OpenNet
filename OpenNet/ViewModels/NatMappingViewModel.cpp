#include "XamlWorkaround.h"
#include "ViewModels/NatMappingViewModel.h"
#include "ViewModels/NatMappingViewModel.g.cpp"
#include "Core/NatMap/TcpMappingService.h"
#include "Core/NatMap/UdpMappingService.h"
#include "Core/NatMap/MappingFirewall.h"
#include "mvvm_framework/delegate_command_builder.h"
#include <chrono>
#include <cmath>
#include <string>
#include <utility>

import OpenNet.Core.P2PManager;
import OpenNet.Core.Utils.Message;
import winrtplus_coroutine;
import winrt.Microsoft.UI.Dispatching;

namespace winrt::OpenNet::ViewModels::implementation
{
    NatMappingViewModel::NatMappingViewModel()
    {
        m_status = ResourceGetString(L"NatMappingStopped");
        m_publicEndpoint = L"—";
        m_firewallStatus = ResourceGetString(L"NatMappingFirewallUnchanged");
        m_tcpStatus = ResourceGetString(L"NatTcpMappingStopped");
        m_tcpPublicEndpoint = L"—";
        m_startCommand = mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](auto const&)
        {
            if (auto self = weak.get())
            {
                bool const followTorrentTarget = self->m_targetPort == 0;
                std::uint16_t target{};
                if (followTorrentTarget)
                {
                    auto& manager = ::OpenNet::Core::P2PManager::Instance();
                    if (manager.IsTorrentCoreInitialized())
                    {
                        auto const status = manager.GetListenStatus();
                        if (status.ipv4UtpPort > 0 && status.ipv4UtpPort <= 65535)
                        {
                            target = static_cast<std::uint16_t>(status.ipv4UtpPort);
                        }
                    }
                }
                if (!std::isfinite(self->m_mappingPort) || self->m_mappingPort < 0 || self->m_mappingPort > 65535 ||
                    std::floor(self->m_mappingPort) != self->m_mappingPort ||
                    (!followTorrentTarget && (!std::isfinite(self->m_targetPort) || self->m_targetPort < 1 ||
                        self->m_targetPort > 65535 || std::floor(self->m_targetPort) != self->m_targetPort)) ||
                    self->m_stunHost.empty() || self->m_stunHost.size() > 253 ||
                    !std::isfinite(self->m_stunPort) || self->m_stunPort < 1 || self->m_stunPort > 65535 ||
                    std::floor(self->m_stunPort) != self->m_stunPort)
                {
                    self->SetProperty(self->m_status, ResourceGetString(L"NatMappingInvalidConfig"), L"Status");
                    return;
                }
                if (!followTorrentTarget)
                {
                    target = static_cast<std::uint16_t>(self->m_targetPort);
                }
                if (self->m_mappingPort != 0 && target != 0 && self->m_mappingPort == target)
                {
                    self->SetProperty(self->m_status, ResourceGetString(L"NatMappingInvalidConfig"), L"Status");
                    return;
                }
                self->ChangeMappingAsync(true, target, followTorrentTarget);
            }
        }).CanExecute([weak = get_weak()](auto const&)
        {
            auto self = weak.get();
            return self && !self->m_isBusy && !self->m_isRunning;
        }).DependsOn(L"IsBusy").DependsOn(L"IsRunning").Build();
        m_stopCommand = mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](auto const&)
        {
            if (auto self = weak.get()) self->ChangeMappingAsync(false, 0);
        }).CanExecute([weak = get_weak()](auto const&)
        {
            auto self = weak.get();
            return self && !self->m_isBusy && self->m_isRunning;
        }).DependsOn(L"IsBusy").DependsOn(L"IsRunning").Build();

        m_allowFirewallCommand = mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](auto const&)
        {
            if (auto self = weak.get())
            {
                std::wstring error;
                auto const mapping = ::OpenNet::Core::NatMap::SharedUdpMappingService().Snapshot();
                auto const port = mapping.localPort;
                if (!mapping.running || self->m_mappingPort == 0 || self->m_mappingPort != port)
                {
                    self->SetProperty(self->m_firewallStatus, ResourceGetString(L"NatMappingFirewallRequiresFixedPort"), L"FirewallStatus");
                    return;
                }
                bool const applied = ::OpenNet::Core::NatMap::AllowInboundUdp(port, error);
                self->SetProperty(self->m_firewallStatus,
                    applied ? ResourceGetString(L"NatMappingFirewallAllowed") : winrt::hstring(error), L"FirewallStatus");
            }
        }).Build();
        m_removeFirewallCommand = mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](auto const&)
        {
            if (auto self = weak.get())
            {
                std::wstring error;
                auto const configured = self->m_mappingPort;
                auto const port = std::isfinite(configured) && configured >= 1 && configured <= 65535 &&
                    std::floor(configured) == configured ? static_cast<std::uint16_t>(configured) :
                    ::OpenNet::Core::NatMap::SharedUdpMappingService().Snapshot().localPort;
                bool const removed = ::OpenNet::Core::NatMap::RemoveInboundUdp(port, error);
                self->SetProperty(self->m_firewallStatus,
                    removed ? ResourceGetString(L"NatMappingFirewallRemoved") : winrt::hstring(error), L"FirewallStatus");
            }
        }).Build();

        m_startTcpCommand = mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](auto const&)
        {
            if (auto self = weak.get())
            {
                auto validPort = [](double value, bool allowZero)
                {
                    return std::isfinite(value) && value >= (allowZero ? 0 : 1) && value <= 65535 &&
                        std::floor(value) == value;
                };
                if (self->m_tcpLocalAddress.empty() || self->m_tcpLocalAddress.size() > 45 ||
                    !validPort(self->m_tcpMappingPort, true) || !validPort(self->m_tcpTargetPort, false) ||
                    self->m_tcpKeepaliveHost.empty() || self->m_tcpKeepaliveHost.size() > 253 ||
                    !validPort(self->m_tcpKeepalivePort, false) ||
                    self->m_tcpStunHost.empty() || self->m_tcpStunHost.size() > 253 ||
                    !validPort(self->m_tcpStunPort, false))
                {
                    self->SetProperty(self->m_tcpStatus, ResourceGetString(L"NatTcpMappingInvalidConfig"), L"TcpStatus");
                    return;
                }
                self->ChangeTcpMappingAsync(true);
            }
        }).CanExecute([weak = get_weak()](auto const&)
        {
            auto self = weak.get();
            return self && !self->m_isTcpBusy && !self->m_isTcpRunning;
        }).DependsOn(L"IsTcpBusy").DependsOn(L"IsTcpRunning").Build();
        m_stopTcpCommand = mvvm::DelegateCommandBuilder<winrt::Windows::Foundation::IInspectable>(*this)
            .Execute([weak = get_weak()](auto const&)
        {
            if (auto self = weak.get()) self->ChangeTcpMappingAsync(false);
        }).CanExecute([weak = get_weak()](auto const&)
        {
            auto self = weak.get();
            return self && !self->m_isTcpBusy && self->m_isTcpRunning;
        }).DependsOn(L"IsTcpBusy").DependsOn(L"IsTcpRunning").Build();

        m_timer = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread().CreateTimer();
        m_timer.Interval(std::chrono::seconds(1));
        m_tickToken = m_timer.Tick([weak = get_weak()](auto&&, auto&&)
        {
            if (auto self = weak.get()) self->Refresh();
        });
        m_timer.Start();
        Refresh();
    }

    NatMappingViewModel::~NatMappingViewModel()
    {
        if (m_timer)
        {
            m_timer.Stop();
            m_timer.Tick(m_tickToken);
        }
    }

    winrt::fire_and_forget NatMappingViewModel::ChangeMappingAsync(
        bool start, std::uint16_t target, bool followTorrentTarget)
    {
        if (m_isBusy) co_return;
        auto lifetime = get_strong();
        auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        auto const port = start ? static_cast<std::uint16_t>(m_mappingPort) : std::uint16_t{};
        auto const stunPort = start ? static_cast<std::uint16_t>(m_stunPort) : std::uint16_t{};
        auto const host = std::wstring(m_stunHost.c_str());
        SetProperty(m_isBusy, true, L"IsBusy");
        if (start) SetProperty(m_status, ResourceGetString(L"NatMappingStarting"), L"Status");
        co_await winrt::resume_background();
        bool failed{};
        try
        {
            auto& service = ::OpenNet::Core::NatMap::SharedUdpMappingService();
            if (start) failed = !service.Start(port, target, host, stunPort, followTorrentTarget);
            else service.Stop();
        }
        catch (...) { failed = true; }
        co_await winrtplus::resume_foreground(dispatcher);
        SetProperty(m_isBusy, false, L"IsBusy");
        SetProperty(m_status, ResourceGetString(failed ? L"NatMappingInvalidConfig" :
            start ? L"NatMappingStarting" : L"NatMappingStopped"), L"Status");
        Refresh();
    }

    winrt::fire_and_forget NatMappingViewModel::ChangeTcpMappingAsync(bool start)
    {
        if (m_isTcpBusy) co_return;
        auto lifetime = get_strong();
        auto const dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        ::OpenNet::Core::NatMap::TcpMappingOptions options;
        if (start)
        {
            options.localAddress = std::wstring(m_tcpLocalAddress.c_str());
            options.localPort = static_cast<std::uint16_t>(m_tcpMappingPort);
            options.targetPort = static_cast<std::uint16_t>(m_tcpTargetPort);
            options.keepaliveHost = std::wstring(m_tcpKeepaliveHost.c_str());
            options.keepalivePort = static_cast<std::uint16_t>(m_tcpKeepalivePort);
            options.stunHost = std::wstring(m_tcpStunHost.c_str());
            options.stunPort = static_cast<std::uint16_t>(m_tcpStunPort);
        }
        SetProperty(m_isTcpBusy, true, L"IsTcpBusy");
        if (start) SetProperty(m_tcpStatus, ResourceGetString(L"NatTcpMappingStarting"), L"TcpStatus");
        co_await winrt::resume_background();
        bool failed{};
        try
        {
            auto& service = ::OpenNet::Core::NatMap::SharedTcpMappingService();
            if (start) failed = !service.Start(std::move(options));
            else service.Stop();
        }
        catch (...) { failed = true; }
        co_await winrtplus::resume_foreground(dispatcher);
        SetProperty(m_isTcpBusy, false, L"IsTcpBusy");
        SetProperty(m_tcpStatus, ResourceGetString(failed ? L"NatTcpMappingInvalidConfig" :
            start ? L"NatTcpMappingStarting" : L"NatTcpMappingStopped"), L"TcpStatus");
        Refresh();
    }

    void NatMappingViewModel::Refresh()
    {
        auto const snapshot = ::OpenNet::Core::NatMap::SharedUdpMappingService().Snapshot();
        SetProperty(m_isRunning, snapshot.running || snapshot.starting, L"IsRunning");
        if (snapshot.running)
        {
            // Keep the probe wired to the actual bound port, including OS allocated ports.
            if (m_lastBoundPort != snapshot.localPort)
            {
                ProbePort(static_cast<double>(snapshot.localPort));
                m_lastBoundPort = snapshot.localPort;
            }
            SetProperty(m_publicEndpoint, snapshot.publicAddress.empty() ? ResourceGetString(L"NatMappingWaitingStun") :
                winrt::hstring(snapshot.publicAddress + L":" + std::to_wstring(snapshot.publicPort)), L"PublicEndpoint");
            winrt::hstring status;
            if (!snapshot.error.empty()) status = winrt::hstring(snapshot.error);
            else if (!snapshot.targetError.empty()) status = winrt::hstring(snapshot.targetError);
            else if (snapshot.followsTorrentTarget && !snapshot.targetAvailable)
                status = ResourceGetString(L"NatMappingWaitingTarget");
            else if (snapshot.followsTorrentTarget)
                status = ResourceGetString(L"ConnProtocol_UTP") + L" → 127.0.0.1:" +
                    winrt::to_hstring(snapshot.targetPort);
            else status = ResourceGetString(L"NatMappingForwardingPrefix") + winrt::to_hstring(snapshot.targetPort);
            SetProperty(m_status, status, L"Status");
        }
        else
        {
            m_lastBoundPort = 0;
            SetProperty(m_publicEndpoint, winrt::hstring(L"—"), L"PublicEndpoint");
            if (snapshot.starting) SetProperty(m_status, ResourceGetString(L"NatMappingStarting"), L"Status");
            else if (!snapshot.error.empty()) SetProperty(m_status, winrt::hstring(snapshot.error), L"Status");
        }

        auto const tcp = ::OpenNet::Core::NatMap::SharedTcpMappingService().Snapshot();
        SetProperty(m_isTcpRunning, tcp.running || tcp.starting, L"IsTcpRunning");
        if (tcp.running)
        {
            SetProperty(m_tcpPublicEndpoint, tcp.publicAddress.empty() ? ResourceGetString(L"NatMappingWaitingStun") :
                winrt::hstring(tcp.publicAddress + L":" + std::to_wstring(tcp.publicPort)), L"TcpPublicEndpoint");
            SetProperty(m_tcpStatus, !tcp.error.empty() ? winrt::hstring(tcp.error) :
                ResourceGetString(L"NatTcpMappingForwardingPrefix") + winrt::to_hstring(tcp.targetPort) +
                ResourceGetString(L"NatTcpMappingActiveRelaysPrefix") +
                winrt::hstring(std::to_wstring(tcp.activeRelays)), L"TcpStatus");
        }
        else
        {
            SetProperty(m_tcpPublicEndpoint, winrt::hstring(L"—"), L"TcpPublicEndpoint");
            if (tcp.starting) SetProperty(m_tcpStatus, ResourceGetString(L"NatTcpMappingStarting"), L"TcpStatus");
            else if (!tcp.error.empty()) SetProperty(m_tcpStatus, winrt::hstring(tcp.error), L"TcpStatus");
        }
    }
}
