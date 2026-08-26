#include <Windows.h>

#include "XamlWorkaround.h"
#include "TaskSummaryPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskSummaryPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskSummaryPage.g.cpp"
#endif

import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.Aria2.Aria2Models;
import OpenNet.Core.DownloadManager;
import OpenNet.Core.P2PManager;
import OpenNet.Core.Utils.Message;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TaskSummaryPage::TaskSummaryPage()
	{
		InitializeComponent();
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

	TaskSummaryPage::~TaskSummaryPage()
	{
	}

	void TaskSummaryPage::OnNavigatedTo(
		winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& args)
	{
		Unsubscribe();
		m_isActive.store(true, std::memory_order_release);
		m_viewModel = args.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel)
		{
			m_viewModel = DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		}
		if (m_viewModel)
		{
			DataContext(m_viewModel);
			m_vmPropertyChangedToken = m_viewModel.PropertyChanged(
				{ this, &TaskSummaryPage::OnViewModelPropertyChanged });
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
		RefreshSummary();
	}

	void TaskSummaryPage::OnNavigatedFrom(
		winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		m_isActive.store(false, std::memory_order_release);
		StopRefreshTimer();
		Unsubscribe();
	}

	void TaskSummaryPage::StopRefreshTimer() noexcept
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

	void TaskSummaryPage::RefreshSummary()
	{
		if (!m_isActive.load(std::memory_order_acquire)) return;
		auto task = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
		if (!task)
		{
			if (!m_graphTaskId.empty())
			{
				TaskSpeedGraph().Reset();
				m_graphTaskId = {};
			}
			ResetSummary();
			return;
		}
		if (m_graphTaskId != task.TaskId())
		{
			TaskSpeedGraph().Reset();
			m_graphTaskId = task.TaskId();
		}

		ResetSummary();
		TaskNameText().Text(task.Name());
		GeneralTaskNameText().Text(task.Name());
		TaskProgress().Value(task.ProgressPercent());
		TaskProgressText().Text(std::format(L"{:.1f}%", task.ProgressPercent()));
		DownloadedText().Text(task.DownloadSize());
		UploadedText().Text(task.UploadSize());
		DownloadSpeedText().Text(task.DownloadRate());
		UploadSpeedText().Text(task.UploadRate());
		RemainingSizeText().Text(task.Remaining());
		AddedOnText().Text(task.AddDate());
		TaskSizeText().Text(task.Size());

		if (task.TaskType() != winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
		{
			TaskSpeedGraph().SetSpeed(
				task.ProgressPercent(),
				task.DownloadSpeedKB() * 1024);
			TaskPathText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeHttpDownload"));
			SavePathText().Text(L"—");
			TaskStatusText().Text(task.ProgressPercent() >= 100.0 ? ResourceGetString(L"TaskStatusCompleted") : ResourceGetString(L"TaskStatusDownloading"));
			auto const information = ::OpenNet::Core::DownloadManager::Instance().GetHttpTaskInformation(to_string(task.Gid()));
			if (information)
			{
				auto const pieceMap = ::OpenNet::Core::Aria2::BuildPieceMapInformation(*information);
				auto const finished = static_cast<std::size_t>(std::count(pieceMap.States.begin(), pieceMap.States.end(), 2));
				std::size_t sourceCount{};
				for (auto const& file : information->Files) sourceCount += file.Uris.size();
				std::wstring downloadedStates;
				downloadedStates.reserve(pieceMap.States.size());
				for (auto const state : pieceMap.States) downloadedStates.push_back(static_cast<wchar_t>(L'0' + state));
				std::wstring availableStates(pieceMap.States.size(), sourceCount > 0 ? L'2' : L'0');
				DownloadedPiecesText().Text(std::format(L"{} / {}", finished, pieceMap.States.size()));
				AvailablePiecesText().Text(std::format(L"{} / {}", sourceCount > 0 ? pieceMap.States.size() : 0, pieceMap.States.size()));
				DownloadedPiecesProgress().Pieces(hstring{ downloadedStates });
				AvailablePiecesProgress().Pieces(hstring{ availableStates });
				DownloadedPiecesProgress().Visibility(Visibility::Visible);
				AvailablePiecesProgress().Visibility(Visibility::Visible);
			}
			else
			{
				DownloadedPiecesText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeNA"));
				AvailablePiecesText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeNA"));
				DownloadedPiecesProgress().Pieces(L"");
				AvailablePiecesProgress().Pieces(L"");
				DownloadedPiecesProgress().Visibility(Visibility::Collapsed);
				AvailablePiecesProgress().Visibility(Visibility::Collapsed);
			}
			return;
		}
		DownloadedPiecesProgress().Visibility(Visibility::Visible);
		AvailablePiecesProgress().Visibility(Visibility::Visible);

		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (!core)
		{
			TaskSpeedGraph().SetSpeed(
				task.ProgressPercent(),
				task.DownloadSpeedKB() * 1024);
			TaskStatusText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeEngineUnavailable"));
			return;
		}

		auto const taskId = to_string(task.TaskId());
		auto const detail = core->GetTorrentSummary(taskId);
		auto const pieces = core->GetTorrentPieceSummary(taskId);
		auto const progress = detail.progressPpm / 10000.0;
		TaskSpeedGraph().SetSpeed(
			progress,
			detail.downloadRate > 0
			? static_cast<std::uint64_t>(detail.downloadRate)
			: std::uint64_t{});

		TaskNameText().Text(to_hstring(detail.name));
		GeneralTaskNameText().Text(to_hstring(detail.name));
		TaskPathText().Text(to_hstring(detail.savePath));
		SavePathText().Text(to_hstring(detail.savePath));
		TaskStatusText().Text(TorrentStateText(detail.state, detail.isPaused));
		TaskProgress().Value(progress);
		TaskProgressText().Text(std::format(L"{:.1f}%", progress));
		DownloadedText().Text(FormatBytes(detail.totalDone));
		UploadedText().Text(FormatBytes(detail.totalUploaded));
		DownloadSpeedText().Text(FormatRate(detail.downloadRate));
		UploadSpeedText().Text(FormatRate(detail.uploadRate));
		TimeElapsedText().Text(FormatDuration(detail.activeTimeSeconds));
		SeedingTimeText().Text(FormatDuration(detail.seedingTimeSeconds));
		auto const downloadLimit = core->GetTorrentDownloadLimit(taskId);
		auto const uploadLimit = core->GetTorrentUploadLimit(taskId);
		DownloadLimitText().Text(
			downloadLimit > 0
			? FormatRate(downloadLimit)
			: hstring{ L"Unlimited" });
		UploadLimitText().Text(
			uploadLimit > 0
			? FormatRate(uploadLimit)
			: hstring{ L"Unlimited" });
		SeedsText().Text(
			detail.numComplete >= 0
			? std::format(
				L"{} of {} connected",
				detail.numSeeds,
				std::max(detail.numSeeds, detail.numComplete))
			: std::format(L"{} connected", detail.numSeeds));
		PeersText().Text(
			detail.numIncomplete >= 0
			? std::format(
				L"{} of {} connected",
				detail.numPeers,
				std::max(detail.numPeers, detail.numIncomplete))
			: std::format(L"{} connected", detail.numPeers));
		ConnectionsText().Text(std::format(L"{}", detail.numConnections));
		ShareRatioText().Text(std::format(L"{:.2f}", detail.shareRatio));

		bool const longTermSeeding = detail.state == 4 || detail.state == 5;
		LongTermUploadSizeText().Text(
			longTermSeeding
			? FormatBytes(detail.totalUploaded)
			: hstring{ L"Not seeding" });
		LongTermUploadSpeedText().Text(
			longTermSeeding
			? FormatRate(detail.uploadRate)
			: hstring{ L"0 B/s" });
		LongTermShareRatioText().Text(
			longTermSeeding
			? hstring{ std::format(L"{:.2f}", detail.shareRatio) }
		: hstring{ L"—" });

		TaskSizeText().Text(
			std::format(
				L"{} / {}",
				FormatBytes(detail.totalDone).c_str(),
				FormatBytes(detail.totalSize).c_str()));
		RemainingSizeText().Text(
			FormatBytes(std::max<std::int64_t>(0, detail.totalSize - detail.totalDone)));
		InfoHashV1Text().Text(
			detail.infoHashV1.empty()
			? hstring{ L"N/A" }
		: to_hstring(detail.infoHashV1));
		InfoHashV2Text().Text(
			detail.infoHashV2.empty()
			? hstring{ L"N/A" }
		: to_hstring(detail.infoHashV2));
		PieceHashesText().Text(
			!detail.infoHashV2.empty() ? L"SHA-256" :
			!detail.infoHashV1.empty() ? L"SHA-1" : L"N/A");
		AddedOnText().Text(FormatTimestamp(detail.addedTimestamp));
		FinishedOnText().Text(FormatTimestamp(detail.completedTimestamp));
		PiecesText().Text(
			std::format(
				L"{} × {}",
				detail.piecesNum,
				FormatBytes(detail.pieceSize).c_str()));
		NumberOfFilesText().Text(std::format(L"{}", detail.files.size()));
		FileAlignmentText().Text(
			detail.isPieceAligned ? L"Piece-aligned" : L"Not piece-aligned");
		QueuePositionText().Text(
			detail.queuePosition >= 0
			? hstring{ std::format(L"{}", detail.queuePosition + 1) }
		: hstring{ L"N/A" });
		CreatedByText().Text(
			detail.creator.empty()
			? hstring{ L"N/A" }
		: to_hstring(detail.creator));
		CreatedOnText().Text(FormatTimestamp(detail.creationTimestamp));
		PrivateTorrentText().Text(detail.isPrivate ? ResourceGetString(L"CommonYes") : ResourceGetString(L"CommonNo"));
		DescriptionText().Text(
			detail.comment.empty()
			? hstring{ L"—" }
		: to_hstring(detail.comment));

		std::vector<hstring> flags;
		if (detail.isPaused) flags.emplace_back(L"Paused");
		if (detail.isAutoManaged) flags.emplace_back(L"Auto managed");
		if (detail.isSequential) flags.emplace_back(L"Sequential");
		if (detail.isSuperSeeding) flags.emplace_back(L"Super seeding");
		if (detail.firstLastPiecePriority) flags.emplace_back(L"First/last piece priority");
		std::wstring flagText;
		for (auto const& flag : flags)
		{
			if (!flagText.empty()) flagText.append(L", ");
			flagText.append(flag.c_str());
		}
		TaskFlagsText().Text(flagText.empty() ? L"None" : hstring{ flagText });

		std::size_t finished{};
		std::size_t available{};
		for (std::size_t index = 0; index < pieces.states.size(); ++index)
		{
			if (pieces.states[index] == 2) ++finished;
			if (index < pieces.availability.size() && pieces.availability[index] > 0)
				++available;
		}
		auto const totalPieces = pieces.states.size();
		std::wstring downloadedStates;
		std::wstring availableStates;
		downloadedStates.reserve(totalPieces);
		availableStates.reserve(totalPieces);
		for (std::size_t index = 0; index < totalPieces; ++index)
		{
			downloadedStates.push_back(static_cast<wchar_t>(L'0' + std::clamp(pieces.states[index], 0, 4)));
			availableStates.push_back(index < pieces.availability.size() && pieces.availability[index] > 0 ? L'2' : L'0');
		}
		DownloadedPiecesText().Text(
			std::format(L"{} / {}", finished, totalPieces));
		AvailablePiecesText().Text(
			std::format(L"{} / {}", available, totalPieces));
		DownloadedPiecesProgress().Value(
			totalPieces ? finished * 100.0 / totalPieces : 0.0);
		AvailablePiecesProgress().Value(
			totalPieces ? available * 100.0 / totalPieces : 0.0);
		DownloadedPiecesProgress().Pieces(hstring{ downloadedStates });
		AvailablePiecesProgress().Pieces(hstring{ availableStates });
	}

	void TaskSummaryPage::ResetSummary()
	{
		TaskNameText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeNoTaskSelected"));
		GeneralTaskNameText().Text(L"—");
		TaskPathText().Text(L"—");
		TaskStatusText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeIdle"));
		TaskProgress().Value(0);
		TaskProgressText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntime0"));
		DownloadedPiecesText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntime00"));
		AvailablePiecesText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntime00"));
		DownloadedPiecesProgress().Pieces(L"");
		AvailablePiecesProgress().Pieces(L"");
		DownloadedPiecesProgress().Visibility(Visibility::Collapsed);
		AvailablePiecesProgress().Visibility(Visibility::Collapsed);
		TimeElapsedText().Text(L"—");
		DownloadedText().Text(L"—");
		DownloadSpeedText().Text(L"—");
		DownloadLimitText().Text(L"—");
		SeedingTimeText().Text(L"—");
		UploadedText().Text(L"—");
		UploadSpeedText().Text(L"—");
		UploadLimitText().Text(L"—");
		SeedsText().Text(L"—");
		PeersText().Text(L"—");
		ConnectionsText().Text(L"—");
		ShareRatioText().Text(L"—");
		LongTermUploadSizeText().Text(L"—");
		LongTermUploadSpeedText().Text(L"—");
		LongTermShareRatioText().Text(L"—");
		SavePathText().Text(L"—");
		TaskSizeText().Text(L"—");
		InfoHashV1Text().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeNA"));
		InfoHashV2Text().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeNA"));
		PieceHashesText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeNA"));
		AddedOnText().Text(L"—");
		FinishedOnText().Text(L"—");
		PublisherText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeNA"));
		TagsText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeNone"));
		RemainingSizeText().Text(L"—");
		PiecesText().Text(L"—");
		NumberOfFilesText().Text(L"—");
		FileAlignmentText().Text(L"—");
		QueuePositionText().Text(L"—");
		CreatedByText().Text(L"—");
		CreatedOnText().Text(L"—");
		PrivateTorrentText().Text(L"—");
		TaskFlagsText().Text(ResourceGetString(L"ViewTaskSummaryPageRuntimeNone"));
		DescriptionText().Text(L"—");
	}

	void TaskSummaryPage::Unsubscribe()
	{
		if (m_viewModel && m_vmPropertyChangedToken.value)
		{
			m_viewModel.PropertyChanged(m_vmPropertyChangedToken);
			m_vmPropertyChangedToken = {};
		}
		m_viewModel = nullptr;
	}

	void TaskSummaryPage::OnViewModelPropertyChanged(
		IInspectable const&,
		winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() == L"SelectedTask")
		{
			RefreshSummary();
		}
	}

	void TaskSummaryPage::OnRefreshTimerTick(
		IInspectable const&,
		IInspectable const&)
	{
		if (!m_isActive.load(std::memory_order_acquire)) return;
		RefreshSummary();
	}

	hstring TaskSummaryPage::FormatBytes(std::int64_t value)
	{
		double amount = static_cast<double>(std::max<std::int64_t>(0, value));
		constexpr std::array units{ L"B", L"KiB", L"MiB", L"GiB", L"TiB" };
		std::size_t unit{};
		while (amount >= 1024.0 && unit + 1 < units.size())
		{
			amount /= 1024.0;
			++unit;
		}
		return hstring{ std::format(L"{:.1f} {}", amount, units[unit]) };
	}

	hstring TaskSummaryPage::FormatRate(std::int64_t value)
	{
		return FormatBytes(value) + L"/s";
	}

	hstring TaskSummaryPage::FormatDuration(std::int64_t seconds)
	{
		if (seconds <= 0)
		{
			return L"0 s";
		}
		auto const days = seconds / 86400;
		seconds %= 86400;
		auto const hours = seconds / 3600;
		seconds %= 3600;
		auto const minutes = seconds / 60;
		seconds %= 60;
		if (days > 0)
		{
			return hstring{ std::format(
				L"{} d {:02}:{:02}:{:02}",
				days,
				hours,
				minutes,
				seconds) };
		}
		return hstring{ std::format(
			L"{:02}:{:02}:{:02}",
			hours,
			minutes,
			seconds) };
	}

	hstring TaskSummaryPage::FormatTimestamp(std::int64_t timestamp)
	{
		if (timestamp <= 0)
		{
			return L"N/A";
		}
		constexpr std::int64_t UnixToFileTimeSeconds = 11644473600LL;
		auto const ticks = static_cast<std::uint64_t>(
			timestamp + UnixToFileTimeSeconds) * 10000000ULL;
		ULARGE_INTEGER value{};
		value.QuadPart = ticks;
		FILETIME utc{ value.LowPart, value.HighPart };
		FILETIME localFileTime{};
		SYSTEMTIME local{};
		if (!::FileTimeToLocalFileTime(&utc, &localFileTime)
			|| !::FileTimeToSystemTime(&localFileTime, &local))
		{
			return hstring{ std::format(L"{}", timestamp) };
		}
		return hstring{ std::format(
			L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
			local.wYear,
			local.wMonth,
			local.wDay,
			local.wHour,
			local.wMinute,
			local.wSecond) };
	}

	hstring TaskSummaryPage::TorrentStateText(int state, bool paused)
	{
		if (paused)
		{
			return L"Paused";
		}
		switch (state)
		{
			case 0: return L"Checking resume data";
			case 1: return L"Checking files";
			case 2: return L"Downloading metadata";
			case 3: return L"Downloading";
			case 4: return L"Finished";
			case 5: return L"Seeding";
			case 6: return L"Allocating";
			case 7: return L"Checking fast resume";
			default: return L"Unknown";
		}
	}
}
