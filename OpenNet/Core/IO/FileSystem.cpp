module;
#include <shlobj_core.h>
#include <windows.h>
#include <shellapi.h>

#include "../AppEnvironment.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

module OpenNet.Core.IO.FileSystem;

import winrt.Microsoft.Windows.Storage;

using namespace winrt;
using namespace winrt::Microsoft::Windows::Storage;

namespace
{
	std::once_flag g_AppDataOnce;
	std::once_flag g_AppTempOnce;
}

namespace winrt::OpenNet::Core::IO
{
	std::wstring FileSystem::AppDataPathW;
	std::wstring FileSystem::AppTempPathW;
	winrt::hstring FileSystem::AppDownloadPathW;

	// Never use it
	std::wstring FileSystem::Utf8ToWide(std::string_view const value)
	{
		if (value.empty()) return {};
		if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
			throw std::length_error("UTF-8 text is too long.");

		auto const required = ::MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), nullptr, 0);
		if (required <= 0)
			throw std::runtime_error("Invalid UTF-8 text.");

		std::wstring result(static_cast<std::size_t>(required), L'\0');
		if (::MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), result.data(), required) != required)
		{
			throw std::runtime_error("Unable to convert UTF-8 text.");
		}
		return result;
	}

	std::string FileSystem::WideToUtf8(std::wstring_view const value)
	{
		if (value.empty()) return {};
		if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
			throw std::length_error("UTF-16 text is too long.");

		auto const required = ::WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
		if (required <= 0)
			throw std::runtime_error("Invalid UTF-16 text.");

		std::string result(static_cast<std::size_t>(required), '\0');
		if (::WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), result.data(), required,
			nullptr, nullptr) != required)
		{
			throw std::runtime_error("Unable to convert UTF-16 text.");
		}
		return result;
	}

	bool FileSystem::CreateAppDirectory(const std::wstring& path)
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
		std::call_once(g_AppDataOnce, []()
		{
			try
			{
				AppDataPathW = ApplicationData::GetDefault().LocalPath();
			}
			catch (...)
			{
				AppDataPathW.clear();
			}
		});

		return AppDataPathW;
	}

	std::wstring_view FileSystem::GetAppTempPathW()
	{
		std::call_once(g_AppTempOnce, []()
		{
			try
			{
				AppTempPathW = ApplicationData::GetDefault().LocalCachePath();
			}
			catch (...)
			{
				AppTempPathW.clear();
			}
		});

		return AppTempPathW;
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
			co_return{};
		}
	}
}
