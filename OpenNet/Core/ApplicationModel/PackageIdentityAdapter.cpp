#include "pch.h"
#include "PackageIdentityAdapter.h"  
#include <PathCch.h>  
#include <memory>  
#include <mutex>  
#include <vector>  

// Link with Version.lib for GetFileVersionInfo* and VerQueryValue
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "Pathcch.lib")

namespace OpenNet::Core::ApplicationModel
{
	bool PackageIdentityAdapter::HasPackageIdentity()
	{
		static std::once_flag flag;
		static bool hasPackageIdentity = false;

		std::call_once(flag, []()
		{
			try
			{
				auto package = winrt::Windows::ApplicationModel::Package::Current();
				auto id = package.Id();
				hasPackageIdentity = true;
			}
			catch (...)
			{
				hasPackageIdentity = false;
			}
		});

		return hasPackageIdentity;
	}

	Version PackageIdentityAdapter::GetAppVersion()
	{
		static std::once_flag flag;
		static Version version(1, 0, 0);

		std::call_once(flag, []()
		{
			if (HasPackageIdentity())
			{
				try
				{
					auto package = winrt::Windows::ApplicationModel::Package::Current();
					auto packageVersion = package.Id().Version();
					version = Version(packageVersion.Major, packageVersion.Minor, packageVersion.Build);
				}
				catch (winrt::hresult hr)
				{
					version = Version(0, 0, 0);
#ifdef _DEBUG
					OutputDebugString(L"Unkonw error" + hr);
#endif // _DEBUG

				}
			}
			else
			{
				WCHAR modulePath[MAX_PATH];
				if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) > 0)
				{
					DWORD handle = 0;
					DWORD size = GetFileVersionInfoSizeW(modulePath, &handle);
					if (size > 0)
					{
						std::vector<BYTE> buffer(size);
						if (GetFileVersionInfoW(modulePath, handle, size, buffer.data()))
						{
							VS_FIXEDFILEINFO* fileInfo = nullptr;
							UINT fileInfoSize = 0;
							if (VerQueryValueW(buffer.data(), L"\\", (LPVOID*)&fileInfo, &fileInfoSize))
							{
								int major = HIWORD(fileInfo->dwFileVersionMS);
								int minor = LOWORD(fileInfo->dwFileVersionMS);
								int build = HIWORD(fileInfo->dwFileVersionLS);
								version = Version(major, minor, build);
							}
						}
					}
				}
			}
		});

		return version;
	}

	std::wstring PackageIdentityAdapter::GetAppDirectory()
	{
		static std::once_flag flag;
		static std::wstring directory;

		std::call_once(flag, []()
		{
			if (HasPackageIdentity())
			{
				try
				{
					auto package = winrt::Windows::ApplicationModel::Package::Current();
					auto location = package.InstalledLocation();
					directory = std::wstring(location.Path());
				}
				catch (...)
				{
					directory = L"";
				}
			}
			else
			{
				WCHAR modulePath[MAX_PATH];
				if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) > 0)
				{
					// Fixed: correct parameter order for PathCchRemoveFileSpec  
					if (PathCchRemoveFileSpec(modulePath, MAX_PATH) == S_OK)
					{
						directory = modulePath;
					}
				}
			}
		});

		return directory;
	}

	std::wstring PackageIdentityAdapter::GetFamilyName()
	{
		static std::once_flag flag;
		static std::wstring familyName;

		std::call_once(flag, []()
		{
			if (HasPackageIdentity())
			{
				try
				{
					auto package = winrt::Windows::ApplicationModel::Package::Current();
					familyName = std::wstring(package.Id().FamilyName());
				}
				catch (...)
				{
					familyName = L"";
				}
			}
			else
			{
				familyName = L"OpenNet.Unpackaged";
			}
		});

		return familyName;
	}

	std::wstring PackageIdentityAdapter::GetPublisherId()
	{
		static std::once_flag flag;
		static std::wstring publisherId;

		std::call_once(flag, []()
		{
			if (HasPackageIdentity())
			{
				try
				{
					auto package = winrt::Windows::ApplicationModel::Package::Current();
					publisherId = std::wstring(package.Id().PublisherId());
				}
				catch (...)
				{
					publisherId = L"";
				}
			}
			else
			{
				publisherId = L"CN=OpenNet-Technology";
			}
		});

		return publisherId;
	}

	Version PackageIdentityAdapter::TrimToThree(const Version& v)
	{
		return Version(v.Major, v.Minor, v.Build);
	}
}