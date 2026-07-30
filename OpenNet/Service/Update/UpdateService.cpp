#include "pch.h"
#include "UpdateService.h"

import OpenNet.Core.ApplicationModel;
import OpenNet.Web.ServerDomain;
import winrt.Windows.Data.Json;
import winrt.Windows.System;
import winrt.Windows.Web.Http;

using namespace winrt;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Web::Http;

namespace OpenNet::Service::Update
{
	IAsyncAction UpdateService::CheckUpdateAsync(
		std::shared_ptr<CheckUpdateResult> result) const
	{
		if (!result)
		{
			co_return;
		}

		*result = {};
		try
		{
			HttpClient client;
			auto const json = co_await client.GetStringAsync(Uri{ GetEndpoint() });
			JsonObject root;
			if (!JsonObject::TryParse(json, root))
			{
				result->Kind = CheckUpdateResultKind::VersionApiInvalidResponse;
				result->ErrorMessage = L"The update server returned invalid JSON.";
				co_return;
			}

			result->Package.Version = root.GetNamedString(L"version", L"");
			result->Package.Validation = root.GetNamedString(L"validation", L"");
			result->Package.ReleaseNotes = root.GetNamedString(L"releaseNotes", L"");
			result->Package.PublishedAtUtc = root.GetNamedString(L"publishedAtUtc", L"");

			auto const mirrors = root.GetNamedArray(L"mirrors", JsonArray{});
			for (auto const& item : mirrors)
			{
				auto const mirror = item;
				if (mirror.ValueType() != JsonValueType::Object)
				{
					continue;
				}

				auto const object = mirror.GetObject();
				PackageMirror parsed{
					object.GetNamedString(L"url", L""),
					object.GetNamedString(L"mirrorName", L""),
					object.GetNamedString(L"mirrorType", L"Browser")
				};
				if (!parsed.Url.empty())
				{
					result->Package.Mirrors.emplace_back(std::move(parsed));
				}
			}

			if (result->Package.Version.empty() || result->Package.Mirrors.empty())
			{
				result->Kind = CheckUpdateResultKind::VersionApiInvalidResponse;
				result->ErrorMessage = L"The update response is missing a version or download mirror.";
				co_return;
			}

			if (!IsValidSha256(result->Package.Validation))
			{
				result->Kind = CheckUpdateResultKind::VersionApiInvalidSha256;
				result->ErrorMessage = L"The update response contains an invalid SHA-256 value.";
				co_return;
			}

			result->Kind = IsRemoteVersionNewer(result->Package.Version)
				? CheckUpdateResultKind::UpdateAvailable
				: CheckUpdateResultKind::AlreadyUpdated;
		}
		catch (hresult_error const& error)
		{
			result->Kind = CheckUpdateResultKind::VersionApiInvalidResponse;
			result->ErrorMessage = error.message();
		}
		catch (...)
		{
			result->Kind = CheckUpdateResultKind::VersionApiInvalidResponse;
			result->ErrorMessage = L"An unexpected error occurred while checking for updates.";
		}
	}

	IAsyncOperation<bool> UpdateService::TriggerUpdateAsync(
		CheckUpdateResult const& result) const
	{
		if (result.Kind != CheckUpdateResultKind::UpdateAvailable)
		{
			co_return false;
		}

		auto const uri = SelectMirror(result.Package);
		if (!uri)
		{
			co_return false;
		}

		try
		{
			co_return co_await winrt::Windows::System::Launcher::LaunchUriAsync(uri);
		}
		catch (...)
		{
			co_return false;
		}
	}

	hstring UpdateService::GetEndpoint()
	{
		std::wstring endpoint{ ::OpenNet::Web::ServerDomain::GetApiRoot() };
		if (!endpoint.ends_with(L'/'))
		{
			endpoint.push_back(L'/');
		}

#if defined(_M_ARM64)
		constexpr wchar_t architecture[] = L"arm64";
#elif defined(_M_IX86)
		constexpr wchar_t architecture[] = L"x86";
#else
		constexpr wchar_t architecture[] = L"x64";
#endif

		endpoint.append(L"api/v1/update/latest?channel=stable&architecture=");
		endpoint.append(architecture);
		endpoint.append(
			::OpenNet::Core::ApplicationModel::PackageIdentityAdapter::HasPackageIdentity()
			? L"&packageType=msix"
			: L"&packageType=installer");
		return hstring{ endpoint };
	}

	bool UpdateService::IsValidSha256(hstring const& value) noexcept
	{
		if (value.size() != 64)
		{
			return false;
		}

		return std::ranges::all_of(value, [](wchar_t character)
		{
			return (character >= L'0' && character <= L'9')
				|| (character >= L'a' && character <= L'f')
				|| (character >= L'A' && character <= L'F');
		});
	}

	std::array<std::uint32_t, 4> UpdateService::ParseVersion(
		std::wstring_view value) noexcept
	{
		std::array<std::uint32_t, 4> components{};
		std::size_t componentIndex{};
		std::uint32_t current{};
		bool hasDigit{};

		if (!value.empty() && (value.front() == L'v' || value.front() == L'V'))
		{
			value.remove_prefix(1);
		}

		for (wchar_t character : value)
		{
			if (character >= L'0' && character <= L'9')
			{
				hasDigit = true;
				current = current * 10u + static_cast<std::uint32_t>(character - L'0');
			}
			else if (character == L'.' && componentIndex + 1 < components.size())
			{
				components[componentIndex++] = current;
				current = 0;
			}
			else
			{
				break;
			}
		}

		if (hasDigit&& componentIndex < components.size())
		{
			components[componentIndex] = current;
		}
		return components;
	}

	bool UpdateService::IsRemoteVersionNewer(hstring const& remoteVersion)
	{
		auto const current =
			::OpenNet::Core::ApplicationModel::PackageIdentityAdapter::GetAppVersion();
		std::array<std::uint32_t, 4> const local{
			static_cast<std::uint32_t>(std::max(0, current.Major)),
			static_cast<std::uint32_t>(std::max(0, current.Minor)),
			static_cast<std::uint32_t>(std::max(0, current.Build)),
			0
		};
		return ParseVersion(remoteVersion) > local;
	}

	Uri UpdateService::SelectMirror(PackageInformation const& package)
	{
		for (std::wstring_view preferredType : { L"Direct", L"Archive", L"Browser" })
		{
			auto const iterator = std::ranges::find_if(
				package.Mirrors,
				[preferredType](PackageMirror const& mirror)
			{
				return std::wstring_view{ mirror.Type.c_str(), mirror.Type.size() }
				.compare(preferredType) == 0;
			});
			if (iterator != package.Mirrors.end())
			{
				try
				{
					return Uri{ iterator->Url };
				}
				catch (...)
				{
				}
			}
		}
		return nullptr;
	}
}
