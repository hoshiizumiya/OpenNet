#include "XamlWorkaround.h"
#include "TaskPieceMapPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskPieceMapPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskPieceMapPage.g.cpp"
#endif

import OpenNet.Core.AppSettingsDatabase;
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
		Unsubscribe();
		if (m_refreshTimer)
		{
			m_refreshTimer.Stop();
			m_refreshTimer.Tick(m_timerTickToken);
		}
	}

	void TaskPieceMapPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args)
	{
		Unsubscribe();
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
			m_timerTickToken = m_refreshTimer.Tick(
				{ this, &TaskPieceMapPage::OnRefreshTimerTick });
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
		if (m_refreshTimer)
		{
			m_refreshTimer.Stop();
		}
		Unsubscribe();
	}

	void TaskPieceMapPage::RefreshButton_Click(IInspectable const&, RoutedEventArgs const&)
	{
		m_renderedStates.clear();
		RefreshPieceMap();
	}

	void TaskPieceMapPage::RefreshPieceMap()
	{
		auto task = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
		if (task && task.TaskType() == winrt::OpenNet::ViewModels::DownloadTaskType::Http)
		{
			auto const information = ::OpenNet::Core::DownloadManager::Instance().GetHttpTaskInformation(to_string(task.Gid()));
			if (!information)
			{
				PieceGrid().Items().Clear();
				PieceMapSubtitle().Text(L"Waiting for HTTP segment information.");
				return;
			}
			auto const pieceLength = information->PieceLength > 0 ? information->PieceLength : 1024 * 1024;
			auto const total = information->NumPieces > 0 ? information->NumPieces : information->TotalLength > 0 ? (information->TotalLength + pieceLength - 1) / pieceLength : 1;
			std::vector<int> states(total, 0);
			auto const completedPieces = information->TotalLength > 0 ? (std::min<std::size_t>)(total, information->CompletedLength * total / information->TotalLength) : 0;
			for (std::size_t index = 0; index < completedPieces; ++index) states[index] = 2;
			if (completedPieces < total && information->Status == ::OpenNet::Core::Aria2::DownloadStatus::Active) states[completedPieces] = 1;
			std::vector<int> availability(total, information->Files.empty() ? 0 : 1);
			DownloadedPiecesText().Text(std::format(L"{} / {}", completedPieces, total));
			AvailablePiecesText().Text(std::format(L"{} / {}", availability.empty() ? 0 : total, total));
			DownloadedPiecesProgress().Value(total ? completedPieces * 100.0 / total : 0.0);
			AvailablePiecesProgress().Value(availability.empty() ? 0.0 : 100.0);
			PieceSizeText().Text(ResourceGetString(L"ViewTaskPieceMapPagePieceSize") + L" " + FormatBytes(pieceLength));
			std::size_t uriCount{};
			for (auto const& file : information->Files) uriCount += file.Uris.size();
			WebSeedCountText().Text(L"HTTP sources: " + std::to_wstring(uriCount));
			PieceMapSubtitle().Text(task.Name() + L" · HTTP segments · hybrid acceleration ready");
			auto items = PieceGrid().Items();
			if (task.Gid() != m_renderedTaskId || items.Size() != total) items.Clear();
			for (std::size_t index = 0; index < total; ++index)
			{
				auto element = CreatePieceElement(index, states[index], availability[index], {});
				if (items.Size() == total) items.SetAt(static_cast<std::uint32_t>(index), element); else items.Append(element);
			}
			m_renderedTaskId = task.Gid();
			m_renderedStates = std::move(states);
			m_renderedAvailability = std::move(availability);
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

		std::size_t finished{};
		std::size_t available{};
		for (std::size_t index = 0; index < pieces.states.size(); ++index)
		{
			if (pieces.states[index] == 2)
			{
				++finished;
			}
			if (index < pieces.availability.size() && pieces.availability[index] > 0)
			{
				++available;
			}
		}

		auto const total = pieces.states.size();
		DownloadedPiecesText().Text(std::format(L"{} / {}", finished, total));
		AvailablePiecesText().Text(std::format(L"{} / {}", available, total));
		DownloadedPiecesProgress().Value(total ? finished * 100.0 / total : 0.0);
		AvailablePiecesProgress().Value(total ? available * 100.0 / total : 0.0);
		PieceSizeText().Text(ResourceGetString(L"ViewTaskPieceMapPagePieceSize") + L" " + FormatBytes(pieces.pieceSize));
		WebSeedCountText().Text(ResourceGetString(L"ViewTaskPieceMapPageWebSeeds") + L" " + std::to_wstring(pieces.webSeeds.size()));
		PieceMapSubtitle().Text(task.Name() + L" · " + std::to_wstring(total) + L" " + ResourceGetString(L"ViewTaskPieceMapPagePieces"));

		if (taskId == m_renderedTaskId
			&& pieces.states == m_renderedStates
			&& pieces.availability == m_renderedAvailability)
		{
			return;
		}

		auto items = PieceGrid().Items();
		bool const canUpdateInPlace =
			taskId == m_renderedTaskId
			&& items.Size() == total
			&& m_renderedStates.size() == total
			&& m_renderedAvailability.size() == pieces.availability.size();
		if (!canUpdateInPlace)
		{
			items.Clear();
		}
		for (std::size_t index = 0; index < total; ++index)
		{
			auto const availability =
				index < pieces.availability.size() ? pieces.availability[index] : 0;
			bool const changed =
				!canUpdateInPlace
				|| pieces.states[index] != m_renderedStates[index]
				|| availability != (
					index < m_renderedAvailability.size()
					? m_renderedAvailability[index]
					: 0);
			if (!changed)
			{
				continue;
			}

			auto element = CreatePieceElement(
				index,
				pieces.states[index],
				availability,
				index < pieces.hashes.size()
				? to_hstring(pieces.hashes[index])
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
		m_renderedStates = pieces.states;
		m_renderedAvailability = pieces.availability;
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
