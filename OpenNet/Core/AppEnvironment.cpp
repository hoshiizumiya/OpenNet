#include "pch.h"
#include "AppEnvironment.h"
#include <windows.h>
#include <appmodel.h>

#pragma comment(lib, "kernel32.lib")

namespace winrt::OpenNet::Core
{
	// 全局变量初始化
	bool g_isAppContainer = false;
	bool AppEnvironment::s_isAppContainer = false;
	bool AppEnvironment::s_initialized = false;

	void AppEnvironment::Initialize()
	{
		if (s_initialized)
			return;

		s_initialized = true;

		try
		{
			// 检测是否在AppContainer中
			HANDLE token = nullptr;
			if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
			{
				DWORD size = 0;
				GetTokenInformation(token, TokenIsAppContainer, nullptr, 0, &size);

				if (size > 0)
				{
					DWORD* isAppContainer = new DWORD(0);
					if (GetTokenInformation(token, TokenIsAppContainer, isAppContainer, sizeof(DWORD), &size))
					{
						s_isAppContainer = (*isAppContainer != 0);
						g_isAppContainer = s_isAppContainer;
					}
					delete isAppContainer;
				}

				CloseHandle(token);
			}
		}
		catch (...)
		{
			s_isAppContainer = false;
		}
	}

	bool AppEnvironment::IsRunningInAppContainer()
	{
		if (!s_initialized)
		{
			Initialize();
		}
		return s_isAppContainer;
	}

	bool AppEnvironment::HasPackageIdentity()
	{
		try
		{
			UINT32 nameLength = 0;
			// 尝试获取包全名长度
			LONG result = GetCurrentPackageFullName(&nameLength, nullptr);

			// APPMODEL_ERROR_NO_PACKAGE 表示没有包标识
			return result != APPMODEL_ERROR_NO_PACKAGE;
		}
		catch (...)
		{
			return false;
		}
	}

	std::string AppEnvironment::GetPackageFamilyName()
	{
		try
		{
			UINT32 nameLength = 0;
			if (GetCurrentPackageFamilyName(&nameLength, nullptr) == ERROR_INSUFFICIENT_BUFFER)
			{
				std::wstring familyName(nameLength, L'\0');
				if (GetCurrentPackageFamilyName(&nameLength, &familyName[0]) == ERROR_SUCCESS)
				{
					// 转换为UTF-8
					int size = WideCharToMultiByte(CP_UTF8, 0, familyName.c_str(), -1, nullptr, 0, nullptr, nullptr);
					if (size > 0)
					{
						std::string result(size - 1, 0);
						WideCharToMultiByte(CP_UTF8, 0, familyName.c_str(), -1, &result[0], size, nullptr, nullptr);
						return result;
					}
				}
			}
		}
		catch (...)
		{
		}

		return "";
	}

	std::string AppEnvironment::GetAppTypeName()
	{
		if (IsRunningInAppContainer())
		{
			return "Packaged (MSIX/AppContainer)";
		}
		else
		{
			return "Desktop (Unpackaged)";
		}
	}
}
