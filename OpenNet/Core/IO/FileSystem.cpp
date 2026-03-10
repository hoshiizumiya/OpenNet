#include "pch.h"
#include "FileSystem.h"
#include "../AppEnvironment.h"
#include <winrt/Microsoft.Windows.Storage.h>
#include <shlobj_core.h>
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

using namespace winrt;
using namespace winrt::Microsoft::Windows::Storage;

namespace winrt::OpenNet::Core::IO
{
	std::string FileSystem::GetAppDataPath()
	{
		try
		{
			// 使用 Windows App SDK GetDefault() - 支持打包和不打包应用
			// 注意：GetDefault() 需要包标识或AppContainer环境
			auto localPath = ApplicationData::GetDefault().LocalPath();

			// Convert to std::string
			std::string appDataPath = winrt::to_string(localPath);

			// Ensure OpenNet subfolder exists 这个不应该用这个子文件夹的吧？？先注释了
			//std::string openNetPath = appDataPath + "\\OpenNet";
			//CreateDirectory(openNetPath);

			//return openNetPath;
			return appDataPath;
		}
		catch (...)
		{
			// Fallback to Windows API if WinRT fails or no package identity
			wchar_t path[MAX_PATH];
			if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path)))
			{
				std::wstring wpath(path);
				// Append OpenNet folder
				wpath += L"\\OpenNet";

				// Create if not exists
				::CreateDirectoryW(wpath.c_str(), nullptr);

				// Convert to UTF-8
				int size = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
				if (size > 0)
				{
					std::string result(size - 1, 0);
					WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &result[0], size, nullptr, nullptr);
					return result;
				}
			}

			return {};
		}
	}

	std::string FileSystem::GetTempPath()
	{
		wchar_t tempPath[MAX_PATH];
		DWORD result = ::GetTempPathW(MAX_PATH, tempPath);
		if (result == 0 || result > MAX_PATH)
		{
			return {};
		}

		std::wstring tempDir(tempPath);
		// Append OpenNet subfolder
		tempDir += L"OpenNet";

		// Create directory if it doesn't exist
		::CreateDirectoryW(tempDir.c_str(), nullptr);

		// Convert to UTF-8
		int size = WideCharToMultiByte(CP_UTF8, 0, tempDir.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (size > 0)
		{
			std::string result_str(size - 1, 0);
			WideCharToMultiByte(CP_UTF8, 0, tempDir.c_str(), -1, &result_str[0], size, nullptr, nullptr);
			return result_str;
		}

		return {};
	}

	std::string FileSystem::GetDownloadsPath()
	{
		try
		{
			wchar_t* downloadsPath = nullptr;
			// Use FOLDERID_Downloads instead of CSIDL_DOWNLOADS
			if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &downloadsPath)))
			{
				// Convert to UTF-8
				int size = WideCharToMultiByte(CP_UTF8, 0, downloadsPath, -1, nullptr, 0, nullptr, nullptr);
				if (size > 0)
				{
					std::string result(size - 1, 0);
					WideCharToMultiByte(CP_UTF8, 0, downloadsPath, -1, &result[0], size, nullptr, nullptr);
					CoTaskMemFree(downloadsPath);
					return result;
				}
				CoTaskMemFree(downloadsPath);
			}

			return {};
		}
		catch (...)
		{
			return {};
		}
	}

	bool FileSystem::CreateDirectory(const std::string& path)
	{
		try
		{
			// Convert UTF-8 to wide string
			int size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
			if (size <= 0)
			{
				return false;
			}

			std::wstring wpath(size - 1, 0);
			MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], size);

			BOOL result = ::CreateDirectoryW(wpath.c_str(), nullptr);
			if (result)
			{
				return true;
			}

			DWORD error = GetLastError();
			// ERROR_ALREADY_EXISTS is acceptable
			return error == ERROR_ALREADY_EXISTS;
		}
		catch (...)
		{
			return false;
		}
	}

	bool FileSystem::DirectoryExists(const std::string& path)
	{
		try
		{
			// Convert UTF-8 to wide string
			int size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
			if (size <= 0)
			{
				return false;
			}

			std::wstring wpath(size - 1, 0);
			MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], size);

			DWORD attributes = GetFileAttributesW(wpath.c_str());
			return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
		}
		catch (...)
		{
			return false;
		}
	}

	bool FileSystem::FileExists(const std::string& path)
	{
		try
		{
			// Convert UTF-8 to wide string
			int size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
			if (size <= 0)
			{
				return false;
			}

			std::wstring wpath(size - 1, 0);
			MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], size);

			DWORD attributes = GetFileAttributesW(wpath.c_str());
			return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
		}
		catch (...)
		{
			return false;
		}
	}
}
