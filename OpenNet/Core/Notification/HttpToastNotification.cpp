module;

#include "pch.h"
#include "WindowsPlatform.h"
#include <unknwn.h>
#include <roapi.h>

#ifdef WINRT_IMPORT_MODULE
#undef WINRT_IMPORT_MODULE
#endif
#include <include/ToastBuilder.hpp>
#include <shellapi.h>

module OpenNet.Core.Notification.HttpToastNotification;

namespace
{
	struct NotificationApartment
	{
		HRESULT result{ RoInitialize(RO_INIT_MULTITHREADED) };
		NotificationApartment()
		{
			if (FAILED(result) && result != RPC_E_CHANGED_MODE)
				throw winrt::hresult_error{ result };
		}
		~NotificationApartment()
		{
			if (SUCCEEDED(result)) RoUninitialize();
		}
	};
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

	void ShowTorrentDownloadCompleted(std::string const& name)
	{
		try
		{
			NotificationApartment apartment;
			auto toast = ToastBuilder::Toast().Duration(ToastBuilder::Long)
				(
					ToastBuilder::Visual()(ToastBuilder::Binding().Template(L"ToastGeneric")
										   (ToastBuilder::Text()(L"Torrent complete"),
											ToastBuilder::Text()(winrt::to_hstring(name).c_str())))
					);
			winrt::Windows::UI::Notifications::ToastNotification legacyToast = toast;
			winrt::Microsoft::Windows::AppNotifications::AppNotification notification{
				std::wstring{ legacyToast.Content().GetXml() } };
			winrt::Microsoft::Windows::AppNotifications::AppNotificationManager::Default().Show(notification);
		}
		catch (winrt::hresult_error const& error)
		{
			OutputDebugStringW((L"Torrent completion notification failed: "
								+ std::wstring{ error.message().c_str() } + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("Torrent completion notification failed.\n");
		}
	}

	void ShowHttpDownloadCompleted(std::string const& name, std::filesystem::path const& outputPath, std::int64_t const elapsedSeconds, std::uint64_t const completedBytes)
	{
		try
		{
			NotificationApartment apartment;
			auto const average = static_cast<double>(completedBytes) / static_cast<double>((std::max<std::int64_t>)(1, elapsedSeconds));
			auto const rate = average >= 1048576.0 ? std::format(L"{:.1f} MiB/s", average / 1048576.0) : std::format(L"{:.0f} KiB/s", average / 1024.0);
			auto toast = ToastBuilder::Toast().Duration(ToastBuilder::Long)
				(
					ToastBuilder::Visual()(ToastBuilder::Binding().Template(L"ToastGeneric")
										   (
											   ToastBuilder::Text()(L"Download complete"),
											   ToastBuilder::Text()(winrt::to_hstring(name).c_str()),
											   ToastBuilder::Text()(std::format(L"Elapsed: {:02}:{:02}:{:02} · Average: {}", elapsedSeconds / 3600, (elapsedSeconds / 60) % 60, elapsedSeconds % 60, rate).c_str())
											   ))
					);
			winrt::Windows::UI::Notifications::ToastNotification legacyToast = toast;
			auto xml = std::wstring{ legacyToast.Content().GetXml() };
			if (auto const folder = outputPath.parent_path(); !folder.empty())
			{
				auto uriText = std::wstring{ L"file:///" } + folder.generic_wstring();
				for (std::size_t position = 0; (position = uriText.find(L' ', position)) != std::wstring::npos; position += 3) uriText.replace(position, 1, L"%20");
				auto const actions = std::wstring{ L"<actions><action content=\"Open folder\" activationType=\"protocol\" arguments=\"" } + uriText + L"\"/></actions>";
				xml.insert(xml.rfind(L"</toast>"), actions);
			}
			winrt::Microsoft::Windows::AppNotifications::AppNotification notification{ xml };
			winrt::Microsoft::Windows::AppNotifications::AppNotificationManager::Default().Show(notification);
		}
		catch (winrt::hresult_error const& error)
		{
			OutputDebugStringW((L"HTTP completion notification failed: "
								+ std::wstring{ error.message().c_str() } + L"\n").c_str());
		}
		catch (std::exception const& error)
		{
			OutputDebugStringA((std::string{ "HTTP completion notification failed: " }
			+ error.what() + "\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("HTTP completion notification failed.\n");
		}
	}
}
