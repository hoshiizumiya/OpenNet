module;

#include "pch.h"
#include "WindowsPlatform.h"
#include <objbase.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

module OpenNet.Core.Notification.HttpToastNotification;

import OpenNet.Core.AppSettingsDatabase;
import winrt.Microsoft.Windows.AppNotifications;
import winrt.Microsoft.Windows.AppNotifications.Builder;
import winrt.Microsoft.Windows.ApplicationModel.Resources;
import winrt.Windows.Foundation;

namespace
{
	struct NotificationApartment
	{
		bool initialized{};
		NotificationApartment()
		{
			APTTYPE type{};
			APTTYPEQUALIFIER qualifier{};
			if (::CoGetApartmentType(&type, &qualifier) == CO_E_NOTINITIALIZED)
			{
				winrt::init_apartment(winrt::apartment_type::multi_threaded);
				initialized = true;
			}
		}
		~NotificationApartment()
		{
			if (initialized) winrt::uninit_apartment();
		}
	};

	winrt::hstring Localized(wchar_t const* key, wchar_t const* fallback)
	{
		try
		{
			NotificationApartment apartment;
			return winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceLoader{}.GetString(key);
		}
		catch (...)
		{
			return fallback;
		}
	}

	void ShowDownloadCompleted(std::string const& name, std::filesystem::path const& folder, winrt::hstring const& detail)
	{
		try
		{
			NotificationApartment apartment;
			auto& db = ::OpenNet::Core::AppSettingsDatabase::Instance();
			db.Initialize();
			if (!db.GetBool("ui", "download_notifications_enabled").value_or(true)) return;

			using namespace winrt::Microsoft::Windows::AppNotifications;
			using namespace winrt::Microsoft::Windows::AppNotifications::Builder;
			auto builder = AppNotificationBuilder{};
			builder.SetDuration(AppNotificationDuration::Long);
			builder.AddText(Localized(L"DownloadNotificationTitle", L"Download complete"));
			builder.AddText(winrt::to_hstring(name));
			if (!detail.empty()) builder.AddText(detail);
			if (!folder.empty())
			{
				wchar_t uri[2048]{};
				DWORD length = static_cast<DWORD>(std::size(uri));
				if (SUCCEEDED(::UrlCreateFromPathW(folder.c_str(), uri, &length, 0)))
				{
					auto button = AppNotificationButton{ Localized(L"DownloadNotificationOpenFolder", L"Open folder") };
					button.SetInvokeUri(winrt::Windows::Foundation::Uri{ uri });
					builder.AddButton(button);
				}
			}
			AppNotificationManager::Default().Show(builder.BuildNotification());
		}
		catch (winrt::hresult_error const& error)
		{
			::OutputDebugStringW((L"Download notification failed: " + std::wstring{ error.message().c_str() } + L"\n").c_str());
		}
		catch (...)
		{
			::OutputDebugStringW(L"Download notification failed unexpectedly\n");
		}
	}
}

namespace OpenNet::Core::Notification
{
	bool ShowTestNotification() noexcept
	{
		try
		{
			NotificationApartment apartment;
			winrt::Microsoft::Windows::AppNotifications::AppNotification notification{
				L"<toast><visual><binding template=\"ToastGeneric\">"
				L"<text>OpenNet test notification</text>"
				L"<text>Sent from the developer window.</text>"
				L"</binding></visual></toast>" };
			winrt::Microsoft::Windows::AppNotifications::AppNotificationManager::Default().Show(notification);
			return true;
		}
		catch (winrt::hresult_error const& error)
		{
			OutputDebugStringW((L"Test notification failed: "
								+ std::wstring{ error.message().c_str() } + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("Test notification failed.\n");
		}
		return false;
	}

	void ShowTorrentDownloadCompleted(std::string const& name, std::filesystem::path const& savePath)
	{
		ShowDownloadCompleted(name, savePath, {});
	}

	void ShowHttpDownloadCompleted(std::string const& name, std::filesystem::path const& outputPath, std::int64_t elapsedSeconds, std::uint64_t completedBytes)
	{
		auto const average = static_cast<double>(completedBytes) / static_cast<double>((std::max<std::int64_t>)(1, elapsedSeconds));
		auto const rate = average >= 1048576.0 ? std::format(L"{:.1f} MiB/s", average / 1048576.0) : std::format(L"{:.0f} KiB/s", average / 1024.0);
		ShowDownloadCompleted(name, outputPath.parent_path(),
			Localized(L"DownloadNotificationElapsed", L"Elapsed: ")
			+ winrt::hstring{ std::format(L"{:02}:{:02}:{:02}", elapsedSeconds / 3600, (elapsedSeconds / 60) % 60, elapsedSeconds % 60) }
			+ L" · " + Localized(L"DownloadNotificationAverage", L"Average: ") + winrt::hstring{ rate });
	}
}
