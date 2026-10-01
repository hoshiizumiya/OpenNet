#include "Core/NatMap/MappingFirewall.h"

#include <Windows.h>
#include <netfw.h>
#include <oleauto.h>
#include <memory>
#include <cwchar>
#include <string>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")

namespace OpenNet::Core::NatMap
{
    namespace
    {
        struct ReleaseBstr { void operator()(wchar_t* value) const { SysFreeString(value); } };
        using Bstr = std::unique_ptr<wchar_t, ReleaseBstr>;

        long ProtocolNumber(MappingTransport transport)
        {
            return transport == MappingTransport::Udp ? NET_FW_IP_PROTOCOL_UDP : NET_FW_IP_PROTOCOL_TCP;
        }

        bool OpenPolicy(INetFwPolicy2** policy, INetFwRules** rules, std::wstring& error)
        {
            HRESULT hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER,
                                          __uuidof(INetFwPolicy2), reinterpret_cast<void**>(policy));
            if (SUCCEEDED(hr)) hr = (*policy)->get_Rules(rules);
            if (SUCCEEDED(hr)) return true;
            error = L"Cannot access Windows Firewall policy (HRESULT " + std::to_wstring(static_cast<unsigned long>(hr)) + L").";
            if (*policy) { (*policy)->Release(); *policy = nullptr; }
            return false;
        }

        std::wstring ExecutablePath()
        {
            std::wstring path(32768, L'\0');
            auto const size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (!size || size == path.size()) return {};
            path.resize(size);
            return path;
        }
    }

    MappingFirewallRuleDescriptor BuildMappingFirewallRule(MappingTransport transport, std::uint16_t port)
    {
        auto const portText = std::to_wstring(port);
        return {
            L"OpenNet NAT mapping " + std::wstring(transport == MappingTransport::Udp ? L"UDP " : L"TCP ") + portText,
            portText,
            static_cast<std::int32_t>(ProtocolNumber(transport)),
        };
    }

    static bool AllowInbound(MappingTransport transport, std::uint16_t port, std::wstring& error)
    {
        if (!port) { error = L"Start a mapping before configuring the firewall."; return false; }
        auto const path = ExecutablePath();
        if (path.empty()) { error = L"Cannot locate the OpenNet executable."; return false; }
        INetFwPolicy2* policy{};
        INetFwRules* rules{};
        if (!OpenPolicy(&policy, &rules, error)) return false;

        auto const descriptor = BuildMappingFirewallRule(transport, port);
        Bstr ruleName(SysAllocString(descriptor.name.c_str()));
        Bstr application(SysAllocString(path.c_str()));
        Bstr localPort(SysAllocString(descriptor.localPort.c_str()));
        INetFwRule* rule{};
        HRESULT hr = rules->Item(ruleName.get(), &rule);
        if (SUCCEEDED(hr))
        {
            BSTR existingPath{};
            hr = rule->get_ApplicationName(&existingPath);
            bool const matches = SUCCEEDED(hr) && existingPath && _wcsicmp(existingPath, path.c_str()) == 0;
            SysFreeString(existingPath);
            long protocol{};
            NET_FW_RULE_DIRECTION direction{};
            NET_FW_ACTION action{};
            BSTR existingPort{};
            if (matches) hr = rule->get_Protocol(&protocol);
            if (SUCCEEDED(hr) && matches) hr = rule->get_LocalPorts(&existingPort);
            if (SUCCEEDED(hr) && matches) hr = rule->get_Direction(&direction);
            if (SUCCEEDED(hr) && matches) hr = rule->get_Action(&action);
            bool const exact = matches && SUCCEEDED(hr) && protocol == descriptor.protocol &&
                existingPort && descriptor.localPort == existingPort &&
                direction == NET_FW_RULE_DIR_IN && action == NET_FW_ACTION_ALLOW;
            SysFreeString(existingPort);
            if (exact)
            {
                long profiles{};
                long existingProfiles{};
                hr = policy->get_CurrentProfileTypes(&profiles);
                if (SUCCEEDED(hr)) hr = rule->get_Profiles(&existingProfiles);
                if (SUCCEEDED(hr)) hr = rule->put_Profiles(profiles | existingProfiles);
                if (SUCCEEDED(hr)) hr = rule->put_Enabled(VARIANT_TRUE);
            }
            else { hr = E_ACCESSDENIED; error = L"The firewall rule does not match this OpenNet mapping."; }
        }
        else
        {
            long profiles{};
            hr = policy->get_CurrentProfileTypes(&profiles);
            if (SUCCEEDED(hr)) hr = CoCreateInstance(__uuidof(NetFwRule), nullptr, CLSCTX_INPROC_SERVER,
                                                       __uuidof(INetFwRule), reinterpret_cast<void**>(&rule));
            if (SUCCEEDED(hr)) hr = rule->put_Name(ruleName.get());
            if (SUCCEEDED(hr)) hr = rule->put_ApplicationName(application.get());
            if (SUCCEEDED(hr)) hr = rule->put_Protocol(descriptor.protocol);
            if (SUCCEEDED(hr)) hr = rule->put_LocalPorts(localPort.get());
            if (SUCCEEDED(hr)) hr = rule->put_Direction(NET_FW_RULE_DIR_IN);
            if (SUCCEEDED(hr)) hr = rule->put_Action(NET_FW_ACTION_ALLOW);
            if (SUCCEEDED(hr)) hr = rule->put_Profiles(profiles);
            if (SUCCEEDED(hr)) hr = rule->put_Enabled(VARIANT_TRUE);
            if (SUCCEEDED(hr)) hr = rules->Add(rule);
        }
        if (rule) rule->Release();
        rules->Release();
        policy->Release();
        if (FAILED(hr) && error.empty()) error = L"Windows Firewall rejected the rule (HRESULT " +
            std::to_wstring(static_cast<unsigned long>(hr)) + L"). Run OpenNet with permission to change firewall rules.";
        return SUCCEEDED(hr);
    }

    static bool RemoveInbound(MappingTransport transport, std::uint16_t port, std::wstring& error)
    {
        if (!port) { error = L"No mapped port is selected."; return false; }
        auto const path = ExecutablePath();
        if (path.empty()) { error = L"Cannot locate the OpenNet executable."; return false; }
        INetFwPolicy2* policy{};
        INetFwRules* rules{};
        if (!OpenPolicy(&policy, &rules, error)) return false;
        auto const descriptor = BuildMappingFirewallRule(transport, port);
        Bstr ruleName(SysAllocString(descriptor.name.c_str()));
        INetFwRule* rule{};
        HRESULT hr = rules->Item(ruleName.get(), &rule);
        if (SUCCEEDED(hr))
        {
            BSTR existingPath{};
            hr = rule->get_ApplicationName(&existingPath);
            bool const matches = SUCCEEDED(hr) && existingPath && _wcsicmp(existingPath, path.c_str()) == 0;
            SysFreeString(existingPath);
            long protocol{};
            NET_FW_RULE_DIRECTION direction{};
            NET_FW_ACTION action{};
            BSTR existingPort{};
            if (matches) hr = rule->get_Protocol(&protocol);
            if (SUCCEEDED(hr) && matches) hr = rule->get_LocalPorts(&existingPort);
            if (SUCCEEDED(hr) && matches) hr = rule->get_Direction(&direction);
            if (SUCCEEDED(hr) && matches) hr = rule->get_Action(&action);
            bool const exact = matches && SUCCEEDED(hr) && protocol == descriptor.protocol &&
                existingPort && descriptor.localPort == existingPort &&
                direction == NET_FW_RULE_DIR_IN && action == NET_FW_ACTION_ALLOW;
            SysFreeString(existingPort);
            if (exact) hr = rules->Remove(ruleName.get());
            else { hr = E_ACCESSDENIED; error = L"The firewall rule does not match this OpenNet mapping."; }
            rule->Release();
        }
        rules->Release();
        policy->Release();
        if (FAILED(hr) && error.empty()) error = L"Could not remove the OpenNet firewall rule.";
        return SUCCEEDED(hr);
    }

    bool AllowInboundUdp(std::uint16_t port, std::wstring& error)
    {
        return AllowInbound(MappingTransport::Udp, port, error);
    }

    bool RemoveInboundUdp(std::uint16_t port, std::wstring& error)
    {
        return RemoveInbound(MappingTransport::Udp, port, error);
    }

    bool AllowInboundTcp(std::uint16_t port, std::wstring& error)
    {
        return AllowInbound(MappingTransport::Tcp, port, error);
    }

    bool RemoveInboundTcp(std::uint16_t port, std::wstring& error)
    {
        return RemoveInbound(MappingTransport::Tcp, port, error);
    }
}
