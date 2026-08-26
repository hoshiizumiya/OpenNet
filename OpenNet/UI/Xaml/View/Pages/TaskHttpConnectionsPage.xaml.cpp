#include "XamlWorkaround.h"
#include "TaskHttpConnectionsPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskHttpConnectionsPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskHttpConnectionsPage.g.cpp"
#endif

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Aria2.Aria2Models;
import OpenNet.Core.DownloadManager;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	namespace
	{
		struct ConnectionProgress
		{
			double start{};
			double end{ 100.0 };
			double value{};
			std::uint64_t completedBytes{};
		};

		hstring FormatBytes(std::uint64_t const bytes)
		{
			constexpr std::array units{ L"B", L"KiB", L"MiB", L"GiB", L"TiB" };
			double value = static_cast<double>(bytes);
			std::size_t unit{};
			while (value >= 1024.0 && unit + 1 < units.size())
			{
				value /= 1024.0;
				++unit;
			}
			return hstring{ std::format(L"{:.1f} {}", value, units[unit]) };
		}

		hstring FormatRate(std::uint64_t const bytesPerSecond)
		{
			return FormatBytes(bytesPerSecond) + L"/s";
		}

		hstring StatusText(::OpenNet::Core::Aria2::DownloadInformation const& information, bool const active)
		{
			using Status = ::OpenNet::Core::Aria2::DownloadStatus;
			if (information.Status == Status::Paused) return L"Paused";
			if (information.Status == Status::Complete) return L"Complete";
			if (information.Status == Status::Error) return L"Error";
			if (information.Status == Status::Removed) return L"Removed";
			return active ? L"Active" : L"Waiting";
		}

		ConnectionProgress GetConnectionProgress(::OpenNet::Core::Aria2::DownloadInformation const& information, std::size_t const index, std::size_t const count)
		{
			ConnectionProgress result;
			if (count == 0) return result;
			auto const pieceMap = ::OpenNet::Core::Aria2::BuildPieceMapInformation(information);
			auto const pieceCount = (std::max)(std::size_t{ 1 }, pieceMap.States.size());
			auto const first = pieceCount * index / count;
			auto const last = pieceCount * (index + 1) / count;
			auto const segmentCount = (std::max)(std::size_t{ 1 }, last - first);
			std::size_t completed{};
			for (auto piece = first; piece < last && piece < pieceMap.States.size(); ++piece)
			{
				if (pieceMap.States[piece] == 2) ++completed;
			}
			result.start = 100.0 * static_cast<double>(first) / pieceCount;
			result.end = 100.0 * static_cast<double>(last) / pieceCount;
			result.value = 100.0 * static_cast<double>(completed) / segmentCount;
			result.completedBytes = (std::min<std::uint64_t>)(completed * pieceMap.PieceLength, information.TotalLength);
			return result;
		}
	}

	TaskHttpConnectionsPage::TaskHttpConnectionsPage()
	{
		InitializeComponent();
		m_items = single_threaded_observable_vector<Windows::Foundation::IInspectable>();
		ConnectionsListView().ItemsSource(m_items);
		auto weak = get_weak();
		Unloaded([weak](auto const&, auto const&)
		{
			if (auto self = weak.get())
			{
				self->m_isActive.store(false, std::memory_order_release);
				self->m_generation.fetch_add(1, std::memory_order_relaxed);
				self->StopRefreshTimer();
				self->Unsubscribe();
			}
		});
	}

	void TaskHttpConnectionsPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args)
	{
		Unsubscribe();
		m_isActive.store(true, std::memory_order_release);
		m_viewModel = args.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel) m_viewModel = DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (m_viewModel)
		{
			DataContext(m_viewModel);
			m_viewModelToken = m_viewModel.PropertyChanged({ this, &TaskHttpConnectionsPage::OnViewModelPropertyChanged });
		}
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		m_configuredConnections = static_cast<std::size_t>(std::clamp<std::int64_t>(database.GetInt(
			::OpenNet::Core::AppSettingsDatabase::CAT_DOWNLOAD, "aria2_connections_per_server", 8), 1, 16));
		StartRefreshTimer();
		RefreshAsync();
	}

	void TaskHttpConnectionsPage::OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		m_isActive.store(false, std::memory_order_release);
		m_generation.fetch_add(1, std::memory_order_relaxed);
		StopRefreshTimer();
		Unsubscribe();
	}

	void TaskHttpConnectionsPage::StartRefreshTimer()
	{
		StopRefreshTimer();
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		m_refreshTimer = DispatcherTimer{};
		m_refreshTimer.Interval(std::chrono::milliseconds(std::clamp<std::int64_t>(
			database.GetInt("ui", "refresh_interval_ms").value_or(1000), 250, 60000)));
		auto weak = get_weak();
		m_timerToken = m_refreshTimer.Tick([weak](auto const&, auto const&)
		{
			if (auto self = weak.get()) self->RefreshAsync();
		});
		m_refreshTimer.Start();
	}

	void TaskHttpConnectionsPage::StopRefreshTimer() noexcept
	{
		if (!m_refreshTimer) return;
		try
		{
			m_refreshTimer.Stop();
			if (m_timerToken.value) m_refreshTimer.Tick(m_timerToken);
		}
		catch (...)
		{
		}
		m_timerToken = {};
		m_refreshTimer = nullptr;
	}

	void TaskHttpConnectionsPage::Unsubscribe()
	{
		if (m_viewModel && m_viewModelToken.value) m_viewModel.PropertyChanged(m_viewModelToken);
		m_viewModelToken = {};
		m_viewModel = nullptr;
	}

	void TaskHttpConnectionsPage::OnViewModelPropertyChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() != L"SelectedTask") return;
		m_generation.fetch_add(1, std::memory_order_relaxed);
		m_items.Clear();
		RefreshAsync();
	}

	winrt::fire_and_forget TaskHttpConnectionsPage::RefreshAsync()
	{
		if (!m_isActive.load(std::memory_order_acquire) || m_refreshInFlight.exchange(true)) co_return;
		auto const task = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
		if (!task || task.TaskType() != winrt::OpenNet::ViewModels::DownloadTaskType::Http)
		{
			m_refreshInFlight.store(false, std::memory_order_release);
			m_items.Clear();
			EmptyStateText().Visibility(Visibility::Visible);
			co_return;
		}
		auto const gid = to_string(task.Gid());
		auto const generation = m_generation.load(std::memory_order_relaxed);
		auto const configuredConnections = m_configuredConnections;
		auto dispatcher = DispatcherQueue();
		auto weak = get_weak();
		co_await resume_background();
		auto information = ::OpenNet::Core::DownloadManager::Instance().GetHttpTaskInformation(gid);
		auto servers = ::OpenNet::Core::DownloadManager::Instance().GetHttpTaskServers(gid);
		dispatcher.TryEnqueue([weak, information = std::move(information), servers = std::move(servers), generation, configuredConnections]()
		{
			auto self = weak.get();
			if (!self) return;
			self->m_refreshInFlight.store(false, std::memory_order_release);
			if (!self->m_isActive.load(std::memory_order_acquire)
				|| self->m_generation.load(std::memory_order_relaxed) != generation) return;
			if (!information)
			{
				self->m_items.Clear();
				self->EmptyStateText().Visibility(Visibility::Visible);
				return;
			}

			std::vector<::OpenNet::Core::Aria2::ServerInformation> serverRows;
			for (auto const& file : servers)
			{
				for (auto const& server : file.Servers) serverRows.push_back(server);
			}
			std::vector<std::string> urls;
			for (auto const& file : information->Files)
			{
				for (auto const& uri : file.Uris) urls.push_back(uri.Uri);
			}
			auto const activeConnections = static_cast<std::size_t>((std::max)(0, information->Connections));
			auto const rowCount = activeConnections > 0 ? activeConnections : configuredConnections;
			while (self->m_items.Size() < rowCount)
			{
				self->m_items.Append(make<winrt::OpenNet::ViewModels::implementation::HttpConnectionDisplayItem>());
			}
			while (self->m_items.Size() > rowCount) self->m_items.RemoveAtEnd();
			for (std::size_t index = 0; index < rowCount; ++index)
			{
				auto const item = self->m_items.GetAt(static_cast<std::uint32_t>(index)).as<winrt::OpenNet::ViewModels::HttpConnectionDisplayItem>();
				auto const server = serverRows.empty() ? nullptr : &serverRows[index % serverRows.size()];
				auto const url = server && !server->CurrentUri.empty()
					? server->CurrentUri
					: server && !server->Uri.empty()
					? server->Uri
					: urls.empty() ? std::string{} : urls[index % urls.size()];
				auto const progress = GetConnectionProgress(*information, index, rowCount);
				auto const speed = server ? server->DownloadSpeed
					: activeConnections > 0 ? information->DownloadSpeed / activeConnections : std::size_t{};
				item.Key(to_hstring(std::format("connection:{}", index)));
				item.URL(to_hstring(url));
				item.DownloadRate(FormatRate(speed));
				item.DownloadSize(FormatBytes(progress.completedBytes));
				item.RangeStart(progress.start);
				item.RangeEnd(progress.end);
				item.ProgressValue(progress.value);
				item.Progress(hstring{ std::format(L"{:.1f}%", progress.value) });
				item.Status(StatusText(*information, index < activeConnections));
			}
			self->EmptyStateText().Visibility(rowCount == 0 ? Visibility::Visible : Visibility::Collapsed);
		});
	}
}
