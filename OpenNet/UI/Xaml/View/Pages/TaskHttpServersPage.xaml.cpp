#include "XamlWorkaround.h"
#include "TaskHttpServersPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskHttpServersPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskHttpServersPage.g.cpp"
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

		hstring StatusText(::OpenNet::Core::Aria2::DownloadStatus const status)
		{
			using Status = ::OpenNet::Core::Aria2::DownloadStatus;
			if (status == Status::Active) return L"Downloading";
			if (status == Status::Paused) return L"Paused";
			if (status == Status::Complete) return L"Complete";
			if (status == Status::Error) return L"Error";
			if (status == Status::Removed) return L"Removed";
			return L"Waiting";
		}
	}

	TaskHttpServersPage::TaskHttpServersPage()
	{
		InitializeComponent();
		m_items = single_threaded_observable_vector<Windows::Foundation::IInspectable>();
		ServersListView().ItemsSource(m_items);
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

	void TaskHttpServersPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args)
	{
		Unsubscribe();
		m_isActive.store(true, std::memory_order_release);
		m_viewModel = args.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel) m_viewModel = DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (m_viewModel)
		{
			DataContext(m_viewModel);
			m_viewModelToken = m_viewModel.PropertyChanged({ this, &TaskHttpServersPage::OnViewModelPropertyChanged });
		}
		StartRefreshTimer();
		RefreshAsync();
	}

	void TaskHttpServersPage::OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		m_isActive.store(false, std::memory_order_release);
		m_generation.fetch_add(1, std::memory_order_relaxed);
		StopRefreshTimer();
		Unsubscribe();
	}

	void TaskHttpServersPage::StartRefreshTimer()
	{
		StopRefreshTimer();
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
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

	void TaskHttpServersPage::StopRefreshTimer() noexcept
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

	void TaskHttpServersPage::Unsubscribe()
	{
		if (m_viewModel && m_viewModelToken.value) m_viewModel.PropertyChanged(m_viewModelToken);
		m_viewModelToken = {};
		m_viewModel = nullptr;
	}

	void TaskHttpServersPage::OnViewModelPropertyChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() != L"SelectedTask") return;
		m_generation.fetch_add(1, std::memory_order_relaxed);
		m_items.Clear();
		RefreshAsync();
	}

	winrt::fire_and_forget TaskHttpServersPage::RefreshAsync()
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
		auto dispatcher = DispatcherQueue();
		auto weak = get_weak();
		co_await resume_background();
		auto information = ::OpenNet::Core::DownloadManager::Instance().GetHttpTaskInformation(gid);
		auto servers = ::OpenNet::Core::DownloadManager::Instance().GetHttpTaskServers(gid);
		dispatcher.TryEnqueue([weak, information = std::move(information), servers = std::move(servers), generation]()
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

			struct ServerRow
			{
				std::string url;
				std::uint64_t speed{};
				std::size_t connections{};
			};
			std::vector<ServerRow> rows;
			std::unordered_map<std::string, std::size_t> rowByUrl;
			for (auto const& file : servers)
			{
				for (auto const& server : file.Servers)
				{
					auto const url = server.CurrentUri.empty() ? server.Uri : server.CurrentUri;
					auto const [entry, inserted] = rowByUrl.try_emplace(url, rows.size());
					if (inserted) rows.push_back({ url });
					rows[entry->second].speed += server.DownloadSpeed;
					++rows[entry->second].connections;
				}
			}
			if (rows.empty())
			{
				for (auto const& file : information->Files)
				{
					for (auto const& uri : file.Uris)
					{
						if (rowByUrl.try_emplace(uri.Uri, rows.size()).second) rows.push_back({ uri.Uri });
					}
				}
			}
			auto const pieceMap = ::OpenNet::Core::Aria2::BuildPieceMapInformation(*information);
			std::wstring pieces;
			pieces.reserve(pieceMap.States.size());
			for (auto const state : pieceMap.States) pieces.push_back(static_cast<wchar_t>(L'0' + state));
			auto const finished = static_cast<std::size_t>(std::count(pieceMap.States.begin(), pieceMap.States.end(), 2));
			auto const progress = pieceMap.States.empty()
				? 0.0
				: 100.0 * static_cast<double>(finished) / pieceMap.States.size();
			while (self->m_items.Size() < rows.size())
			{
				self->m_items.Append(make<winrt::OpenNet::ViewModels::implementation::HttpServerDisplayItem>());
			}
			while (self->m_items.Size() > rows.size()) self->m_items.RemoveAtEnd();
			auto const totalConnections = static_cast<std::size_t>((std::max)(0, information->Connections));
			for (std::size_t index = 0; index < rows.size(); ++index)
			{
				auto const item = self->m_items.GetAt(static_cast<std::uint32_t>(index)).as<winrt::OpenNet::ViewModels::HttpServerDisplayItem>();
				auto const& row = rows[index];
				item.Key(to_hstring(row.url));
				item.URL(to_hstring(row.url));
				item.Connections(hstring{ std::format(L"{} / {}", row.connections, (std::max)(row.connections, totalConnections)) });
				item.DownloadRate(FormatBytes(row.speed) + L"/s");
				item.DownloadSize(FormatBytes(information->CompletedLength));
				item.Pieces(hstring{ pieces });
				item.Progress(hstring{ std::format(L"{:.1f}%", progress) });
				item.Status(StatusText(information->Status));
			}
			self->EmptyStateText().Visibility(rows.empty() ? Visibility::Visible : Visibility::Collapsed);
		});
	}
}
