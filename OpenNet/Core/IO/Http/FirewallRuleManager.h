#pragma once

#include <Windows.h>
#include <netfw.h>
#include <icftypes.h>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")

class FirewallRuleManager
{
public:
    FirewallRuleManager();
    ~FirewallRuleManager();

    HRESULT Initialize();
    HRESULT IsLoopbackExempt(const std::wstring_view& familyName, const std::wstring_view& sid, BOOL* enabled);
    HRESULT AddLoopbackExempt(const std::wstring_view& familyName, const std::wstring_view& sid);
    HRESULT IsPublicFirewallEnabled(BOOL* enabled);

private:
    INetFwPolicy2* m_policy;
    INetFwRules* m_rules;
    bool m_initialized;
    HRESULT CreateFirewallRule(const std::wstring_view& ruleName, const std::wstring_view& appPath,
                               const std::wstring_view& sid, NET_FW_RULE_DIRECTION direction,
                               NET_FW_ACTION action);
    std::wstring_view GetRuleName(const std::wstring_view& familyName, const std::wstring_view& sid);
    std::wstring_view GetCurrentUserSid();

    // 安全释放COM对象
    template<typename T>
    void SafeRelease(T*& p)
    {
        if (p)
        {
            p->Release();
            p = nullptr;
        }
    }
};