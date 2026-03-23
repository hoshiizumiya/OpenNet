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
	std::wstring FileSystem::AppDataPathW;
	std::wstring FileSystem::AppTempPathW;
	winrt::hstring FileSystem::AppDownloadPathW;

	// Never use it
	bool FileSystem::CreateDirectory(const std::wstring& path)
	{
		try
		{
			BOOL result = ::CreateDirectoryW(path.c_str(), nullptr);
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

	bool FileSystem::DirectoryExists(const std::wstring& path)
	{
		try
		{
			DWORD attributes = GetFileAttributesW(path.c_str());
			return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
		}
		catch (...)
		{
			return false;
		}
	}

	bool FileSystem::FileExists(const std::wstring& path)
	{
		try
		{
			DWORD attributes = GetFileAttributesW(path.c_str());
			return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
		}
		catch (...)
		{
			return false;
		}
	}

	std::wstring_view FileSystem::GetAppDataPathW()
	{
		if (AppDataPathW.empty())
		{
			try
			{
				AppDataPathW = ApplicationData::GetDefault().LocalPath();
			}
			catch (...)
			{
				return {};
			}
		}

		return AppDataPathW;
	}

	std::wstring_view FileSystem::GetAppTempPathW()
	{
		if (!AppTempPathW.empty())
		{
			return AppTempPathW;
		}
		try
		{
			AppTempPathW = ApplicationData::GetDefault().LocalCachePath();
			return AppTempPathW;
		}
		catch (...)
		{
			return {};
		}
	}

	// .get() may not available in the UI thread, so make sure it's not in the UI thread at the first call
	winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> FileSystem::GetDownloadsPathW()
	{
		if (!AppDownloadPathW.empty())
		{
			co_return AppDownloadPathW;
		}
		try
		{ 
			Windows::Storage::StorageFolder DownF = co_await winrt::Windows::Storage::KnownFolders::GetFolderAsync(winrt::Windows::Storage::KnownFolderId::DownloadsFolder);
				
			AppDownloadPathW = DownF.Path().c_str();
			co_return AppDownloadPathW;
		}
		catch (...)
		{
			co_return {};
		}
	}
}
