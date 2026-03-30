#pragma once

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

		std::string ToString() const
		{
			return std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Build);
		}
	};

	class PackageIdentityAdapter
	{
	public:
		static bool HasPackageIdentity();
		static Version GetAppVersion();
		static std::wstring GetAppDirectory();
		static winrt::hstring GetFamilyName();
		static std::wstring GetPublisherId();

	private:
		static Version TrimToThree(const Version& v);
	};
}