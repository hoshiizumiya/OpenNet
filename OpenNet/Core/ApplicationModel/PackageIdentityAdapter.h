#pragma once

#include <winrt/Windows.ApplicationModel.h>  
#include <winrt/Windows.Storage.h>  
#include <winrt/Windows.Foundation.h>  
#include <windows.h>  
#include <string>  

namespace OpenNet::Core::ApplicationModel
{
	class Version
	{
	public:
		Version(int major, int minor, int build)
			: Major(major), Minor(minor), Build(build) {
		}

		int Major;
		int Minor;
		int Build;

		std::wstring ToString() const
		{
			return std::to_wstring(Major) + L"." + std::to_wstring(Minor) + L"." + std::to_wstring(Build);
		}
	};

	class PackageIdentityAdapter
	{
	public:
		static bool HasPackageIdentity();
		static Version GetAppVersion();
		static std::wstring GetAppDirectory();
		static std::wstring GetFamilyName();
		static std::wstring GetPublisherId();

	private:
		static Version TrimToThree(const Version& v);
	};
}