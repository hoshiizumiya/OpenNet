#include "pch.h"
#include "FirewallRuleManager.h"
#include <sddl.h>
#include <icftypes.h>
#include <netfw.h>

FirewallRuleManager::FirewallRuleManager()
	: m_policy(nullptr)
	, m_rules(nullptr)
	, m_initialized(false)
{
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
}

FirewallRuleManager::~FirewallRuleManager()
{
	SafeRelease(m_rules);
	SafeRelease(m_policy);
	CoUninitialize();
}

HRESULT FirewallRuleManager::Initialize()
{
	if (m_initialized)
		return S_OK;

	HRESULT hr = CoCreateInstance(
		__uuidof(NetFwPolicy2),
		nullptr,
		CLSCTX_INPROC_SERVER,
		__uuidof(INetFwPolicy2),
		reinterpret_cast<void**>(&m_policy)
	);

	if (FAILED(hr))
		return hr;

	hr = m_policy->get_Rules(&m_rules);
	if (FAILED(hr))
	{
		SafeRelease(m_policy);
		return hr;
	}

	m_initialized = true;
	return S_OK;
}

HRESULT FirewallRuleManager::IsLoopbackExempt(const std::wstring_view& familyName, const std::wstring_view& sid, BOOL* enabled)
{
	if (!enabled)
		return E_INVALIDARG;

	*enabled = FALSE;

	HRESULT hr = Initialize();
	if (FAILED(hr))
		return hr;

	std::wstring ruleName = GetRuleName(familyName, sid).data();
	INetFwRule* rule = nullptr;
	hr = m_rules->Item(BSTR(ruleName.data()), &rule);

	if (SUCCEEDED(hr) && rule)
	{
		VARIANT_BOOL ruleEnabled = VARIANT_FALSE;
		hr = rule->get_Enabled(&ruleEnabled);
		if (SUCCEEDED(hr))
		{
			*enabled = (ruleEnabled == VARIANT_TRUE) ? TRUE : FALSE;
		}
		SafeRelease(rule);
	}

	return S_OK;
}

HRESULT FirewallRuleManager::AddLoopbackExempt(const std::wstring_view& familyName, const std::wstring_view& sid)
{
	HRESULT hr = Initialize();
	if (FAILED(hr))
		return hr;

	std::wstring_view ruleName = GetRuleName(familyName, sid);

	// 检查规则是否已存在
	INetFwRule* existingRule = nullptr;
	hr = m_rules->Item(BSTR(ruleName.data()), &existingRule);

	if (SUCCEEDED(hr) && existingRule)
	{
		// 规则已存在，启用它
		hr = existingRule->put_Enabled(VARIANT_TRUE);
		SafeRelease(existingRule);
		return hr;
	}

	SafeRelease(existingRule);

	// 创建新规则
	return CreateFirewallRule(
		ruleName,
		L"loopback",
		sid,
		NET_FW_RULE_DIR_IN,
		NET_FW_ACTION_ALLOW
	);
}

HRESULT FirewallRuleManager::IsPublicFirewallEnabled(BOOL* enabled)
{
	if (!enabled)
		return E_INVALIDARG;

	*enabled = FALSE;

	HRESULT hr = Initialize();
	if (FAILED(hr))
		return hr;

	NET_FW_PROFILE_TYPE2 profileType = NET_FW_PROFILE2_PUBLIC;
	VARIANT_BOOL firewallEnabled = VARIANT_FALSE;

	hr = m_policy->get_FirewallEnabled(profileType, &firewallEnabled);
	if (SUCCEEDED(hr))
	{
		*enabled = (firewallEnabled == VARIANT_TRUE) ? TRUE : FALSE;
	}

	return hr;
}

HRESULT FirewallRuleManager::CreateFirewallRule(const std::wstring_view& ruleName, const std::wstring_view& appPath,
												const std::wstring_view& sid, NET_FW_RULE_DIRECTION direction,
												NET_FW_ACTION action)
{
	INetFwRule* rule = nullptr;
	std::wstring groupName = L"HutaoLoopback";

	HRESULT hr = CoCreateInstance(
		__uuidof(NetFwRule),
		nullptr,
		CLSCTX_INPROC_SERVER,
		__uuidof(INetFwRule),
		reinterpret_cast<void**>(&rule)
	);

	if (FAILED(hr))
		return hr;

	// 设置规则属性
	hr = rule->put_Name(BSTR(ruleName.data()));
	if (FAILED(hr)) goto cleanup;

	hr = rule->put_Description(BSTR(L"Hutao Game Launcher Loopback Exemption"));
	if (FAILED(hr)) goto cleanup;

	hr = rule->put_ApplicationName(BSTR(appPath.data()));
	if (FAILED(hr)) goto cleanup;

	hr = rule->put_Protocol(NET_FW_IP_PROTOCOL_ANY);
	if (FAILED(hr)) goto cleanup;

	hr = rule->put_Direction(direction);
	if (FAILED(hr)) goto cleanup;

	hr = rule->put_Action(action);
	if (FAILED(hr)) goto cleanup;

	hr = rule->put_Enabled(VARIANT_TRUE);
	if (FAILED(hr)) goto cleanup;

	hr = rule->put_LocalAddresses(BSTR(L"127.0.0.1"));
	if (FAILED(hr)) goto cleanup;

	hr = rule->put_LocalPorts(BSTR(L"*"));
	if (FAILED(hr)) goto cleanup;

	if (!sid.empty())
	{
		groupName += L"_" + std::wstring(sid);
	}
	rule->put_Grouping(BSTR(groupName.data()));

	// 将规则添加到集合中
	hr = m_rules->Add(rule);

cleanup:
	SafeRelease(rule);
	return hr;
}

std::wstring_view FirewallRuleManager::GetRuleName(const std::wstring_view& familyName, const std::wstring_view& sid)
{
	std::wstring ruleName = L"Hutao_Loopback_" + std::wstring(familyName);

	if (!sid.empty())
	{
		ruleName += L"_" + std::wstring(sid);
	}

	return ruleName;
}

std::wstring_view FirewallRuleManager::GetCurrentUserSid()
{
	std::wstring_view sidString;
	HANDLE hToken = nullptr;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		return sidString;

	// 获取所需缓冲区大小
	DWORD tokenSize = 0;
	GetTokenInformation(hToken, TokenUser, nullptr, 0, &tokenSize);

	if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		std::vector<BYTE> buffer(tokenSize);
		PTOKEN_USER tokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());

		if (GetTokenInformation(hToken, TokenUser, tokenUser, tokenSize, &tokenSize))
		{
			LPWSTR sidStr = nullptr;
			if (ConvertSidToStringSidW(tokenUser->User.Sid, &sidStr))
			{
				sidString = sidStr;
				LocalFree(sidStr);
			}
		}
	}

	CloseHandle(hToken);
	return sidString;
}