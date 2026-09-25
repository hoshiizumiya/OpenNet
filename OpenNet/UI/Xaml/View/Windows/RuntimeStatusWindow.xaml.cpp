#include <WinSock2.h>
#include <Windows.h>
#include <Psapi.h>
#include <netfw.h>
#include "LibtorrentIncludeGuard.h"
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/version.hpp>
#include "LibtorrentIncludeRestore.h"

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
import OpenNet.Core.Utils.Message;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
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

		std::wstring FormatRuntimeText(
			wchar_t const* resourceKey,
			std::initializer_list<std::wstring> values)
		{
			auto text = std::wstring{ ResourceGetString(resourceKey).c_str() };
			for (auto const& value : values)
			{
				auto const placeholder = text.find(L"{}");
				if (placeholder == std::wstring::npos)
					break;
				text.replace(placeholder, 2, value);
			}
			return text;
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
			return value <= 0
				? std::wstring{ ResourceGetString(L"CommonUnlimited").c_str() }
				: FormatBytes(value) + L"/s";
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
			return result.empty()
				? std::wstring{ ResourceGetString(L"RuntimeStatusNoneDetected").c_str() }
				: result;
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
				return std::wstring{ ResourceGetString(L"RuntimeStatusNoMountedVolumes").c_str() };
			std::vector<wchar_t> buffer(required + 1);
			if (!GetLogicalDriveStringsW(
				static_cast<DWORD>(buffer.size()), buffer.data()))
				return std::wstring{ ResourceGetString(L"RuntimeStatusUnableEnumerateVolumes").c_str() };

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
					volumes.push_back(FormatRuntimeText(
						L"RuntimeStatusVolumeEntryValue", {
						std::wstring{ root }, FormatBytes(free.QuadPart),
						FormatBytes(total.QuadPart)}));
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
				cached = std::wstring{ ResourceGetString(L"RuntimeStatusFirewallQueryFailedPrefix").c_str() } +
					std::format(L"0x{:08X}", hr);
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
						enabled == VARIANT_TRUE
							? ResourceGetString(L"CommonEnabled").c_str()
							: ResourceGetString(L"CommonDisabled").c_str()));
				}
			};
			append(NET_FW_PROFILE2_DOMAIN, ResourceGetString(L"RuntimeStatusFirewallDomain").c_str());
			append(NET_FW_PROFILE2_PRIVATE, ResourceGetString(L"RuntimeStatusFirewallPrivate").c_str());
			append(NET_FW_PROFILE2_PUBLIC, ResourceGetString(L"RuntimeStatusFirewallPublic").c_str());
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
		InitializeWindowExBase();
		SetTitleBar(RuntimeStatusTitleBar());
		AppWindow().TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);

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
			{
				self->m_refreshTimer.Stop();
				::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Save(self->AppWindow());
			}
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
		LastUpdatedText().Text(ResourceGetString(L"ViewRuntimeStatusWindowReportCopied"));
	}

	void RuntimeStatusWindow::RefreshReport()
	{
		RefreshIndicator().IsActive(true);
		m_lastReport = BuildReport();
		SyncStatusItems();
		SYSTEMTIME now{};
		GetLocalTime(&now);
		LastUpdatedText().Text(ResourceGetString(L"RuntimeStatusLastUpdatedPrefix") + std::format(
			L"{:02}:{:02}:{:02}.{:03}",
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
			group.Value(ResourceGetString(L"RuntimeStatusItemsSuffix") +
				to_hstring(static_cast<std::uint32_t>(section.rows.size())));

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
		auto const notApplicable = std::wstring{
			ResourceGetString(L"RuntimeStatusNotApplicable").c_str() };
		auto const stats =
			::OpenNet::Core::P2PManager::Instance().GetSessionStats();
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		auto const sessionMetrics = core
			? core->GetSessionMetrics()
			: std::unordered_map<std::string, std::int64_t>{};
		auto const mapping = core
			? core->GetPortMappingStatus()
			: ::OpenNet::Core::Torrent::LibtorrentHandle::PortMappingStatus{};
		auto const runtimeSettings = core
			? core->GetRuntimeSettings()
			: ::OpenNet::Core::Torrent::LibtorrentHandle::RuntimeSettingsSnapshot{};
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

		std::wstring version = ResourceGetString(L"RuntimeStatusUnpackagedDev").c_str();
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
		std::wstring report = std::wstring{
			ResourceGetString(L"RuntimeStatusReportTitle").c_str() } + L"\r\n";
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
			std::wstring_view name, auto const& value)
		{
			std::wstring valueText;
			if constexpr (std::is_same_v<std::decay_t<decltype(value)>, winrt::hstring>)
				valueText = value.c_str();
			else
				valueText = value;
			if (currentSection)
				currentSection->rows.push_back(
					StatusRow{ std::wstring(name), valueText });
			report += std::format(L"{:<38} {}\r\n", name, valueText);
		};
		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionApplicationTasks").c_str()));
		row(ResourceGetString(L"RuntimeStatusVersion"), version);
		row(
			ResourceGetString(L"RuntimeStatusBitTorrentEngine"),
			std::wstring(to_hstring(libtorrent::version_str).c_str()));
		row(
			ResourceGetString(L"RuntimeStatusUpTime"),
			FormatDuration(
				(FileTimeValue(currentTime) - FileTimeValue(creation))
				/ 10000ULL));
		row(
			ResourceGetString(L"RuntimeStatusOverallTasks"),
			FormatRuntimeText(L"RuntimeStatusTaskBreakdownValue", {
				std::to_wstring(std::max<std::size_t>(stats.numTorrents, persistedP2PTasks) + httpRecords.size()),
				std::to_wstring(std::max<std::size_t>(stats.numTorrents, persistedP2PTasks)),
				std::to_wstring(httpRecords.size())}));
		row(
			ResourceGetString(L"RuntimeStatusRunningTasks"),
			FormatRuntimeText(L"RuntimeStatusTaskBreakdownValue", {
				std::to_wstring(stats.numRunningTorrents + httpStateCounts[1]),
				std::to_wstring(stats.numRunningTorrents),
				std::to_wstring(httpStateCounts[1])}));
		row(
			ResourceGetString(L"RuntimeStatusBitTorrentTaskStates"),
			FormatRuntimeText(L"RuntimeStatusBitTorrentTaskStatesValue", {
				std::to_wstring(stats.numDownloadingTorrents),
				std::to_wstring(stats.numMetadataTorrents),
				std::to_wstring(stats.numSeedingTorrents),
				std::to_wstring(stats.numCheckingTorrents),
				std::to_wstring(stats.numPausedTorrents),
				std::to_wstring(stats.numErrorTorrents)}));
		row(
			ResourceGetString(L"RuntimeStatusHttpTaskStates"),
			FormatRuntimeText(L"RuntimeStatusHttpTaskStatesValue", {
				std::to_wstring(httpStateCounts[0]),
				std::to_wstring(httpStateCounts[1]),
				std::to_wstring(httpStateCounts[2]),
				std::to_wstring(httpStateCounts[3]),
				std::to_wstring(httpStateCounts[4])}));
		row(
			ResourceGetString(L"RuntimeStatusLongTermSeeding"),
			FormatRuntimeText(L"RuntimeStatusLongTermSeedingValue", {
				std::to_wstring(stats.numSeedingTorrents)}));
		row(
			ResourceGetString(L"RuntimeStatusMetadataDownloading"),
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
			ResourceGetString(L"RuntimeStatusMetadataCacheFiles"),
			FormatRuntimeText(L"RuntimeStatusMetadataCacheFilesValue", {
				std::to_wstring(metadataFiles), FormatBytes(metadataBytes)}));
		row(
			ResourceGetString(L"RuntimeStatusTorrentExchangeBlocklist"),
			FormatRuntimeText(L"RuntimeStatusEnabledPersistedRulesValue", {
				std::wstring{ (::OpenNet::Core::IPFilterManager::Instance().IsEnabled()
					? ResourceGetString(L"CommonEnabled")
					: ResourceGetString(L"CommonDisabled")).c_str() },
				std::to_wstring(::OpenNet::Core::IPFilterManager::Instance().GetRuleCount())}));
		row(
			ResourceGetString(L"RuntimeStatusBitTorrentCore"),
			::OpenNet::Core::P2PManager::Instance()
			.IsTorrentCoreInitialized()
			? ResourceGetString(L"RuntimeStatusInitialized")
			: ResourceGetString(L"RuntimeStatusNotInitialized"));
		row(
			ResourceGetString(L"RuntimeStatusHttpCore"),
			aria2Available ? ResourceGetString(L"RuntimeStatusAria2Available") : ResourceGetString(L"RuntimeStatusAria2Unavailable"));
		row(
			ResourceGetString(L"RuntimeStatusRemoteAccessWebUi"),
			::OpenNet::Core::WebUI::IsWebUIRunning()
			? ResourceGetString(L"WebUiRunning")
			: ResourceGetString(L"WebUiStopped"));
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		auto const webAddress =
			database.GetString("webui_host", "address").value_or("127.0.0.1");
		auto const webPort =
			database.GetInt("webui_host", "port").value_or(8080);
		row(
			ResourceGetString(L"RuntimeStatusWebUiEndpoint"),
			std::format(
				L"http://{}:{}",
				to_hstring(webAddress).c_str(),
				webPort));
		row(
			ResourceGetString(L"RuntimeStatusWebUiAccount"),
			FormatRuntimeText(L"RuntimeStatusWebUiAccountValue", {
				std::wstring{ to_hstring(database.GetString(
					"webui_host", "username").value_or("admin")).c_str() },
				std::wstring{ ResourceGetString(database.GetBool("webui_host", "initialized").value_or(false)
					? L"RuntimeStatusPasswordInitialized"
					: L"RuntimeStatusPasswordNeedsInitialization").c_str() }}));
		row(
			ResourceGetString(L"RuntimeStatusWebUiActivity"),
			FormatRuntimeText(L"RuntimeStatusWebUiActivityValue", {
				std::to_wstring(webStats.requests),
				std::to_wstring(webStats.activeConnections),
				std::to_wstring(webStats.failedLogins)}));

		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionConnectionsAddresses").c_str()));
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
			ResourceGetString(L"RuntimeStatusTcpConnectionsEstablished"),
			FormatRuntimeText(L"RuntimeStatusDirectProxyConnectionsValue", {
				std::to_wstring(tcpPeers), std::to_wstring(proxyPeers)}));
		row(
			ResourceGetString(L"RuntimeStatusTcpConnectionsMaximum"),
			std::to_wstring(torrentSettings.connectionsLimit));
		row(
			ResourceGetString(L"RuntimeStatusTcpHalfOpenMaximum"),
			FormatRuntimeText(L"RuntimeStatusHalfOpenManagedValue", {
				std::to_wstring(metric("peer.num_peers_half_open"))}));
		row(
			ResourceGetString(L"RuntimeStatusPendingConnections"),
			FormatRuntimeText(L"RuntimeStatusPendingConnectionsValue", {
				std::to_wstring(metric("peer.num_peers_half_open")),
				std::to_wstring(metric("ses.num_outstanding_accept"))}));
		row(
			ResourceGetString(L"RuntimeStatusBtConnectionCount"),
			FormatRuntimeText(L"RuntimeStatusBtConnectionCountValue", {
				std::to_wstring(stats.numPeers), std::to_wstring(tcpPeers),
				std::to_wstring(utpPeers), std::to_wstring(proxyPeers)}));
		row(
			ResourceGetString(L"RuntimeStatusWebTorrentWebRtc"),
			torrentSettings.enableWebTorrent
			? FormatRuntimeText(L"RuntimeStatusWebTorrentEnabledValue", {
				std::wstring{ to_hstring(torrentSettings.webTorrentStunServer).c_str() },
				std::to_wstring(torrentSettings.maxWebTorrentOffers),
				std::to_wstring(torrentSettings.webTorrentConnectionTimeout)})
			: std::wstring{ ResourceGetString(L"CommonDisabled").c_str() });
		row(
			ResourceGetString(L"RuntimeStatusI2pPex"),
			torrentSettings.enableI2p
			? FormatRuntimeText(L"RuntimeStatusI2pEnabledValue", {
				std::wstring{ to_hstring(torrentSettings.i2pHostname).c_str() },
				std::to_wstring(torrentSettings.i2pPort),
				torrentSettings.allowI2pMixed
					? std::wstring{ ResourceGetString(L"CommonEnabled").c_str() }
					: std::wstring{ ResourceGetString(L"CommonDisabled").c_str() }})
			: std::wstring{ ResourceGetString(L"CommonDisabled").c_str() });
		row(ResourceGetString(L"RuntimeStatusInternalPeerBans"), std::to_wstring(stats.internalBannedIps));
		row(
			ResourceGetString(L"RuntimeStatusHttpConnectionActivity"),
			FormatRuntimeText(L"RuntimeStatusHttpConnectionActivityValue", {
				std::to_wstring(httpStateCounts[1]), std::to_wstring(httpStateCounts[0])}));
		row(
			ResourceGetString(L"RuntimeStatusHttpTrackerConnectionCount"),
			FormatRuntimeText(L"RuntimeStatusQueuedAnnouncesValue", {
				std::to_wstring(metric("tracker.num_queued_tracker_announces"))}));
		row(ResourceGetString(L"RuntimeStatusLanIpv4"), Join(networkAddresses.ipv4));
		row(ResourceGetString(L"RuntimeStatusLanIpv6"), Join(networkAddresses.ipv6));
		row(
			ResourceGetString(L"RuntimeStatusWanIpv4"),
			mapping.externalAddress.empty()
			? ResourceGetString(L"RuntimeStatusWanIpv4NotObserved")
			: to_hstring(mapping.externalAddress).c_str());
		row(
			ResourceGetString(L"RuntimeStatusWanIpv6"),
			networkAddresses.ipv6.empty()
			? ResourceGetString(L"RuntimeStatusWanIpv6NotObserved")
			: ResourceGetString(L"RuntimeStatusWanIpv6SeeLanAddress"));
		row(
			ResourceGetString(L"RuntimeStatusBtTcpListenPort"),
			stats.isListening
			? std::to_wstring(stats.listenPort)
			: std::wstring(ResourceGetString(L"RuntimeStatusNotListeningPrefix").c_str())
			+ (stats.listenError.empty()
				? std::wstring{ ResourceGetString(L"RuntimeStatusListenerErrorUnknown").c_str() }
				: std::wstring(to_hstring(stats.listenError))));
		row(
			ResourceGetString(L"RuntimeStatusIncomingPeerConnectivity"),
			metric("net.has_incoming_connections") != 0
			? std::wstring{ ResourceGetString(L"RuntimeStatusIncomingConnectivityObserved").c_str() }
			: std::wstring{ ResourceGetString(L"RuntimeStatusIncomingConnectivityNotObserved").c_str() });
		row(
			ResourceGetString(L"RuntimeStatusBtTcpFirewallIpv4"),
			mapping.tcpExternalPort > 0
			? FormatRuntimeText(L"RuntimeStatusMappedViaValue", {
				std::to_wstring(mapping.tcpExternalPort),
				std::wstring{ to_hstring(mapping.tcpMechanism).c_str() }})
			: std::wstring{ ResourceGetString(L"RuntimeStatusNoConfirmedExternalMapping").c_str() });
		row(
			ResourceGetString(L"RuntimeStatusBtTcpFirewallIpv6"),
			ResourceGetString(L"RuntimeStatusDirectIpv6NoNatMapping"));
		row(
			ResourceGetString(L"RuntimeStatusBtUdpListenPort"),
			stats.listenPort > 0
			? std::to_wstring(stats.listenPort)
			: std::wstring{ ResourceGetString(L"RuntimeStatusNotListening").c_str() });
		row(
			ResourceGetString(L"RuntimeStatusBtUdpMappedPort"),
			mapping.udpExternalPort > 0
			? std::to_wstring(mapping.udpExternalPort)
			: std::wstring{ ResourceGetString(L"RuntimeStatusNoConfirmedMapping").c_str() });
		row(
			ResourceGetString(L"RuntimeStatusUtpConnections"),
			FormatRuntimeText(L"RuntimeStatusUtpConnectionsValue", {
				std::to_wstring(metric("utp.num_utp_connected")),
				std::to_wstring(metric("utp.num_utp_syn_sent")),
				std::to_wstring(metric("utp.num_utp_fin_sent")),
				std::to_wstring(metric("utp.num_utp_close_wait")),
				std::to_wstring(metric("utp.num_utp_idle"))}));
		row(
			ResourceGetString(L"RuntimeStatusRemoteAccessPort"),
			std::to_wstring(webPort));
		row(
			ResourceGetString(L"RuntimeStatusRemoteAccessConnections"),
			std::to_wstring(webStats.activeConnections));
		row(
			ResourceGetString(L"RuntimeStatusLsdPort"),
			torrentSettings.enableLsd
			? ResourceGetString(L"RuntimeStatusLsdPortEnabled")
			: ResourceGetString(L"CommonDisabled"));
		row(ResourceGetString(L"RuntimeStatusWindowsFirewallState"), GetFirewallState());
		row(
			ResourceGetString(L"RuntimeStatusUpnpNatMapping"),
			mapping.upnpEnabled ? ResourceGetString(L"CommonEnabled") : ResourceGetString(L"CommonDisabled"));
		row(
			ResourceGetString(L"RuntimeStatusNatPmpMapping"),
			mapping.natPmpEnabled ? ResourceGetString(L"CommonEnabled") : ResourceGetString(L"CommonDisabled"));
		row(
			ResourceGetString(L"RuntimeStatusPortMappingMechanisms"),
			FormatRuntimeText(L"RuntimeStatusPortMappingMechanismsValue", {
				mapping.tcpMechanism.empty()
					? std::wstring{ ResourceGetString(L"RuntimeStatusNoneConfirmed").c_str() }
					: std::wstring{ to_hstring(mapping.tcpMechanism).c_str() },
				mapping.udpMechanism.empty()
					? std::wstring{ ResourceGetString(L"RuntimeStatusNoneConfirmed").c_str() }
					: std::wstring{ to_hstring(mapping.udpMechanism).c_str() }}));
		row(
			ResourceGetString(L"RuntimeStatusTcpUdpMappedPorts"),
			std::format(
				L"{} / {}",
				mapping.tcpExternalPort,
				mapping.udpExternalPort));
		row(
			ResourceGetString(L"RuntimeStatusPortMappingError"),
			mapping.lastError.empty()
			? ResourceGetString(L"CommonNone")
			: to_hstring(mapping.lastError).c_str());

		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionTransfer").c_str()));
		row(
			ResourceGetString(L"RuntimeStatusOverallDownloadRate"),
			FormatBytes(std::max<std::int64_t>(0, stats.totalDownloadRate)
						+ httpDown) + L"/s");
		row(
			ResourceGetString(L"RuntimeStatusOverallUploadRate"),
			FormatBytes(std::max<std::int64_t>(0, stats.totalUploadRate)
						+ httpUp) + L"/s");
		row(
			ResourceGetString(L"RuntimeStatusBtDownloadUploadRate"),
			std::format(
				L"{} / {}",
				FormatBytes(std::max<std::int64_t>(
					0, stats.totalDownloadRate)) + L"/s",
				FormatBytes(std::max<std::int64_t>(
					0, stats.totalUploadRate)) + L"/s"));
		row(
			ResourceGetString(L"RuntimeStatusHttpDownloadUploadRate"),
			std::format(
				L"{} / {}",
				FormatBytes(httpDown) + L"/s",
				FormatBytes(httpUp) + L"/s"));
		row(
			ResourceGetString(L"RuntimeStatusDownloadUploadLimits"),
			std::format(
				L"{} / {}",
				LimitText(torrentSettings.downloadRateLimit),
				LimitText(torrentSettings.uploadRateLimit)));
		row(
			ResourceGetString(L"RuntimeStatusActiveTaskLimits"),
			FormatRuntimeText(L"RuntimeStatusActiveTaskLimitsValue", {
				std::to_wstring(torrentSettings.activeDownloads),
				std::to_wstring(torrentSettings.activeSeeds),
				std::to_wstring(torrentSettings.activeLimit)}));
		row(
			ResourceGetString(L"RuntimeStatusMaximumPeerListPerTask"),
			std::to_wstring(torrentSettings.maxPeerListSize));
		row(
			ResourceGetString(L"RuntimeStatusTrackerReceiveSendRate"),
			std::format(
				L"{} / {}",
				FormatBytes(metricRate("net.recv_tracker_bytes")) + L"/s",
				FormatBytes(metricRate("net.sent_tracker_bytes")) + L"/s"));
		row(
			ResourceGetString(L"RuntimeStatusTrackerReceiveSendTotal"),
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_tracker_bytes")),
				FormatBytes(metric("net.sent_tracker_bytes"))));
		row(
			ResourceGetString(L"RuntimeStatusBtMetadataMessages"),
			FormatRuntimeText(L"RuntimeStatusReceivedSentValue", {
				std::to_wstring(metric("ses.num_incoming_metadata")),
				std::to_wstring(metric("ses.num_outgoing_metadata"))}));
		row(
			ResourceGetString(L"RuntimeStatusBtUploadSlots"),
			FormatRuntimeText(L"RuntimeStatusBtUploadSlotsValue", {
				std::to_wstring(metric("peer.num_peers_up_unchoked")),
				std::to_wstring(metric("ses.num_unchoke_slots"))}));
		row(
			ResourceGetString(L"RuntimeStatusSeedingUploadRate"),
			FormatBytes(std::max<std::int64_t>(
				0, stats.longTermSeedingUploadRate)) + L"/s");
		row(
			ResourceGetString(L"RuntimeStatusRemoteAccessTransferRate"),
			FormatRuntimeText(L"RuntimeStatusReceiveSendValue", {
				FormatBytes(webReceiveRate) + L"/s",
				FormatBytes(webSendRate) + L"/s"}));
		row(
			ResourceGetString(L"RuntimeStatusRemoteAccessTransferTotal"),
			FormatRuntimeText(L"RuntimeStatusReceiveSendValue", {
				FormatBytes(webStats.requestBytes),
				FormatBytes(webStats.responseBytes)}));
		row(ResourceGetString(L"RuntimeStatusDhtNodesIpv4"), std::to_wstring(stats.dhtNodes));
		row(
			ResourceGetString(L"RuntimeStatusDhtRoutingCacheTorrentsPeers"),
			FormatRuntimeText(L"RuntimeStatusTripleCountsValue", {
				std::to_wstring(metric("dht.dht_node_cache")),
				std::to_wstring(metric("dht.dht_torrents")),
				std::to_wstring(metric("dht.dht_peers"))}));
		row(
			ResourceGetString(L"RuntimeStatusDnsResolverState"),
			ResourceGetString(L"RuntimeStatusDnsResolverStateValue"));

		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionProcessMemory").c_str()));
		row(
			ResourceGetString(L"RuntimeStatusCpuUsage"),
			FormatRuntimeText(L"RuntimeStatusCpuUsageValue", {
				std::format(L"{:.2f}", cpu),
				std::to_wstring(GetPhysicalCoreCount()),
				std::to_wstring(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS))}));
		row(ResourceGetString(L"RuntimeStatusMemoryWorkingSet"), FormatBytes(memory.WorkingSetSize));
		row(ResourceGetString(L"RuntimeStatusMemoryCommitPrivate"), FormatBytes(memory.PrivateUsage));
		row(
			ResourceGetString(L"RuntimeStatusProcessHeap"),
			FormatRuntimeText(L"RuntimeStatusProcessHeapValue", {
				FormatBytes(heap.busyBytes),
				FormatBytes(heap.overheadBytes),
				std::to_wstring(heap.heapCount)}));
		row(ResourceGetString(L"RuntimeStatusProcessHandleCount"), std::to_wstring(handleCount));
		row(
			ResourceGetString(L"RuntimeStatusLibtorrentDiskCache"),
			FormatBytes(std::max<std::int64_t>(0, stats.diskCacheBytes)));
		row(
			ResourceGetString(L"RuntimeStatusDiskWriteBuffer"),
			FormatRuntimeText(L"RuntimeStatusDiskWriteBufferValue", {
				FormatBytes(std::max<std::int64_t>(0, metric("disk.queued_write_bytes"))),
				FormatBytes(std::max(0, runtimeSettings.maxQueuedDiskBytes))}));
		row(
			ResourceGetString(L"RuntimeStatusTcpUdpSocketBuffers"),
			FormatRuntimeText(L"RuntimeStatusSocketBuffersValue", {
				std::to_wstring(runtimeSettings.receiveSocketBufferSize),
				std::to_wstring(runtimeSettings.sendSocketBufferSize)}));
		row(
			ResourceGetString(L"RuntimeStatusApplicationModelBuffers"),
			ResourceGetString(L"RuntimeStatusApplicationModelBuffersValue"));
		row(
			ResourceGetString(L"RuntimeStatusLoggingBuffers"),
			ResourceGetString(L"RuntimeStatusLoggingBuffersValue"));
		row(
			ResourceGetString(L"RuntimeStatusReservedVirtualMemory"),
			FormatRuntimeText(L"RuntimeStatusReservedVirtualMemoryValue", {
				FormatBytes(memory.PrivateUsage)}));
		row(
			ResourceGetString(L"RuntimeStatusFreePhysicalMemory"),
			FormatBytes(systemMemory.ullAvailPhys));
		row(
			ResourceGetString(L"RuntimeStatusFreeVirtualMemory"),
			FormatBytes(systemMemory.ullAvailVirtual));
		row(
			ResourceGetString(L"RuntimeStatusFreeProcessAddressSpace"),
			FormatBytes(freeAddressSpace));

		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionStorageDiskCache").c_str()));
		row(
			ResourceGetString(L"RuntimeStatusStorageTotal"),
			FormatBytes(diskTotal.QuadPart));
		row(
			ResourceGetString(L"RuntimeStatusStorageFree"),
			FormatBytes(diskFree.QuadPart));
		row(ResourceGetString(L"RuntimeStatusVolumeList"), GetVolumeSummary());
		row(
			ResourceGetString(L"RuntimeStatusDiskCacheDataSize"),
			FormatRuntimeText(L"RuntimeStatusDiskCacheDataSizeValue", {
				FormatBytes(std::max<std::int64_t>(0, stats.diskCacheBytes)),
				std::to_wstring(metric("disk.disk_blocks_in_use"))}));
		row(
			ResourceGetString(L"RuntimeStatusLibtorrentDiskReadWrite"),
			std::format(
				L"{} / {}",
				FormatBytes(std::max<std::int64_t>(
					0, metric("disk.num_blocks_read")) * 16 * 1024),
				FormatBytes(std::max<std::int64_t>(
					0, metric("disk.num_blocks_written")) * 16 * 1024)));
		row(
			ResourceGetString(L"RuntimeStatusLibtorrentReadWriteOperations"),
			std::format(
				L"{} / {}",
				metric("disk.num_read_ops"),
				metric("disk.num_write_ops")));
		row(
			ResourceGetString(L"RuntimeStatusLibtorrentDiskJobs"),
			FormatRuntimeText(L"RuntimeStatusDiskJobsValue", {
				std::to_wstring(metric("disk.queued_disk_jobs")),
				std::to_wstring(metric("disk.num_running_disk_jobs")),
				std::to_wstring(metric("disk.blocked_disk_jobs")),
				std::to_wstring(metric("disk.num_read_jobs")),
				std::to_wstring(metric("disk.num_write_jobs"))}));
		row(
			ResourceGetString(L"RuntimeStatusLibtorrentDiskTiming"),
			FormatRuntimeText(L"RuntimeStatusDiskTimingValue", {
				std::to_wstring(metric("disk.disk_read_time") / 1000),
				std::to_wstring(metric("disk.disk_write_time") / 1000),
				std::to_wstring(metric("disk.disk_hash_time") / 1000),
				std::to_wstring(metric("disk.request_latency"))}));
		row(
			ResourceGetString(L"RuntimeStatusProcessDiskReadTotal"),
			FormatRuntimeText(L"RuntimeStatusProcessDiskIoValue", {
				FormatBytes(ioCounters.ReadTransferCount),
				std::to_wstring(ioCounters.ReadOperationCount)}));
		row(
			ResourceGetString(L"RuntimeStatusProcessDiskWriteTotal"),
			FormatRuntimeText(L"RuntimeStatusProcessDiskIoValue", {
				FormatBytes(ioCounters.WriteTransferCount),
				std::to_wstring(ioCounters.WriteOperationCount)}));
		row(
			ResourceGetString(L"RuntimeStatusPersistedHttpCompletedData"),
			FormatBytes(httpCompletedBytes));
		row(
			ResourceGetString(L"RuntimeStatusLongTermSeedDiskBoost"),
			notApplicable);

		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionSessionTotals").c_str()));
		row(
			ResourceGetString(L"RuntimeStatusTotalDownloadedSession"),
			FormatBytes(std::max<std::int64_t>(0, stats.totalDownloaded)));
		row(
			ResourceGetString(L"RuntimeStatusTotalUploadedSession"),
			FormatBytes(std::max<std::int64_t>(0, stats.totalUploaded)));
		row(
			ResourceGetString(L"RuntimeStatusNetworkPayloadReceiveSend"),
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_payload_bytes")),
				FormatBytes(metric("net.sent_payload_bytes"))));
		row(
			ResourceGetString(L"RuntimeStatusNetworkProtocolReceiveSend"),
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_bytes")),
				FormatBytes(metric("net.sent_bytes"))));
		row(
			ResourceGetString(L"RuntimeStatusIpOverheadReceiveSend"),
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_ip_overhead_bytes")),
				FormatBytes(metric("net.sent_ip_overhead_bytes"))));
		row(
			ResourceGetString(L"RuntimeStatusFailedRedundantDownload"),
			std::format(
				L"{} / {}",
				FormatBytes(metric("net.recv_failed_bytes")),
				FormatBytes(metric("net.recv_redundant_bytes"))));
		row(
			ResourceGetString(L"RuntimeStatusPieceChecksPassedFailed"),
			std::format(
				L"{} / {}",
				metric("ses.num_piece_passed"),
				metric("ses.num_piece_failed")));
		row(
			ResourceGetString(L"RuntimeStatusPersistedHttpDownloaded"),
			FormatBytes(httpCompletedBytes));
		row(
			ResourceGetString(L"RuntimeStatusPersistedP2pDownloaded"),
			FormatBytes(persistedP2PDownloaded));
		row(
			ResourceGetString(L"RuntimeStatusPersistedCombinedDownloaded"),
			FormatBytes(persistedP2PDownloaded + httpCompletedBytes));

		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionUdpDhtDiagnostics").c_str()));
		row(
			ResourceGetString(L"RuntimeStatusDhtBytesReceiveSend"),
			std::format(
				L"{} / {}",
				FormatBytes(stats.dhtBytesReceived),
				FormatBytes(stats.dhtBytesSent)));
		row(
			ResourceGetString(L"RuntimeStatusDhtReceiveSendRate"),
			std::format(
				L"{} / {}",
				FormatBytes(metricRate("dht.dht_bytes_in")) + L"/s",
				FormatBytes(metricRate("dht.dht_bytes_out")) + L"/s"));
		row(
			ResourceGetString(L"RuntimeStatusDhtMessagesReceiveSend"),
			std::format(
				L"{} / {}",
				metric("dht.dht_messages_in"),
				metric("dht.dht_messages_out")));
		row(
			ResourceGetString(L"RuntimeStatusDhtDroppedReceiveSend"),
			std::format(
				L"{} / {}",
				metric("dht.dht_messages_in_dropped"),
				metric("dht.dht_messages_out_dropped")));
		row(
			ResourceGetString(L"RuntimeStatusDhtRequestTypesReceiveSend"),
			FormatRuntimeText(L"RuntimeStatusDhtRequestTypesValue", {
				std::to_wstring(metric("dht.dht_ping_in")),
				std::to_wstring(metric("dht.dht_ping_out")),
				std::to_wstring(metric("dht.dht_find_node_in")),
				std::to_wstring(metric("dht.dht_find_node_out")),
				std::to_wstring(metric("dht.dht_get_peers_in")),
				std::to_wstring(metric("dht.dht_get_peers_out")),
				std::to_wstring(metric("dht.dht_announce_peer_in")),
				std::to_wstring(metric("dht.dht_announce_peer_out")),
				std::to_wstring(metric("dht.dht_get_in")),
				std::to_wstring(metric("dht.dht_get_out")),
				std::to_wstring(metric("dht.dht_put_in")),
				std::to_wstring(metric("dht.dht_put_out"))}));
		row(
			ResourceGetString(L"RuntimeStatusDhtInvalidRequests"),
			std::to_wstring(sumPrefix("dht.dht_invalid_")));
		row(
			ResourceGetString(L"RuntimeStatusDhtPendingRpcObservers"),
			std::to_wstring(metric("dht.dht_allocated_observers")));
		row(
			ResourceGetString(L"RuntimeStatusDhtUploadRateLimit"),
			LimitText(runtimeSettings.dhtUploadRateLimit));
		row(
			ResourceGetString(L"RuntimeStatusUtpPacketsReceiveSend"),
			std::format(
				L"{} / {}",
				metric("utp.utp_packets_in"),
				metric("utp.utp_packets_out")));
		row(
			ResourceGetString(L"RuntimeStatusUtpPayloadReceiveSend"),
			std::format(
				L"{} / {}",
				metric("utp.utp_payload_pkts_in"),
				metric("utp.utp_payload_pkts_out")));
		row(
			ResourceGetString(L"RuntimeStatusUtpLossTimeoutResend"),
			std::format(
				L"{} / {} / {}",
				metric("utp.utp_packet_loss"),
				metric("utp.utp_timeout"),
				metric("utp.utp_packet_resend")));
		row(
			ResourceGetString(L"RuntimeStatusUdpTrackerTraffic"),
			FormatRuntimeText(L"RuntimeStatusUdpTrackerTrafficValue", {
				FormatBytes(metric("net.recv_tracker_bytes")),
				FormatBytes(metric("net.sent_tracker_bytes"))}));
		row(
			ResourceGetString(L"RuntimeStatusNatDetection"),
			FormatRuntimeText(L"RuntimeStatusNatDetectionValue", {
				mapping.externalAddress.empty()
				? std::wstring{ ResourceGetString(L"RuntimeStatusNotObserved").c_str() }
				: std::wstring{ to_hstring(mapping.externalAddress).c_str() },
				std::to_wstring(mapping.tcpExternalPort),
				std::to_wstring(mapping.udpExternalPort)}));
		row(
			ResourceGetString(L"RuntimeStatusIpDetection"),
			metric("net.has_incoming_connections") != 0
			? ResourceGetString(L"RuntimeStatusIncomingConnectivityObserved")
			: ResourceGetString(L"RuntimeStatusNoIncomingConnectivityObserved"));
		row(
			ResourceGetString(L"RuntimeStatusDnsFailureDomainCounts"),
			ResourceGetString(L"RuntimeStatusResolverDetailsUnavailable"));

		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionSchedulersTrackers").c_str()));
		row(
			ResourceGetString(L"RuntimeStatusPeerMessagesReceiveSend"),
			std::format(
				L"{} / {}",
				sumPrefix("ses.num_incoming_"),
				sumPrefix("ses.num_outgoing_")));
		row(
			ResourceGetString(L"RuntimeStatusRateLimiterQueues"),
			FormatRuntimeText(L"RuntimeStatusRateLimiterQueuesValue", {
				std::to_wstring(metric("net.limiter_up_queue")),
				FormatBytes(metric("net.limiter_up_bytes")),
				std::to_wstring(metric("net.limiter_down_queue")),
				FormatBytes(metric("net.limiter_down_bytes"))}));
		row(
			ResourceGetString(L"RuntimeStatusNetworkEventWakeups"),
			FormatRuntimeText(L"RuntimeStatusNetworkEventWakeupsValue", {
				std::to_wstring(metric("net.on_read_counter")),
				std::to_wstring(metric("net.on_write_counter")),
				std::to_wstring(metric("net.on_tick_counter")),
				std::to_wstring(metric("net.on_udp_counter")),
				std::to_wstring(metric("net.on_accept_counter")),
				std::to_wstring(metric("net.on_disk_counter"))}));
		row(
			ResourceGetString(L"RuntimeStatusQueuedTrackerAnnounces"),
			std::to_wstring(
				metric("tracker.num_queued_tracker_announces")));
		row(
			ResourceGetString(L"RuntimeStatusTrackerConnectionDetails"),
			FormatRuntimeText(L"RuntimeStatusTrackerConnectionDetailsValue", {
				std::to_wstring(metric("tracker.num_queued_tracker_announces")),
				FormatBytes(metric("net.recv_tracker_bytes")),
				FormatBytes(metric("net.sent_tracker_bytes"))}));
		row(
			ResourceGetString(L"RuntimeStatusDiskScheduler"),
			FormatRuntimeText(L"RuntimeStatusDiskSchedulerValue", {
				std::to_wstring(metric("disk.queued_disk_jobs")),
				std::to_wstring(metric("disk.num_running_disk_jobs")),
				std::to_wstring(metric("disk.blocked_disk_jobs"))}));
		row(
			ResourceGetString(L"RuntimeStatusTimerMessageImplementation"),
			ResourceGetString(L"RuntimeStatusTimerMessageImplementationValue"));

		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionBitCometCoverage").c_str()), false);
		row(ResourceGetString(L"RuntimeStatusLtSeedProtocol"), notApplicable);
		row(ResourceGetString(L"RuntimeStatusLtSeedUdpQueues"), notApplicable);
		row(ResourceGetString(L"RuntimeStatusDiskBoostService"), notApplicable);
		row(ResourceGetString(L"RuntimeStatusBitCometMessageQueues"), notApplicable);
		row(ResourceGetString(L"RuntimeStatusBitCometTimerQueues"), notApplicable);
		row(
			ResourceGetString(L"RuntimeStatusOpenNetEquivalents"),
			ResourceGetString(L"RuntimeStatusOpenNetEquivalentsValue"));

		section(std::wstring(ResourceGetString(L"RuntimeStatusSectionRawLibtorrentMetrics").c_str()), false);
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
