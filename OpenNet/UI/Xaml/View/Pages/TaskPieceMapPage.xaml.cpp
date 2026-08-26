#include "XamlWorkaround.h"
#include "TaskPieceMapPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskPieceMapPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskPieceMapPage.g.cpp"
#endif

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Aria2.Aria2Models;
import OpenNet.Core.P2PManager;
import OpenNet.Core.DownloadManager;
import OpenNet.Core.Utils.Message;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.UI;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Windows::UI;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TaskPieceMapPage::~TaskPieceMapPage()
	{
	}

	void TaskPieceMapPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args)
	{
		Unsubscribe();
		m_isActive.store(true, std::memory_order_release);
		if (!m_unloadedHandlerRegistered)
		{
			m_unloadedHandlerRegistered = true;
			auto weak = get_weak();
			Unloaded([weak](auto const&, auto const&)
			{
				if (auto self = weak.get())
				{
					self->m_isActive.store(false, std::memory_order_release);
					self->StopRefreshTimer();
					self->Unsubscribe();
				}
			});
		}
		m_viewModel = args.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel)
		{
			m_viewModel = DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		}
		if (m_viewModel)
		{
			DataContext(m_viewModel);
			m_vmPropertyChangedToken = m_viewModel.PropertyChanged(
				{ this, &TaskPieceMapPage::OnViewModelPropertyChanged });
		}

		if (!m_refreshTimer)
		{
			m_refreshTimer = DispatcherTimer{};
			auto weak = get_weak();
			m_timerTickToken = m_refreshTimer.Tick([weak](auto const& sender, auto const& timerArgs)
			{
				if (auto self = weak.get()) self->OnRefreshTimerTick(sender, timerArgs);
			});
		}
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		m_refreshTimer.Interval(std::chrono::milliseconds(
			std::clamp<std::int64_t>(
				database.GetInt("ui", "refresh_interval_ms").value_or(1000),
				250,
				60000)));
		m_refreshTimer.Start();
		RefreshPieceMap();
	}

	void TaskPieceMapPage::OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		m_isActive.store(false, std::memory_order_release);
		StopRefreshTimer();
		Unsubscribe();
	}

	void TaskPieceMapPage::StopRefreshTimer() noexcept
	{
		if (!m_refreshTimer) return;
		try
		{
			m_refreshTimer.Stop();
			if (m_timerTickToken.value) m_refreshTimer.Tick(m_timerTickToken);
		}
		catch (...)
		{
		}
		m_timerTickToken = {};
		m_refreshTimer = nullptr;
	}

	void TaskPieceMapPage::RefreshButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		m_renderedStates.clear();
		RefreshPieceMap();
	}

	void TaskPieceMapPage::RefreshPieceMap()
	{
		if (!m_isActive.load(std::memory_order_acquire)) return;
		auto task = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
		if (task && task.TaskType() == winrt::OpenNet::ViewModels::DownloadTaskType::Http)
		{
			auto const information = ::OpenNet::Core::DownloadManager::Instance().GetHttpTaskInformation(to_string(task.Gid()));
			if (!information)
			{
				PieceMapSubtitle().Text(ResourceGetString(L"ViewTaskPieceMapPageMetadataPending"));
				return;
			}
			auto const pieceMap = ::OpenNet::Core::Aria2::BuildPieceMapInformation(*information);
			std::size_t sourceCount{};
			for (auto const& file : information->Files) sourceCount += file.Uris.size();
			std::vector<int> availability(pieceMap.States.size(), sourceCount > 0 ? 1 : 0);
			RenderPieceMap(task.TaskId(), task.Name(), pieceMap.PieceLength, pieceMap.States, availability, std::vector<std::string>{}, sourceCount);
			return;
		}
		if (!task || task.TaskType() != winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
		{
			PieceGrid().Items().Clear();
			PieceMapSubtitle().Text(ResourceGetString(L"ViewTaskPieceMapPageBitTorrentOnly"));
			DownloadedPiecesText().Text(L"0 / 0");
			AvailablePiecesText().Text(L"0 / 0");
			DownloadedPiecesProgress().Value(0);
			AvailablePiecesProgress().Value(0);
			DownloadedPiecesProgress().Pieces(L"");
			AvailablePiecesProgress().Pieces(L"");
			m_renderedTaskId.clear();
			m_renderedStates.clear();
			m_renderedAvailability.clear();
			return;
		}

		auto const taskId = task.TaskId();
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (!core)
		{
			return;
		}

		auto const pieces = core->GetTorrentPieceInfo(to_string(taskId));
		if (pieces.states.empty())
		{
			PieceGrid().Items().Clear();
			PieceMapSubtitle().Text(ResourceGetString(L"ViewTaskPieceMapPageMetadataPending"));
			return;
		}

		RenderPieceMap(taskId, task.Name(), pieces.pieceSize, pieces.states, pieces.availability, pieces.hashes, pieces.webSeeds.size());
	}

	void TaskPieceMapPage::RenderPieceMap(
		hstring const& taskId,
		hstring const& taskName,
		std::size_t const pieceSize,
		std::vector<int> const& states,
		std::vector<int> const& availability,
		std::vector<std::string> const& hashes,
		std::size_t const sourceCount)
	{
		std::size_t finished{};
		std::size_t available{};
		for (std::size_t index = 0; index < states.size(); ++index)
		{
			if (states[index] == 2)
			{
				++finished;
			}
			if (index < availability.size() && availability[index] > 0)
			{
				++available;
			}
		}

		auto const total = states.size();
		std::wstring downloadedStates;
		std::wstring availableStates;
		downloadedStates.reserve(total);
		availableStates.reserve(total);
		for (std::size_t index = 0; index < total; ++index)
		{
			downloadedStates.push_back(static_cast<wchar_t>(L'0' + std::clamp(states[index], 0, 4)));
			availableStates.push_back(index < availability.size() && availability[index] > 0 ? L'2' : L'0');
		}
		DownloadedPiecesText().Text(std::format(L"{} / {}", finished, total));
		AvailablePiecesText().Text(std::format(L"{} / {}", available, total));
		DownloadedPiecesProgress().Value(total ? finished * 100.0 / total : 0.0);
		AvailablePiecesProgress().Value(total ? available * 100.0 / total : 0.0);
		DownloadedPiecesProgress().Pieces(hstring{ downloadedStates });
		AvailablePiecesProgress().Pieces(hstring{ availableStates });
		PieceSizeText().Text(ResourceGetString(L"ViewTaskPieceMapPagePieceSize") + L" " + FormatBytes(pieceSize));
		WebSeedCountText().Text(ResourceGetString(L"ViewTaskPieceMapPageWebSeeds") + L" " + std::to_wstring(sourceCount));
		PieceMapSubtitle().Text(taskName + L" · " + std::to_wstring(total) + L" " + ResourceGetString(L"ViewTaskPieceMapPagePieces"));

		if (taskId == m_renderedTaskId
			&& states == m_renderedStates
			&& availability == m_renderedAvailability)
		{
			return;
		}

		auto items = PieceGrid().Items();
		bool const canUpdateInPlace =
			taskId == m_renderedTaskId
			&& items.Size() == total
			&& m_renderedStates.size() == total
			&& m_renderedAvailability.size() == availability.size();
		if (!canUpdateInPlace)
		{
			items.Clear();
		}
		for (std::size_t index = 0; index < total; ++index)
		{
			auto const availabilityValue =
				index < availability.size() ? availability[index] : 0;
			bool const changed =
				!canUpdateInPlace
				|| states[index] != m_renderedStates[index]
				|| availabilityValue != (
					index < m_renderedAvailability.size()
					? m_renderedAvailability[index]
					: 0);
			if (!changed)
			{
				continue;
			}

			auto element = CreatePieceElement(
				index,
				states[index],
				availabilityValue,
				index < hashes.size()
				? to_hstring(hashes[index])
				: hstring{});
			if (canUpdateInPlace)
			{
				items.SetAt(static_cast<std::uint32_t>(index), element);
			}
			else
			{
				items.Append(element);
			}
		}
		m_renderedTaskId = taskId;
		m_renderedStates = states;
		m_renderedAvailability = availability;
	}

	void TaskPieceMapPage::Unsubscribe()
	{
		if (m_viewModel && m_vmPropertyChangedToken.value)
		{
			m_viewModel.PropertyChanged(m_vmPropertyChangedToken);
			m_vmPropertyChangedToken = {};
		}
		m_viewModel = nullptr;
	}

	void TaskPieceMapPage::OnViewModelPropertyChanged(IInspectable const&, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() == L"SelectedTask")
		{
			m_renderedTaskId.clear();
			m_renderedStates.clear();
			m_renderedAvailability.clear();
			RefreshPieceMap();
		}
	}

	void TaskPieceMapPage::OnRefreshTimerTick(
		IInspectable const&,
		IInspectable const&)
	{
		if (!m_isActive.load(std::memory_order_acquire)) return;
		RefreshPieceMap();
	}

	hstring TaskPieceMapPage::FormatBytes(std::int64_t value)
	{
		double amount = static_cast<double>(std::max<std::int64_t>(0, value));
		constexpr std::array units{ L"B", L"KiB", L"MiB", L"GiB" };
		std::size_t unit{};
		while (amount >= 1024.0 && unit + 1 < units.size())
		{
			amount /= 1024.0;
			++unit;
		}
		return hstring{ std::format(L"{:.1f} {}", amount, units[unit]) };
	}

	Border TaskPieceMapPage::CreatePieceElement(
		std::size_t index,
		int state,
		int availability,
		hstring const& hash)
	{
		Border piece;
		piece.Width(16);
		piece.Height(16);
		piece.Margin(Thickness{ 1 });
		piece.CornerRadius(
			winrt::Microsoft::UI::Xaml::CornerRadius{
				2.0,
				2.0,
				2.0,
				2.0 });

		Color color{};
		hstring stateName;
		switch (state)
		{
			case 1:
				color = Color{ 255, 215, 47, 154 };
				stateName = L"Downloading";
				break;
			case 2:
				color = Color{ 255, 22, 131, 216 };
				stateName = L"Finished";
				break;
			case 3:
				color = Color{ 255, 105, 105, 105 };
				stateName = L"Disabled";
				break;
			case 4:
				color = Color{ 255, 242, 200, 17 };
				stateName = L"Unchecked";
				break;
			default:
				color = Color{ 255, 205, 205, 205 };
				stateName = L"Empty";
				break;
		}
		piece.Background(SolidColorBrush{ color });
		if (state == 3)
		{
			TextBlock cross;
			cross.Text(L"×");
			cross.FontSize(11);
			cross.HorizontalAlignment(HorizontalAlignment::Center);
			cross.VerticalAlignment(VerticalAlignment::Center);
			piece.Child(cross);
		}

		auto tooltip = std::format(
			L"Piece {}\nState: {}\nAvailability: {}",
			index,
			stateName.c_str(),
			availability);
		if (!hash.empty())
		{
			tooltip.append(L"\nHash: ");
			tooltip.append(hash.c_str());
		}
		ToolTipService::SetToolTip(piece, box_value(hstring{ tooltip }));
		return piece;
	}
}
