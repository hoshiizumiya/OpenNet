#pragma once

import winrt.Windows.Foundation;
import std;

namespace OpenNet::Service::Update
{
	enum class CheckUpdateResultKind
	{
		None,
		VersionApiInvalidResponse,
		VersionApiInvalidSha256,
		AlreadyUpdated,
		UpdateAvailable,
	};

	struct PackageMirror
	{
		winrt::hstring Url;
		winrt::hstring Name;
		winrt::hstring Type;
	};

	struct PackageInformation
	{
		winrt::hstring Version;
		winrt::hstring Validation;
		winrt::hstring ReleaseNotes;
		winrt::hstring PublishedAtUtc;
		std::vector<PackageMirror> Mirrors;
	};

	struct CheckUpdateResult
	{
		CheckUpdateResultKind Kind{ CheckUpdateResultKind::None };
		PackageInformation Package;
		winrt::hstring ErrorMessage;
	};

	class UpdateService final
	{
	public:
		winrt::Windows::Foundation::IAsyncAction CheckUpdateAsync(std::shared_ptr<CheckUpdateResult> result) const;
		winrt::Windows::Foundation::IAsyncOperation<bool> TriggerUpdateAsync(CheckUpdateResult const& result) const;

	private:
		static winrt::hstring GetEndpoint();
		static bool IsValidSha256(winrt::hstring const& value) noexcept;
		static std::array<std::uint32_t, 4> ParseVersion(
			std::wstring_view value) noexcept;
		static bool IsRemoteVersionNewer(winrt::hstring const& remoteVersion);
		static winrt::Windows::Foundation::Uri SelectMirror(
			PackageInformation const& package);
	};
}
