#include <Windows.h>
#include <Psapi.h>
#include <netfw.h>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/version.hpp>

#include "XamlWorkaround.h"
#include "RuntimeStatusWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/RuntimeStatusWindow.g.cpp")
#include "UI/Xaml/View/Windows/RuntimeStatusWindow.g.cpp"
#endif

#include "Core/WebUI/WebUIControl.h"
#include "Core/IPFilter/IPFilterManager.h"

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.DownloadManager;
import OpenNet.Core.HttpStateManager;
import OpenNet.Core.IO.FileSystem;
import OpenNet.Core.P2PManager;
import OpenNet.Core.TorrentSettings;
import OpenNet.Helpers.ThemeHelper;
import winrt.Microsoft.UI.Windowing;
import winrt.Windows.ApplicationModel;
import winrt.Windows.ApplicationModel.DataTransfer;
import winrt.Windows.Globalization.DateTimeFormatting;
import winrt.Windows.Networking;
import winrt.Windows.Networking.Connectivity;
import winrt.Windows.System.UserProfile;

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	namespace
	{
		constexpr wchar_t const* NotApplicable =
			L"Not applicable to OpenNet";

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

		std::wstring LimitText(std::int64_t value)
		{
			return value <= 0 ? L"Unlimited" : FormatBytes(value) + L"/s";
		}

		std::wstring Join(
			std::vector<std::wstring> values,
			std::wstring_view separator = L", ")
		{
			std::sort(values.begin(), values.end());
			values.erase(std::unique(values.begin(), values.end()), values.end());
			std::wstring result;
			for (auto const& value : values)
			{
				if (!result.empty())
					result += separator;
				result += value;
			}
			return result.empty() ? L"None detected" : result;
		}

		struct NetworkAddresses
		{
			std::vector<std::wstring> ipv4;
			std::vector<std::wstring> ipv6;
		};

		NetworkAddresses GetNetworkAddresses()
		{
			static NetworkAddresses cached;
			static std::uint64_t lastQuery = 0;
			auto const now = GetTickCount64();
			if (lastQuery != 0 && now - lastQuery < 10'000)
				return cached;
			lastQuery = now;

			NetworkAddresses result;
			try
			{
				using namespace winrt::Windows::Networking;
				using namespace winrt::Windows::Networking::Connectivity;
				for (auto const& host : NetworkInformation::GetHostNames())
				{
					auto const ip = host.IPInformation();
					if (!ip || !ip.NetworkAdapter())
						continue;
					auto const value = std::wstring(host.CanonicalName());
					if (value == L"127.0.0.1" || value == L"::1")
						continue;
					if (host.Type() == HostNameType::Ipv4)
						result.ipv4.push_back(value);
					else if (host.Type() == HostNameType::Ipv6)
						result.ipv6.push_back(value);
				}
			}
			catch (...)
			{
			}
			cached = result;
			return cached;
		}

		std::wstring GetVolumeSummary()
		{
			static std::wstring cached;
			static std::uint64_t lastQuery = 0;
			auto const now = GetTickCount64();
			if (!cached.empty() && now - lastQuery < 30'000)
				return cached;
			lastQuery = now;

			DWORD const required = GetLogicalDriveStringsW(0, nullptr);
			if (required == 0)
				return L"No mounted volumes";
			std::vector<wchar_t> buffer(required + 1);
			if (!GetLogicalDriveStringsW(
				static_cast<DWORD>(buffer.size()), buffer.data()))
				return L"Unable to enumerate volumes";

			std::vector<std::wstring> volumes;
			for (auto root = buffer.data(); *root;
				 root += std::wcslen(root) + 1)
			{
				auto const type = GetDriveTypeW(root);
				if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
					continue;
				ULARGE_INTEGER available{}, total{}, free{};
				if (GetDiskFreeSpaceExW(root, &available, &total, &free))
				{
					volumes.push_back(std::format(
						L"{} {} free / {}",
						root,
						FormatBytes(free.QuadPart),
						FormatBytes(total.QuadPart)));
				}
			}
			cached = Join(std::move(volumes), L"; ");
			return cached;
		}

		struct HeapUsage
		{
			std::uint64_t busyBytes{};
			std::uint64_t overheadBytes{};
			std::uint32_t heapCount{};
		};

		HeapUsage GetHeapUsage()
		{
			static HeapUsage cached;
			static std::uint64_t lastQuery = 0;
			auto const now = GetTickCount64();
			if (lastQuery != 0 && now - lastQuery < 5'000)
				return cached;
			lastQuery = now;

			HeapUsage result;
			auto const count = GetProcessHeaps(0, nullptr);
			if (count == 0)
				return result;
			std::vector<HANDLE> heaps(count);
			auto const actual = GetProcessHeaps(count, heaps.data());
			result.heapCount = std::min(actual, count);
			for (DWORD index = 0; index < result.heapCount; ++index)
			{
				if (!HeapLock(heaps[index]))
					continue;
				PROCESS_HEAP_ENTRY entry{};
				while (HeapWalk(heaps[index], &entry))
				{
					if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY)
						result.busyBytes += entry.cbData;
					result.overheadBytes += entry.cbOverhead;
				}
				HeapUnlock(heaps[index]);
			}
			cached = result;
			return cached;
		}

		std::uint64_t GetFreeProcessAddressSpace()
		{
			static std::uint64_t cached = 0;
			static std::uint64_t lastQuery = 0;
			auto const now = GetTickCount64();
			if (lastQuery != 0 && now - lastQuery < 5'000)
				return cached;
			lastQuery = now;

			SYSTEM_INFO info{};
			GetSystemInfo(&info);
			auto address = static_cast<std::byte*>(
				info.lpMinimumApplicationAddress);
			auto const maximum = reinterpret_cast<std::uintptr_t>(
				info.lpMaximumApplicationAddress);
			std::uint64_t freeBytes = 0;
			while (reinterpret_cast<std::uintptr_t>(address) < maximum)
			{
				MEMORY_BASIC_INFORMATION region{};
				if (VirtualQuery(address, &region, sizeof(region)) == 0
					|| region.RegionSize == 0)
					break;
				if (region.State == MEM_FREE)
					freeBytes += region.RegionSize;
				address += region.RegionSize;
			}
			cached = freeBytes;
			return cached;
		}

		std::uint32_t GetPhysicalCoreCount()
		{
			static std::uint32_t const cached = []
			{
				DWORD length = 0;
				GetLogicalProcessorInformationEx(
					RelationProcessorCore, nullptr, &length);
				std::vector<std::byte> buffer(length);
				if (length == 0 || !GetLogicalProcessorInformationEx(
					RelationProcessorCore,
					reinterpret_cast<
					PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
						buffer.data()),
					&length))
					return std::uint32_t{};
				std::uint32_t cores = 0;
				for (DWORD offset = 0; offset < length;)
				{
					auto const entry = reinterpret_cast<
						PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
							buffer.data() + offset);
					++cores;
					offset += entry->Size;
				}
				return cores;
			}();
			return cached;
		}

		std::wstring GetFirewallState()
		{
			static std::wstring cached;
			static std::uint64_t lastQuery = 0;
			auto const now = GetTickCount64();
			if (!cached.empty() && now - lastQuery < 10'000)
				return cached;
			lastQuery = now;

			INetFwPolicy2* policy = nullptr;
			auto const hr = CoCreateInstance(
				__uuidof(NetFwPolicy2),
				nullptr,
				CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&policy));
			if (FAILED(hr) || !policy)
			{
				cached = std::format(L"Unable to query (0x{:08X})", hr);
				return cached;
			}

			long currentProfiles = 0;
			policy->get_CurrentProfileTypes(&currentProfiles);
			std::vector<std::wstring> states;
			auto append = [&](long profile, wchar_t const* name)
			{
				if ((currentProfiles & profile) == 0)
					return;
				VARIANT_BOOL enabled = VARIANT_FALSE;
				if (SUCCEEDED(policy->get_FirewallEnabled(
					static_cast<NET_FW_PROFILE_TYPE2>(profile), &enabled)))
				{
					states.push_back(std::format(
						L"{}: {}",
						name,
						enabled == VARIANT_TRUE ? L"On" : L"Off"));
				}
			};
			append(NET_FW_PROFILE2_DOMAIN, L"Domain");
			append(NET_FW_PROFILE2_PRIVATE, L"Private");
			append(NET_FW_PROFILE2_PUBLIC, L"Public");
			policy->Release();
			cached = Join(std::move(states));
			return cached;
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
				group.IsExpanded(section.initiallyExpanded);
				group.IsGroup(true);
				m_statusItems.Append(group);
			}
			group.Name(section.title);
			group.Value(std::format(
				L"{} items", section.rows.size()));

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
		auto const sessionMetrics = core
			? core->GetSessionMetrics()
			: std::unordered_map<std::string, std::int64_t>{};
		auto const mapping = core
			? core->GetPortMappingStatus()
			: ::OpenNet::Core::Torrent::LibtorrentHandle::PortMappingStatus{};
		auto const httpDown =
			::OpenNet::Core::DownloadManager::Instance().TotalHttpDownloadSpeed();
		auto const httpUp =
			::OpenNet::Core::DownloadManager::Instance().TotalHttpUploadSpeed();
		auto const aria2Available =
			::OpenNet::Core::DownloadManager::Instance().IsAria2Available();
		auto const webStats =
			::OpenNet::Core::WebUI::GetWebUIRuntimeStats();

		auto& settingsManager =
			::OpenNet::Core::TorrentSettingsManager::Instance();
		settingsManager.Load();
		auto const torrentSettings = settingsManager.Get();

		std::vector<::OpenNet::Core::HttpDownloadRecord> httpRecords;
		try
		{
			httpRecords =
				::OpenNet::Core::HttpStateManager::Instance().LoadAllRecords();
		}
		catch (...)
		{
		}
		std::array<std::uint64_t, 5> httpStateCounts{};
		std::uint64_t httpCompletedBytes = 0;
		for (auto const& record : httpRecords)
		{
			if (record.status >= 0
				&& record.status < static_cast<int>(httpStateCounts.size()))
				++httpStateCounts[record.status];
			httpCompletedBytes += static_cast<std::uint64_t>(
				std::max<std::int64_t>(0, record.completedSize));
		}

		static std::uint64_t persistedP2PDownloaded = 0;
		static std::size_t persistedP2PTasks = 0;
		static std::uint64_t lastP2PDatabaseQuery = 0;
		auto const databaseQueryTime = GetTickCount64();
		if (lastP2PDatabaseQuery == 0
			|| databaseQueryTime - lastP2PDatabaseQuery >= 5'000)
		{
			lastP2PDatabaseQuery = databaseQueryTime;
			try
			{
				auto const tasks =
					::OpenNet::Core::P2PManager::Instance().GetAllTasks();
				persistedP2PTasks = tasks.size();
				persistedP2PDownloaded = 0;
				for (auto const& task : tasks)
				{
					persistedP2PDownloaded += static_cast<std::uint64_t>(
						std::max<std::int64_t>(0, task.downloadedSize));
				}
			}
			catch (...)
			{
			}
		}

		auto metric = [&sessionMetrics](
			std::string_view name) -> std::int64_t
		{
			auto const it = sessionMetrics.find(std::string(name));
			return it == sessionMetrics.end() ? 0 : it->second;
		};
		auto sumPrefix = [&sessionMetrics](std::string_view prefix)
		{
			std::int64_t result = 0;
			for (auto const& [name, value] : sessionMetrics)
			{
				if (name.starts_with(prefix))
					result += value;
			}
			return result;
		};

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
		auto const heap = GetHeapUsage();
		auto const freeAddressSpace = GetFreeProcessAddressSpace();
		IO_COUNTERS ioCounters{};
		GetProcessIoCounters(GetCurrentProcess(), &ioCounters);
		DWORD handleCount = 0;
		GetProcessHandleCount(GetCurrentProcess(), &handleCount);
		auto const networkAddresses = GetNetworkAddresses();

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

		double const metricInterval = m_previousMetricWallTime > 0
			&& wallValue > m_previousMetricWallTime
			? static_cast<double>(wallValue - m_previousMetricWallTime)
			/ 10'000'000.0
			: 0.0;
		auto metricRate = [this, &metric, metricInterval](
			std::string_view name) -> std::uint64_t
		{
			if (metricInterval <= 0.0)
				return 0;
			auto const current = metric(name);
			auto const previous = m_previousMetrics.find(std::string(name));
			if (previous == m_previousMetrics.end()
				|| current < previous->second)
				return 0;
			return static_cast<std::uint64_t>(
				(current - previous->second) / metricInterval);
		};
		auto counterRate = [this, metricInterval](
			std::string_view name,
			std::uint64_t current) -> std::uint64_t
		{
			if (metricInterval <= 0.0)
				return 0;
			auto const previous = m_previousMetrics.find(std::string(name));
			if (previous == m_previousMetrics.end()
				|| previous->second < 0
				|| current < static_cast<std::uint64_t>(previous->second))
				return 0;
			return static_cast<std::uint64_t>(
				(current - static_cast<std::uint64_t>(previous->second))
				/ metricInterval);
		};
		auto const webReceiveRate =
			counterRate("webui.request_bytes", webStats.requestBytes);
		auto const webSendRate =
			counterRate("webui.response_bytes", webStats.responseBytes);

		ULARGE_INTEGER diskFree{}, diskTotal{}, diskAvailable{};
		GetDiskFreeSpaceExW(
			nullptr, &diskAvailable, &diskTotal, &diskFree);

		m_statusSections.clear();
		std::wstring report = L"OpenNet complete runtime status\r\n";
		StatusSection* currentSection = nullptr;
		auto section = [this, &report, &currentSection](
			std::wstring_view title, bool initiallyExpanded = true)
		{
			m_statusSections.push_back(StatusSection{
				std::wstring(title), {}, initiallyExpanded });
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
		section(L"Application and tasks");
		row(L"Version", version);
		row(
			L"BitTorrent engine",
			std::wstring(to_hstring(libtorrent::version_str).c_str()));
		row(
			L"Up time",
			FormatDuration(
				(FileTimeValue(currentTime) - FileTimeValue(creation))
				/ 10000ULL));
		row(
			L"Overall tasks",
			std::format(
				L"{} (BitTorrent {}, HTTP {})",
				std::max<std::size_t>(
					stats.numTorrents, persistedP2PTasks) + httpRecords.size(),
				std::max<std::size_t>(
					stats.numTorrents, persistedP2PTasks),
				httpRecords.size()));
		row(
			L"Running tasks",
			std::format(
				L"{} (BitTorrent {}, HTTP {})",
				stats.numRunningTorrents + httpStateCounts[1],
				stats.numRunningTorrents,
				httpStateCounts[1]));
		row(
			L"BitTorrent task states",
			std::format(
				L"Downloading {}, metadata {}, seeding {}, checking {}, "
				L"paused {}, error {}",
				stats.numDownloadingTorrents,
				stats.numMetadataTorrents,
				stats.numSeedingTorrents,
				stats.numCheckingTorrents,
				stats.numPausedTorrents,
				stats.numErrorTorrents));
		row(
			L"HTTP task states",
			std::format(
				L"Pending {}, downloading {}, paused {}, completed {}, failed {}",
				httpStateCounts[0],
				httpStateCounts[1],
				httpStateCounts[2],
				httpStateCounts[3],
				httpStateCounts[4]));
		row(
			L"Long-term seeding",
			std::format(
				L"{} native seeding tasks; BitComet LTSeed protocol is not used",
				stats.numSeedingTorrents));
		row(
			L"Metadata downloading",
			std::to_wstring(stats.numMetadataTorrents));

		std::uint64_t metadataBytes = 0;
		std::uint64_t metadataFiles = 0;
		try
		{
			auto const folder = std::filesystem::path(
				winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW())
				/ L"Torrents";
			std::error_code error;
			for (std::filesystem::directory_iterator it(folder, error), end;
				 !error && it != end; it.increment(error))
			{
				if (it->is_regular_file(error)
					&& it->path().extension() == L".torrent")
				{
					++metadataFiles;
					metadataBytes += it->file_size(error);
				}
			}
		}
		catch (...)
		{
		}
		row(
			L"Metadata cache files",
			std::format(
				L"{} files / {}", metadataFiles, FormatBytes(metadataBytes)));
		row(
			L"Torrent exchange blocklist",
			std::format(
				L"{}; {} persisted rules",
				::OpenNet::Core::IPFilterManager::Instance().IsEnabled()
				? L"Enabled"
				: L"Disabled",
				::OpenNet::Core::IPFilterManager::Instance().GetRuleCount()));
		row(
			L"BitTorrent core",
			::OpenNet::Core::P2PManager::Instance()
			.IsTorrentCoreInitialized()
			? L"Initialized"
			: L"Not initialized");
		row(
			L"HTTP core",
			aria2Available ? L"Aria2 available" : L"Aria2 unavailable");
		row(
			L"Remote access / Web UI",
			::OpenNet::Core::WebUI::IsWebUIRunning()
			? L"Running"
			: L"Stopped");
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto const webAddress =
			database.GetString("webui_host", "address").value_or("127.0.0.1");
		auto const webPort =
			database.GetInt("webui_host", "port").value_or(8080);
		row(
			L"Web UI endpoint",
			std::format(
				L"http://{}:{}",
				to_hstring(webAddress).c_str(),
				webPort));
		row(
			L"Web UI account",
			std::format(
				L"{}; password {}",
				to_hstring(database.GetString(
					"webui_host", "username").value_or("admin")).c_str(),
				database.GetBool("webui_host", "initialized").value_or(false)
				? L"initialized"
				: L"requires first-run initialization"));
		row(
			L"Web UI activity",
			std::format(
				L"{} requests, {} active connections, {} failed logins",
				webStats.requests,
				webStats.activeConnections,
				webStats.failedLogins));

		section(L"Connections and addresses");
		auto const tcpPeers =
			metric("peer.num_tcp_peers")
			+ metric("peer.num_ssl_peers");
		auto const proxyPeers =
			metric("peer.num_socks5_peers")
			+ metric("peer.num_http_proxy_peers")
			+ metric("peer.num_ssl_socks5_peers")
			+ metric("peer.num_ssl_http_proxy_peers");
		auto const utpPeers =
			metric("peer.num_utp_peers")
			+ metric("peer.num_ssl_utp_peers");
		row(
			L"TCP connections established",
			std::format(
				L"{} direct, {} proxy", tcpPeers, proxyPeers));
		row(
			L"TCP connections maximum",
			std::to_wstring(torrentSettings.connectionsLimit));
		row(
			L"TCP half-open / maximum",
			std::format(
				L"{} / dynamically managed",
				metric("peer.num_peers_half_open")));
		row(
			L"Pending connections",
			std::format(
				L"{} half-open, {} outstanding accepts",
				metric("peer.num_peers_half_open"),
				metric("ses.num_outstanding_accept")));
		row(
			L"BT connection count",
			std::format(
				L"{} reported peers; TCP {}, uTP {}, proxy {}",
				stats.numPeers,
				tcpPeers,
				utpPeers,
				proxyPeers));
		row(
			L"HTTP connection activity",
			std::format(
				L"{} active / {} waiting tasks (Aria2 reports tasks, not sockets)",
				httpStateCounts[1],
				httpStateCounts[0]));
		row(
			L"HTTP tracker connection count",
			std::format(
				L"{} queued announces",
				metric("tracker.num_queued_tracker_announces")));
		row(L"LAN IPv4", Join(networkAddresses.ipv4));
		row(L"LAN IPv6", Join(networkAddresses.ipv6));
		row(
			L"WAN IPv4",
			mapping.externalAddress.empty()
			? L"Not observed by port mapping"
			: to_hstring(mapping.externalAddress).c_str());
		row(
			L"WAN IPv6",
			networkAddresses.ipv6.empty()
			? L"No IPv6 address observed"
			: L"See LAN IPv6 (globally routable/link-local scope is OS supplied)");
		row(
			L"BT TCP listen port",
			stats.isListening
			? std::to_wstring(stats.listenPort)
			: std::wstring(L"Not listening: ")
			+ std::wstring(to_hstring(stats.listenError)));
		row(
			L"Incoming peer connectivity",
			metric("net.has_incoming_connections") != 0
			? L"Observed"
			: L"Not observed in this session");
		row(
			L"BT TCP firewall/router state IPv4",
			mapping.tcpExternalPort > 0
			? std::format(
				L"Mapped to {} via {}",
				mapping.tcpExternalPort,
				to_hstring(mapping.tcpMechanism).c_str())
			: L"No confirmed external mapping");
		row(
			L"BT TCP firewall/router state IPv6",
			L"Direct IPv6 reachability; no NAT mapping is required");
		row(
			L"BT UDP listen port",
			stats.listenPort > 0
			? std::to_wstring(stats.listenPort)
			: L"Not listening");
		row(
			L"BT UDP mapped port",
			mapping.udpExternalPort > 0
			? std::to_wstring(mapping.udpExternalPort)
			: L"No confirmed mapping");
		row(
			L"uTP connections",
			std::format(
				L"Connected {}, SYN sent {}, FIN sent {}, close wait {}, idle {}",
				metric("utp.num_utp_connected"),
				metric("utp.num_utp_syn_sent"),
				metric("utp.num_utp_fin_sent"),
				metric("utp.num_utp_close_wait"),
				metric("utp.num_utp_idle")));
		row(
			L"Remote access port",
			std::to_wstring(webPort));
		row(
			L"Remote access connections",
			std::to_wstring(webStats.activeConnections));
		row(
			L"LSD port",
			torrentSettings.enableLsd
			? L"6771/UDP (enabled)"
			: L"Disabled");
		row(L"Windows Firewall state", GetFirewallState());
		row(
			L"UPnP NAT mapping",
			mapping.upnpEnabled ? L"Enabled" : L"Disabled");
		row(
			L"NAT-PMP mapping",
			mapping.natPmpEnabled ? L"Enabled" : L"Disabled");
		row(
			L"Port mapping mechanisms",
			std::format(
				L"TCP: {}; UDP: {}",
				mapping.tcpMechanism.empty()
				? L"none confirmed"
				: to_hstring(mapping.tcpMechanism).c_str(),
				mapping.udpMechanism.empty()
				? L"none confirmed"
				: to_hstring(mapping.udpMechanism).c_str()));
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
		row(
			L"BitTorrent download / upload rate",
			std::format(
				L"{} / {}",
				FormatBytes(std::max<std::int64_t>(
					0, stats.totalDownloadRate)) + L"/s",
				FormatBytes(std::max<std::int64_t>(
					0, stats.totalUploadRate)) + L"/s"));
		row(
			L"HTTP download / upload rate",
			std::format(
				L"{} / {}",
				FormatBytes(httpDown) + L"/s",
				FormatBytes(httpUp) + L"/s"));
		row(
			L"Download / upload limits",
			std::format(
				L"{} / {}",
				LimitText(torrentSettings.downloadRateLimit),
				LimitText(torrentSettings.uploadRateLimit)));
		row(
			L"Active task limits",
			std::format(
				L"Downloads {}, seeds {}, total {}",
				torrentSettings.activeDownloads,
				torrentSettings.activeSeeds,
				torrentSettings.activeLimit));
		row(
			L"Maximum peer list per task",
			std::to_wstring(torrentSettings.maxPeerListSize));
		row(
			L"Tracker receive / send rate",
			std::format(
				L"{} / {}",
				FormatBytes(metricRate("net.recv_tracker_bytes")) + L"/s",
				FormatBytes(metricRate("net.sent_tracker_bytes")) + L"/s"));
		row(
			L"Tracker receive / send total",
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_tracker_bytes")),
				FormatBytes(metric("net.sent_tracker_bytes"))));
		row(
			L"BT metadata messages",
			std::format(
				L"Received {}, sent {}",
				metric("ses.num_incoming_metadata"),
				metric("ses.num_outgoing_metadata")));
		row(
			L"BT upload slots",
			std::format(
				L"{} active / {} allowed",
				metric("peer.num_peers_up_unchoked"),
				metric("ses.num_unchoke_slots")));
		row(
			L"Seeding upload rate",
			FormatBytes(std::max<std::int64_t>(
				0, stats.longTermSeedingUploadRate)) + L"/s");
		row(
			L"Remote access transfer rate",
			std::format(
				L"Receive {} / send {}",
				FormatBytes(webReceiveRate) + L"/s",
				FormatBytes(webSendRate) + L"/s"));
		row(
			L"Remote access transfer total",
			std::format(
				L"Receive {} / send {}",
				FormatBytes(webStats.requestBytes),
				FormatBytes(webStats.responseBytes)));
		row(L"DHT nodes IPv4", std::to_wstring(stats.dhtNodes));
		row(
			L"DHT routing cache / torrents / peers",
			std::format(
				L"{} / {} / {}",
				metric("dht.dht_node_cache"),
				metric("dht.dht_torrents"),
				metric("dht.dht_peers")));
		row(
			L"DNS resolver state",
			L"Managed internally by libtorrent/WinHTTP; no public cache queue counter");

		section(L"Process and memory");
		row(
			L"CPU usage",
			std::format(
				L"{:.2f}% ({} physical cores / {} logical processors)",
				cpu,
				GetPhysicalCoreCount(),
				GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
		row(L"Memory working set", FormatBytes(memory.WorkingSetSize));
		row(L"Memory commit/private", FormatBytes(memory.PrivateUsage));
		row(
			L"Process heap",
			std::format(
				L"{} busy + {} overhead across {} heaps",
				FormatBytes(heap.busyBytes),
				FormatBytes(heap.overheadBytes),
				heap.heapCount));
		row(L"Process handle count", std::to_wstring(handleCount));
		row(
			L"libtorrent disk cache",
			FormatBytes(std::max<std::int64_t>(0, stats.diskCacheBytes)));
		row(
			L"Disk write buffer",
			std::format(
				L"{} queued; setting maximum {}",
				FormatBytes(std::max<std::int64_t>(
					0, metric("disk.queued_write_bytes"))),
				FormatBytes(std::max(
					0, core ? core->GetSettings().get_int(
						libtorrent::settings_pack::max_queued_disk_bytes) : 0))));
		row(
			L"TCP/UDP socket buffers",
			std::format(
				L"Receive {}, send {} (0 means OS default)",
				core ? core->GetSettings().get_int(
					libtorrent::settings_pack::recv_socket_buffer_size) : 0,
				core ? core->GetSettings().get_int(
					libtorrent::settings_pack::send_socket_buffer_size) : 0));
		row(
			L"Application model buffers",
			L"STL/WinRT containers are included in heap/private memory totals");
		row(
			L"Logging buffers",
			L"OpenNet uses ETW/debug sinks; no fixed BitComet-style log arenas");
		row(
			L"Reserved virtual memory",
			std::format(
				L"{} reserved/private address space",
				FormatBytes(memory.PrivateUsage)));
		row(
			L"Free physical memory",
			FormatBytes(systemMemory.ullAvailPhys));
		row(
			L"Free virtual memory",
			FormatBytes(systemMemory.ullAvailVirtual));
		row(
			L"Free process address space",
			FormatBytes(freeAddressSpace));

		section(L"Storage and disk cache");
		row(
			L"Storage total",
			FormatBytes(diskTotal.QuadPart));
		row(
			L"Storage free",
			FormatBytes(diskFree.QuadPart));
		row(L"Volume list", GetVolumeSummary());
		row(
			L"Disk cache data size",
			std::format(
				L"{} ({} 16-KiB blocks)",
				FormatBytes(std::max<std::int64_t>(0, stats.diskCacheBytes)),
				metric("disk.disk_blocks_in_use")));
		row(
			L"libtorrent disk read / write",
			std::format(
				L"{} / {}",
				FormatBytes(std::max<std::int64_t>(
					0, metric("disk.num_blocks_read")) * 16 * 1024),
				FormatBytes(std::max<std::int64_t>(
					0, metric("disk.num_blocks_written")) * 16 * 1024)));
		row(
			L"libtorrent read / write operations",
			std::format(
				L"{} / {}",
				metric("disk.num_read_ops"),
				metric("disk.num_write_ops")));
		row(
			L"libtorrent disk jobs",
			std::format(
				L"Queued {}, running {}, blocked {}, read {}, write {}",
				metric("disk.queued_disk_jobs"),
				metric("disk.num_running_disk_jobs"),
				metric("disk.blocked_disk_jobs"),
				metric("disk.num_read_jobs"),
				metric("disk.num_write_jobs")));
		row(
			L"libtorrent disk timing",
			std::format(
				L"Read {} ms, write {} ms, hash {} ms, request latency {} us",
				metric("disk.disk_read_time") / 1000,
				metric("disk.disk_write_time") / 1000,
				metric("disk.disk_hash_time") / 1000,
				metric("disk.request_latency")));
		row(
			L"Process disk read total",
			std::format(
				L"{} in {} operations",
				FormatBytes(ioCounters.ReadTransferCount),
				ioCounters.ReadOperationCount));
		row(
			L"Process disk write total",
			std::format(
				L"{} in {} operations",
				FormatBytes(ioCounters.WriteTransferCount),
				ioCounters.WriteOperationCount));
		row(
			L"Persisted HTTP completed data",
			FormatBytes(httpCompletedBytes));
		row(
			L"Long-term seed / disk boost service",
			NotApplicable);

		section(L"Session totals");
		row(
			L"Total downloaded (session)",
			FormatBytes(std::max<std::int64_t>(0, stats.totalDownloaded)));
		row(
			L"Total uploaded (session)",
			FormatBytes(std::max<std::int64_t>(0, stats.totalUploaded)));
		row(
			L"Network payload receive / send",
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_payload_bytes")),
				FormatBytes(metric("net.sent_payload_bytes"))));
		row(
			L"Network protocol receive / send",
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_bytes")),
				FormatBytes(metric("net.sent_bytes"))));
		row(
			L"IP overhead receive / send",
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_ip_overhead_bytes")),
				FormatBytes(metric("net.sent_ip_overhead_bytes"))));
		row(
			L"Failed / redundant download",
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_failed_bytes")),
				FormatBytes(metric("net.recv_redundant_bytes"))));
		row(
			L"Piece checks passed / failed",
			std::format(
				L"{} / {}",
				metric("ses.num_piece_passed"),
				metric("ses.num_piece_failed")));
		row(
			L"Persisted HTTP downloaded",
			FormatBytes(httpCompletedBytes));
		row(
			L"Persisted P2P downloaded",
			FormatBytes(persistedP2PDownloaded));
		row(
			L"Persisted combined downloaded",
			FormatBytes(persistedP2PDownloaded + httpCompletedBytes));

		section(L"UDP, DHT and detection diagnostics");
		row(
			L"DHT bytes receive / send",
			std::format(
				L"{} / {}",
				FormatBytes(stats.dhtBytesReceived),
				FormatBytes(stats.dhtBytesSent)));
		row(
			L"DHT receive / send rate",
			std::format(
				L"{} / {}",
				FormatBytes(metricRate("dht.dht_bytes_in")) + L"/s",
				FormatBytes(metricRate("dht.dht_bytes_out")) + L"/s"));
		row(
			L"DHT messages receive / send",
			std::format(
				L"{} / {}",
				metric("dht.dht_messages_in"),
				metric("dht.dht_messages_out")));
		row(
			L"DHT dropped receive / send",
			std::format(
				L"{} / {}",
				metric("dht.dht_messages_in_dropped"),
				metric("dht.dht_messages_out_dropped")));
		row(
			L"DHT request types receive / send",
			std::format(
				L"ping {}/{}, find_node {}/{}, get_peers {}/{}, "
				L"announce_peer {}/{}, get {}/{}, put {}/{}",
				metric("dht.dht_ping_in"),
				metric("dht.dht_ping_out"),
				metric("dht.dht_find_node_in"),
				metric("dht.dht_find_node_out"),
				metric("dht.dht_get_peers_in"),
				metric("dht.dht_get_peers_out"),
				metric("dht.dht_announce_peer_in"),
				metric("dht.dht_announce_peer_out"),
				metric("dht.dht_get_in"),
				metric("dht.dht_get_out"),
				metric("dht.dht_put_in"),
				metric("dht.dht_put_out")));
		row(
			L"DHT invalid requests",
			std::to_wstring(sumPrefix("dht.dht_invalid_")));
		row(
			L"DHT pending RPC observers",
			std::to_wstring(metric("dht.dht_allocated_observers")));
		row(
			L"DHT upload rate limit",
			LimitText(core ? core->GetSettings().get_int(
				libtorrent::settings_pack::dht_upload_rate_limit) : 0));
		row(
			L"uTP packets receive / send",
			std::format(
				L"{} / {}",
				metric("utp.utp_packets_in"),
				metric("utp.utp_packets_out")));
		row(
			L"uTP payload packets receive / send",
			std::format(
				L"{} / {}",
				metric("utp.utp_payload_pkts_in"),
				metric("utp.utp_payload_pkts_out")));
		row(
			L"uTP loss / timeout / resend",
			std::format(
				L"{} / {} / {}",
				metric("utp.utp_packet_loss"),
				metric("utp.utp_timeout"),
				metric("utp.utp_packet_resend")));
		row(
			L"UDP tracker traffic",
			std::format(
				L"Included in tracker receive/send totals: {} / {}",
				FormatBytes(metric("net.recv_tracker_bytes")),
				FormatBytes(metric("net.sent_tracker_bytes"))));
		row(
			L"NAT detection",
			std::format(
				L"External address {}; mapped TCP/UDP {} / {}",
				mapping.externalAddress.empty()
				? L"not observed"
				: to_hstring(mapping.externalAddress).c_str(),
				mapping.tcpExternalPort,
				mapping.udpExternalPort));
		row(
			L"IP detection",
			metric("net.has_incoming_connections") != 0
			? L"Incoming connectivity observed"
			: L"No incoming connectivity observed");
		row(
			L"DNS failure domain counts",
			L"Resolver details are not part of libtorrent's public statistics API");

		section(L"Schedulers and trackers");
		row(
			L"Peer messages receive / send",
			std::format(
				L"{} / {}",
				sumPrefix("ses.num_incoming_"),
				sumPrefix("ses.num_outgoing_")));
		row(
			L"Rate limiter queues",
			std::format(
				L"Upload {} sockets / {}; download {} sockets / {}",
				metric("net.limiter_up_queue"),
				FormatBytes(metric("net.limiter_up_bytes")),
				metric("net.limiter_down_queue"),
				FormatBytes(metric("net.limiter_down_bytes"))));
		row(
			L"Network event wakeups",
			std::format(
				L"Read {}, write {}, tick {}, UDP {}, accept {}, disk {}",
				metric("net.on_read_counter"),
				metric("net.on_write_counter"),
				metric("net.on_tick_counter"),
				metric("net.on_udp_counter"),
				metric("net.on_accept_counter"),
				metric("net.on_disk_counter")));
		row(
			L"Queued tracker announces",
			std::to_wstring(
				metric("tracker.num_queued_tracker_announces")));
		row(
			L"Tracker connection details",
			std::format(
				L"Queued {}; received {}; sent {}",
				metric("tracker.num_queued_tracker_announces"),
				FormatBytes(metric("net.recv_tracker_bytes")),
				FormatBytes(metric("net.sent_tracker_bytes"))));
		row(
			L"Disk scheduler",
			std::format(
				L"{} queued / {} running / {} blocked",
				metric("disk.queued_disk_jobs"),
				metric("disk.num_running_disk_jobs"),
				metric("disk.blocked_disk_jobs")));
		row(
			L"Timer/message implementation",
			L"libtorrent ASIO executor; BitComet timer queue internals do not apply");

		section(L"BitComet-specific subsystem coverage", false);
		row(L"LTSeed client/server protocol", NotApplicable);
		row(L"LTSeed UDP queues", NotApplicable);
		row(L"Disk boost service", NotApplicable);
		row(L"BitComet message queue categories", NotApplicable);
		row(L"BitComet timer queue categories", NotApplicable);
		row(
			L"OpenNet equivalents",
			L"Native BitTorrent/libtorrent, Aria2 HTTP, ASIO scheduler, mmap disk I/O");

		section(L"Raw libtorrent session metrics", false);
		std::vector<std::pair<std::string, std::int64_t>> sortedMetrics(
			sessionMetrics.begin(), sessionMetrics.end());
		std::ranges::sort(sortedMetrics, [](auto const& left, auto const& right)
		{
			return left.first < right.first;
		});
		for (auto const& [name, value] : sortedMetrics)
			row(std::wstring(to_hstring(name).c_str()), std::to_wstring(value));

		report += L"\r\nAll values above are bound to live display items. "
			L"Raw counters come directly from session_stats_alert; "
			L"engine-specific omissions are identified explicitly.\r\n";
		m_previousMetrics = sessionMetrics;
		m_previousMetrics["webui.request_bytes"] =
			static_cast<std::int64_t>(webStats.requestBytes);
		m_previousMetrics["webui.response_bytes"] =
			static_cast<std::int64_t>(webStats.responseBytes);
		m_previousMetricWallTime = wallValue;
		return hstring{ report };
	}
}
