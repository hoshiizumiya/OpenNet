#include <Windows.h>
#include <Psapi.h>

#include "XamlWorkaround.h"
#include "RuntimeStatusWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/RuntimeStatusWindow.g.cpp")
#include "UI/Xaml/View/Windows/RuntimeStatusWindow.g.cpp"
#endif

#include "Core/WebUI/WebUIControl.h"

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.DownloadManager;
import OpenNet.Core.P2PManager;
import OpenNet.Helpers.ThemeHelper;
import winrt.Microsoft.UI.Windowing;
import winrt.Windows.ApplicationModel;
import winrt.Windows.ApplicationModel.DataTransfer;
import winrt.Windows.Globalization.DateTimeFormatting;
import winrt.Windows.System.UserProfile;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	namespace
	{
		constexpr wchar_t const* Unavailable =
			L"N/A (not exposed by the current OpenNet/libtorrent API)";

		std::wstring FormatBytes(std::uint64_t value)
		{
			constexpr double kib = 1024.0;
			constexpr double mib = kib * 1024.0;
			constexpr double gib = mib * 1024.0;
			if (value >= static_cast<std::uint64_t>(gib))
				return std::format(L"{:.2f} GiB", value / gib);
			if (value >= static_cast<std::uint64_t>(mib))
				return std::format(L"{:.2f} MiB", value / mib);
			if (value >= static_cast<std::uint64_t>(kib))
				return std::format(L"{:.2f} KiB", value / kib);
			return std::format(L"{} B", value);
		}

		std::wstring FormatDuration(std::uint64_t milliseconds)
		{
			auto seconds = milliseconds / 1000;
			auto const days = seconds / 86400;
			seconds %= 86400;
			auto const hours = seconds / 3600;
			seconds %= 3600;
			auto const minutes = seconds / 60;
			seconds %= 60;
			return std::format(
				L"{}d {:02}:{:02}:{:02}",
				days,
				hours,
				minutes,
				seconds);
		}

		std::uint64_t FileTimeValue(FILETIME const& value)
		{
			ULARGE_INTEGER result{};
			result.LowPart = value.dwLowDateTime;
			result.HighPart = value.dwHighDateTime;
			return result.QuadPart;
		}
	}

	RuntimeStatusWindow::RuntimeStatusWindow()
	{
		ExtendsContentIntoTitleBar(true);
	}

	void RuntimeStatusWindow::InitializeComponent()
	{
		RuntimeStatusWindowT::InitializeComponent();
		SetTitleBar(RuntimeStatusTitleBar());
		AppWindow().TitleBar().PreferredHeightOption(
			winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);
		AppWindow().Resize({ 920, 760 });
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(*this);
		::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(*this);

		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		auto const interval = std::clamp<std::int64_t>(
			database.GetInt("ui", "refresh_interval_ms").value_or(1000),
			100,
			60000);
		m_refreshTimer = DispatcherQueue().CreateTimer();
		m_refreshTimer.Interval(std::chrono::milliseconds(interval));
		auto weak = get_weak();
		m_refreshTimer.Tick([weak](auto const&, auto const&)
		{
			if (auto self = weak.get())
				self->RefreshReport();
		});
		m_refreshTimer.Start();
		Closed([weak](auto const&, auto const&)
		{
			if (auto self = weak.get(); self && self->m_refreshTimer)
				self->m_refreshTimer.Stop();
		});
		RefreshReport();
	}

	void RuntimeStatusWindow::RefreshButton_Click(
		IInspectable const&, RoutedEventArgs const&)
	{
		RefreshReport();
	}

	void RuntimeStatusWindow::CopyButton_Click(
		IInspectable const&, RoutedEventArgs const&)
	{
		DataPackage package;
		package.SetText(m_lastReport);
		Clipboard::SetContent(package);
		LastUpdatedText().Text(L"Report copied to clipboard");
	}

	void RuntimeStatusWindow::RefreshReport()
	{
		RefreshIndicator().IsActive(true);
		m_lastReport = BuildReport();
		SyncStatusItems();
		SYSTEMTIME now{};
		GetLocalTime(&now);
		LastUpdatedText().Text(std::format(
			L"Last updated {:02}:{:02}:{:02}.{:03}",
			now.wHour,
			now.wMinute,
			now.wSecond,
			now.wMilliseconds));
		RefreshIndicator().IsActive(false);
	}

	winrt::Windows::Foundation::Collections::IObservableVector<
		winrt::Windows::Foundation::IInspectable>
		RuntimeStatusWindow::StatusItems() const
	{
		return m_statusItems;
	}

	void RuntimeStatusWindow::SyncStatusItems()
	{
		for (std::uint32_t sectionIndex = 0;
			sectionIndex < m_statusSections.size();
			++sectionIndex)
		{
			auto const& section = m_statusSections[sectionIndex];
			OpenNet::ViewModels::RuntimeStatusDisplayItem group{ nullptr };
			if (sectionIndex < m_statusItems.Size())
			{
				group = m_statusItems.GetAt(sectionIndex).as<
					OpenNet::ViewModels::RuntimeStatusDisplayItem>();
			}
			else
			{
				group = make<
					OpenNet::ViewModels::implementation::
						RuntimeStatusDisplayItem>();
				group.IsExpanded(true);
				group.IsGroup(true);
				m_statusItems.Append(group);
			}
			group.Name(section.title);
			group.Value(L"");

			auto const children = group.Children();
			for (std::uint32_t rowIndex = 0;
				rowIndex < section.rows.size();
				++rowIndex)
			{
				auto const& row = section.rows[rowIndex];
				OpenNet::ViewModels::RuntimeStatusDisplayItem item{ nullptr };
				if (rowIndex < children.Size())
				{
					item = children.GetAt(rowIndex);
				}
				else
				{
					item = make<
						OpenNet::ViewModels::implementation::
							RuntimeStatusDisplayItem>();
					children.Append(item);
				}
				item.Name(row.name);
				item.Value(row.value);
				item.IsGroup(false);
			}
			while (children.Size() > section.rows.size())
				children.RemoveAtEnd();
		}
		while (m_statusItems.Size() > m_statusSections.size())
			m_statusItems.RemoveAtEnd();
	}

	hstring RuntimeStatusWindow::BuildReport()
	{
		auto const stats =
			::OpenNet::Core::P2PManager::Instance().GetSessionStats();
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		auto const mapping = core
			? core->GetPortMappingStatus()
			: ::OpenNet::Core::Torrent::LibtorrentHandle::PortMappingStatus{};
		auto const httpDown =
			::OpenNet::Core::DownloadManager::Instance().TotalHttpDownloadSpeed();
		auto const httpUp =
			::OpenNet::Core::DownloadManager::Instance().TotalHttpUploadSpeed();

		std::wstring version = L"Unpackaged/dev";
		try
		{
			auto const value =
				winrt::Windows::ApplicationModel::Package::Current()
				.Id().Version();
			version = std::format(
				L"{}.{}.{}.{}",
				value.Major,
				value.Minor,
				value.Build,
				value.Revision);
		}
		catch (...)
		{
		}

		PROCESS_MEMORY_COUNTERS_EX memory{};
		memory.cb = sizeof(memory);
		GetProcessMemoryInfo(
			GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
			sizeof(memory));
		MEMORYSTATUSEX systemMemory{};
		systemMemory.dwLength = sizeof(systemMemory);
		GlobalMemoryStatusEx(&systemMemory);

		FILETIME creation{}, exit{}, kernel{}, user{};
		GetProcessTimes(
			GetCurrentProcess(), &creation, &exit, &kernel, &user);
		FILETIME currentTime{};
		GetSystemTimeAsFileTime(&currentTime);
		auto const kernelValue = FileTimeValue(kernel);
		auto const userValue = FileTimeValue(user);
		auto const wallValue = GetTickCount64() * 10000ULL;
		double cpu = 0.0;
		if (m_previousWallTime > 0 && wallValue > m_previousWallTime)
		{
			auto const processorCount =
				std::max<DWORD>(1, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
			cpu = 100.0
				* static_cast<double>(
					(kernelValue - m_previousKernelTime)
					+ (userValue - m_previousUserTime))
				/ static_cast<double>(wallValue - m_previousWallTime)
				/ processorCount;
		}
		m_previousKernelTime = kernelValue;
		m_previousUserTime = userValue;
		m_previousWallTime = wallValue;

		ULARGE_INTEGER diskFree{}, diskTotal{}, diskAvailable{};
		GetDiskFreeSpaceExW(
			nullptr, &diskAvailable, &diskTotal, &diskFree);

		m_statusSections.clear();
		std::wstring report = L"OpenNet complete runtime status\r\n";
		StatusSection* currentSection = nullptr;
		auto section = [this, &report, &currentSection](std::wstring_view title)
		{
			m_statusSections.push_back(StatusSection{ std::wstring(title), {} });
			currentSection = &m_statusSections.back();
			report += L"\r\n[" + std::wstring(title) + L"]\r\n";
		};
		auto row = [&report, &currentSection](
			std::wstring_view name, std::wstring const& value)
		{
			if (currentSection)
				currentSection->rows.push_back(
					StatusRow{ std::wstring(name), value });
			report += std::format(L"{:<38} {}\r\n", name, value);
		};
		auto unavailable = [&row](std::wstring_view name)
		{
			row(name, Unavailable);
		};

		section(L"Application and tasks");
		row(L"Version", version);
		row(
			L"Up time",
			FormatDuration(
				(FileTimeValue(currentTime) - FileTimeValue(creation))
				/ 10000ULL));
		row(L"Overall tasks", std::to_wstring(stats.numTorrents));
		unavailable(L"Running tasks");
		unavailable(L"Long-term seeding");
		unavailable(L"Metadata downloading");
		unavailable(L"Metadata cache files");
		unavailable(L"Torrent exchange blocklist");
		row(
			L"Remote access / Web UI",
			::OpenNet::Core::WebUI::IsWebUIRunning()
			? L"Running"
			: L"Stopped");

		section(L"Connections and addresses");
		row(L"TCP connections established", std::to_wstring(stats.numPeers));
		unavailable(L"TCP connections maximum");
		unavailable(L"TCP half-open / maximum");
		unavailable(L"Pending connections");
		unavailable(L"BT TCP connection count");
		unavailable(L"HTTP connection count");
		unavailable(L"HTTP tracker connection count");
		unavailable(L"LAN IPv4");
		unavailable(L"LAN IPv6");
		row(
			L"WAN IPv4",
			mapping.externalAddress.empty()
			? Unavailable
			: to_hstring(mapping.externalAddress).c_str());
		unavailable(L"WAN IPv6");
		row(
			L"BT TCP listen port",
			stats.isListening
			? std::to_wstring(stats.listenPort)
			: std::wstring(L"Not listening: ")
			+ std::wstring(to_hstring(stats.listenError)));
		unavailable(L"BT TCP firewall/router state IPv4");
		unavailable(L"BT TCP firewall/router state IPv6");
		row(
			L"BT UDP listen port",
			stats.listenPort > 0
			? std::to_wstring(stats.listenPort)
			: L"Not listening");
		row(
			L"BT UDP mapped port",
			mapping.udpExternalPort > 0
			? std::to_wstring(mapping.udpExternalPort)
			: L"Pending / unavailable");
		unavailable(L"uTP observed ports");
		row(
			L"Remote access port",
			std::to_wstring(
				::OpenNet::Core::AppSettingsDatabase::Instance()
				.GetInt("webui_host", "port").value_or(8080)));
		unavailable(L"LSD port");
		unavailable(L"Windows Firewall state");
		row(
			L"UPnP NAT mapping",
			mapping.upnpEnabled ? L"Enabled" : L"Disabled");
		row(
			L"NAT-PMP mapping",
			mapping.natPmpEnabled ? L"Enabled" : L"Disabled");
		unavailable(L"UPnP router");
		row(
			L"TCP / UDP mapped ports",
			std::format(
				L"{} / {}",
				mapping.tcpExternalPort,
				mapping.udpExternalPort));
		row(
			L"Port mapping error",
			mapping.lastError.empty()
			? L"None"
			: to_hstring(mapping.lastError).c_str());

		section(L"Transfer");
		row(
			L"Overall download rate",
			FormatBytes(std::max<std::int64_t>(0, stats.totalDownloadRate)
						+ httpDown) + L"/s");
		row(
			L"Overall upload rate",
			FormatBytes(std::max<std::int64_t>(0, stats.totalUploadRate)
						+ httpUp) + L"/s");
		unavailable(L"Download / upload limits");
		unavailable(L"Download limit");
		unavailable(L"Upload limit");
		unavailable(L"Maximum connections per task");
		unavailable(L"TCP tracker download rate");
		unavailable(L"TCP tracker upload rate");
		unavailable(L"BT metadata download rate");
		unavailable(L"BT metadata upload rate");
		unavailable(L"BT upload slots");
		unavailable(L"Long-term seeding download rate");
		unavailable(L"Long-term seeding upload rate");
		unavailable(L"Remote access download rate");
		unavailable(L"Remote access upload rate");
		row(L"DHT nodes IPv4", std::to_wstring(stats.dhtNodes));
		unavailable(L"DHT nodes IPv6");
		unavailable(L"DNS cached queries");
		unavailable(L"DNS pending queries");

		section(L"Process and memory");
		row(
			L"CPU usage",
			std::format(
				L"{:.2f}% ({} logical processors)",
				cpu,
				GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
		row(L"Memory working set", FormatBytes(memory.WorkingSetSize));
		row(L"Memory commit/private", FormatBytes(memory.PrivateUsage));
		unavailable(L"Process heap");
		unavailable(L"Disk cache");
		unavailable(L"Disk write buffer");
		unavailable(L"TCP receive/send buffers");
		unavailable(L"UDP receive/send buffers");
		unavailable(L"File list buffer");
		unavailable(L"Torrent list buffer");
		unavailable(L"Metadata buffer/download buffer");
		unavailable(L"Tracker log buffer");
		unavailable(L"Task log buffer");
		unavailable(L"Peer log buffer");
		unavailable(L"Global log buffer");
		unavailable(L"Reserved memory regions");
		row(
			L"Free physical memory",
			FormatBytes(systemMemory.ullAvailPhys));
		row(
			L"Free virtual memory",
			FormatBytes(systemMemory.ullAvailVirtual));
		unavailable(L"Free process address space");

		section(L"Storage and disk cache");
		row(
			L"Storage total",
			FormatBytes(diskTotal.QuadPart));
		row(
			L"Storage free",
			FormatBytes(diskFree.QuadPart));
		unavailable(L"Volume list");
		unavailable(L"Disk cache data size");
		unavailable(L"Disk cache read/write size");
		unavailable(L"Disk read total");
		unavailable(L"Disk read BT");
		unavailable(L"Disk read long-term seed");
		unavailable(L"Disk read requests/actual/hit ratio");
		unavailable(L"Disk write total");
		unavailable(L"Disk write BT");
		unavailable(L"Disk write HTTP");
		unavailable(L"Disk write requests/actual/hit ratio");
		unavailable(L"Disk boost service");

		section(L"Session totals");
		row(
			L"Total downloaded (session)",
			FormatBytes(std::max<std::int64_t>(0, stats.totalDownloaded)));
		row(
			L"Total uploaded (session)",
			FormatBytes(std::max<std::int64_t>(0, stats.totalUploaded)));
		unavailable(L"Lifetime downloaded / uploaded");

		section(L"UDP, DHT and detection diagnostics");
		unavailable(L"UDP total bytes/packets/rates/queues");
		unavailable(L"DHT bytes/packets/rates/queues");
		unavailable(L"UDP tracker bytes/packets/rates/queues");
		unavailable(L"LTSeed UDP client bytes/packets/queues");
		unavailable(L"LTSeed UDP server bytes/packets/queues");
		unavailable(L"uTP bytes/packets/rates/queues");
		unavailable(L"IP detection bytes/packets/queues");
		unavailable(L"NAT detection bytes/packets/queues");
		unavailable(L"DHT rate limits");
		unavailable(L"DHT dropped packets");
		unavailable(L"DHT pending requests");
		unavailable(L"DNS failure domain counts");

		section(L"Schedulers and trackers");
		unavailable(L"Message queue count");
		unavailable(L"Message process speed");
		unavailable(L"Message last tick");
		unavailable(L"Message invoke average");
		unavailable(L"Queued message categories");
		unavailable(L"Timer queue count");
		unavailable(L"Timer process speed");
		unavailable(L"Timer last tick");
		unavailable(L"Timer invoke average");
		unavailable(L"Timer queue categories");
		unavailable(L"HTTP tracker connection details");

		report += L"\r\nNote: unavailable rows are retained intentionally so "
			L"the report remains field-compatible with the supplied BitComet "
			L"status reference while OpenNet exposes more libtorrent counters.\r\n";
		return hstring{ report };
	}
}
