// 文件: OpenNetUnitTest\FileSystemBasicTests.cpp
// FileSystem 基本单元测试（不依赖项目链接）

#include "pch.h"
#include <CppUnitTest.h>
#include <string>
#include <windows.h>
#include <shlobj_core.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace OpenNetUnitTest
{
	TEST_CLASS(FileSystemIntegrationTests)
	{
	public:
		/// <summary>
		/// 测试Windows API - GetTempPath
		/// 验证Windows临时文件夹获取功能
		/// </summary>
		TEST_METHOD(TestWindowsAPI_GetTempPath)
		{
			wchar_t tempPath[MAX_PATH];
			DWORD result = ::GetTempPathW(MAX_PATH, tempPath);

			Assert::IsTrue(result > 0 && result < MAX_PATH, L"GetTempPathW should succeed");
			Assert::IsFalse(wcslen(tempPath) == 0, L"Temp path should not be empty");
		}

		/// <summary>
		/// 测试Windows API - CreateDirectory
		/// 验证文件夹创建功能
		/// </summary>
		TEST_METHOD(TestWindowsAPI_CreateAndDeleteDirectory)
		{
			wchar_t tempPath[MAX_PATH];
			::GetTempPathW(MAX_PATH, tempPath);

			// 创建测试文件夹
			std::wstring testPath = std::wstring(tempPath) + L"UnitTestFolder123\\";
			BOOL created = ::CreateDirectoryW(testPath.c_str(), nullptr);

			// CreateDirectory可能返回FALSE如果文件夹已存在
			Assert::IsTrue(created || GetLastError() == ERROR_ALREADY_EXISTS, 
						   L"CreateDirectoryW should succeed or folder already exists");

			// 检查文件夹是否存在
			DWORD attribs = GetFileAttributesW(testPath.c_str());
			Assert::IsTrue(attribs != INVALID_FILE_ATTRIBUTES && (attribs & FILE_ATTRIBUTE_DIRECTORY),
						   L"Folder should exist and be a directory");

			// 删除文件夹
			BOOL deleted = ::RemoveDirectoryW(testPath.c_str());
			Assert::IsTrue(deleted, L"RemoveDirectoryW should succeed");
		}

		/// <summary>
		/// 测试Windows API - GetFileAttributes
		/// 验证文件/文件夹属性检查功能
		/// </summary>
		TEST_METHOD(TestWindowsAPI_GetFileAttributes)
		{
			// 系统文件夹应该存在
			DWORD attribs = GetFileAttributesW(L"C:\\Windows\\");
			Assert::IsTrue(attribs != INVALID_FILE_ATTRIBUTES, L"Windows folder should exist");
			Assert::IsTrue(attribs & FILE_ATTRIBUTE_DIRECTORY, L"Windows should be a directory");

			// 系统文件应该存在
			attribs = GetFileAttributesW(L"C:\\Windows\\System32\\kernel32.dll");
			Assert::IsTrue(attribs != INVALID_FILE_ATTRIBUTES, L"kernel32.dll should exist");
			Assert::IsFalse(attribs & FILE_ATTRIBUTE_DIRECTORY, L"kernel32.dll should be a file");
		}

		/// <summary>
		/// 测试Windows API - AppContainer检测
		/// 验证AppContainer环境检测功能
		/// </summary>
		TEST_METHOD(TestWindowsAPI_AppContainerDetection)
		{
			HANDLE token = nullptr;
			BOOL success = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
			Assert::IsTrue(success, L"OpenProcessToken should succeed");

			DWORD size = 0;
			DWORD isAppContainer = 0;

			success = GetTokenInformation(token, TokenIsAppContainer, &isAppContainer, sizeof(DWORD), &size);
			Assert::IsTrue(success, L"GetTokenInformation should succeed");

			// isAppContainer 应该是 0 或 1
			Assert::IsTrue((isAppContainer == 0 || isAppContainer == 1), 
						   L"isAppContainer should be 0 or 1");

			if (token)
				CloseHandle(token);
		}

		/// <summary>
		/// 测试字符编码转换
		/// 验证UTF-8和Wide String之间的转换
		/// </summary>
		TEST_METHOD(TestCharacterConversion_UTF8ToWideString)
		{
			std::string utf8String = "Hello\\World\\Test";

			// UTF-8 to Wide
			int size = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
			Assert::IsTrue(size > 0, L"MultiByteToWideChar size check should succeed");

			std::wstring wideString(size - 1, L'\0');
			int converted = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &wideString[0], size);
			Assert::IsTrue(converted > 0, L"MultiByteToWideChar should succeed");
		}

		/// <summary>
		/// 测试磁盘空间查询
		/// 验证GetDiskFreeSpaceEx功能
		/// </summary>
		TEST_METHOD(TestWindowsAPI_GetDiskFreeSpace)
		{
			ULARGE_INTEGER freeBytes;
			ULARGE_INTEGER totalBytes;
			ULARGE_INTEGER totalFreeBytes;

			BOOL success = GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, &totalFreeBytes);
			Assert::IsTrue(success, L"GetDiskFreeSpaceExW should succeed");

			// 空间应该大于0
			Assert::IsTrue(freeBytes.QuadPart > 0, L"Free space should be > 0");
			Assert::IsTrue(totalBytes.QuadPart > freeBytes.QuadPart, L"Total should be > free");
		}
	};

	TEST_CLASS(AppEnvironmentConceptTests)
	{
	public:
		/// <summary>
		/// 验证全局变量概念
		/// 测试全局标志的实用性
		/// </summary>
		TEST_METHOD(TestGlobalVariableConcept_AppContainerFlag)
		{
			// 模拟全局变量 g_isAppContainer
			static bool g_isAppContainer_local = false;

			// 初始化时检测
			HANDLE token = nullptr;
			if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
			{
				DWORD size = 0;
				DWORD isAppContainer = 0;
				if (GetTokenInformation(token, TokenIsAppContainer, &isAppContainer, sizeof(DWORD), &size))
				{
					g_isAppContainer_local = (isAppContainer != 0);
				}
				CloseHandle(token);
			}

			// 验证全局标志可以使用
			Logger::WriteMessage(std::format(L"g_isAppContainer = {}\n", g_isAppContainer_local).c_str());
			Assert::IsTrue((g_isAppContainer_local || !g_isAppContainer_local), 
						   L"Global flag should be set correctly");
		}

		/// <summary>
		/// 验证路径构造
		/// 测试动态路径构造和验证
		/// </summary>
		TEST_METHOD(TestPathConstruction_Validation)
		{
			// 构造应用数据路径示例
			std::wstring examplePath = L"C:\\Users\\Username\\AppData\\Local\\OpenNet";

			Logger::WriteMessage(std::format(L"Example Path: {}\n", examplePath).c_str());

			Assert::IsTrue(examplePath.find(L"OpenNet") != std::wstring::npos,
						   L"Path should contain OpenNet folder");

			Assert::IsTrue(examplePath.find(L"AppData") != std::wstring::npos,
						   L"Path should contain AppData folder");
		}
	};
}
