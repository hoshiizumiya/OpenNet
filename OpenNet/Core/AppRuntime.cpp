#include "pch.h"
#include "AppRuntime.h"

#include "Core/Utils/Message.h"
#include <openssl/evp.h>
#include <wil/registry.h>
#include <winrt/Microsoft.Web.WebView2.Core.h>

namespace OpenNet::Core
{
	::OpenNet::Web::WebView2::WebView2Version AppRuntime::WebView2Version()
	{
		return ::OpenNet::Core::AppRuntime::InitializeWebView2();
	}

	winrt::hstring AppRuntime::GetDisplayName()
	{
		return L"OpenNet";
	}

    winrt::hstring AppRuntime::InitializeDeviceId()
    {
        wchar_t userNameBuffer[256];
        DWORD userNameSize = _countof(userNameBuffer);
        GetUserNameW(userNameBuffer, &userNameSize);

        std::wstring userName = userNameBuffer;
        std::wstring machineGuid;

        try
        {
            wil::unique_hkey hKey;
            if (hKey = wil::reg::open_unique_key(
                HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Cryptography",
                wil::reg::key_access::read))
            {
                machineGuid = wil::reg::get_value_string<std::wstring>(
                    hKey.get(),
                    L"MachineGuid");
            }
        }
        catch (...)
        {
            machineGuid = userName;
        }

        std::wstring combined = userName + machineGuid;

        //  转 UTF-8（关键）
        int len = WideCharToMultiByte(CP_UTF8, 0, combined.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::vector<char> utf8(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, combined.c_str(), -1, utf8.data(), len, nullptr, nullptr);

        //  EVP 接口
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        const EVP_MD* md = EVP_blake2b512();

        unsigned char digest[64];
        unsigned int digestLen = 0;

        EVP_DigestInit_ex(ctx, md, nullptr);
        EVP_DigestUpdate(ctx, utf8.data(), utf8.size());
        EVP_DigestFinal_ex(ctx, digest, &digestLen);

        EVP_MD_CTX_free(ctx);

        // hex
        static const char hex[] = "0123456789abcdef";
        std::string out;
        out.resize(digestLen * 2);

        for (unsigned int i = 0; i < digestLen; ++i)
        {
            out[2 * i] = hex[digest[i] >> 4];
            out[2 * i + 1] = hex[digest[i] & 0xF];
        }

        return winrt::to_hstring(out);
    }

    OpenNet::Web::WebView2::WebView2Version AppRuntime::InitializeWebView2()
	{
		try
		{
			winrt::hstring version = winrt::Microsoft::Web::WebView2::Core::CoreWebView2Environment::GetAvailableBrowserVersionString();
			return OpenNet::Web::WebView2::WebView2Version(version, version, true);
		}
		catch (...)
		{
			return OpenNet::Web::WebView2::WebView2Version(winrt::hstring(), ResourceGetString(L"CoreWebView2HelperVersionUndetected"), false);
		}

	}

}