#include "pch.h"
#include "TaskTrackersPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskTrackersPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskTrackersPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "Core/P2PManager.h"
#include "ViewModels/DisplayItems.h"
#include "Helpers/ColumnWidthHelper.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace ::OpenNet::Helpers;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TaskTrackersPage::TaskTrackersPage()
	{
		InitializeComponent();
		Loaded([this](auto, auto)
		{
			RestoreColumn(ColTrackerTier(), "Trackers.Tier");
			RestoreColumn(ColTrackerPeers(), "Trackers.Peers");
			RestoreColumn(ColTrackerStatus(), "Trackers.Status");
			RestoreColumn(ColTrackerMessage(), "Trackers.Message");
		});
		Unloaded([this](auto, auto)
		{
			SaveColumnWidth("Trackers.Tier", ColTrackerTier().ActualWidth());
			SaveColumnWidth("Trackers.Peers", ColTrackerPeers().ActualWidth());
			SaveColumnWidth("Trackers.Status", ColTrackerStatus().ActualWidth());
			SaveColumnWidth("Trackers.Message", ColTrackerMessage().ActualWidth());
		});
	}

	TaskTrackersPage::~TaskTrackersPage()
	{
		Unsubscribe();
		if (m_refreshTimer)
		{
			m_refreshTimer.Stop();
			m_refreshTimer.Tick(m_timerTickToken);
			m_refreshTimer = nullptr;
		}
	}

	void TaskTrackersPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		Unsubscribe();

		m_viewModel = e.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel)
			m_viewModel = this->DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();

		if (m_viewModel)
		{
			this->DataContext(m_viewModel);
			m_vmPropertyChangedToken = m_viewModel.PropertyChanged(
				{ this, &TaskTrackersPage::OnViewModelPropertyChanged });
		}

		if (!m_refreshTimer)
		{
			m_refreshTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
			m_refreshTimer.Interval(std::chrono::seconds(3));
			m_timerTickToken = m_refreshTimer.Tick(
				{ this, &TaskTrackersPage::OnRefreshTimerTick });
		}
		m_refreshTimer.Start();

		RefreshTrackerList();
	}

	void TaskTrackersPage::OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		if (m_refreshTimer)
		{
			m_refreshTimer.Stop();
		}
		Unsubscribe();
	}

	void TaskTrackersPage::Unsubscribe()
	{
		if (m_viewModel && m_vmPropertyChangedToken.value)
		{
			m_viewModel.PropertyChanged(m_vmPropertyChangedToken);
			m_vmPropertyChangedToken = {};
		}
		m_viewModel = nullptr;
	}

	void TaskTrackersPage::OnViewModelPropertyChanged(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() == L"SelectedTask")
		{
			RefreshTrackerList();
		}
	}

	void TaskTrackersPage::OnRefreshTimerTick(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Windows::Foundation::IInspectable const&)
	{
		RefreshTrackerList();
	}

	void TaskTrackersPage::RefreshTrackerList()
	{
		auto listView = TrackersListView();
		auto emptyText = EmptyStateText();
		if (!listView) return;

		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			listView.ItemsSource(nullptr);
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto selectedTask = m_viewModel.SelectedTask();
		auto taskType = selectedTask.TaskType();

		if (taskType != winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
		{
			listView.ItemsSource(nullptr);
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto taskId = winrt::to_string(selectedTask.TaskId());
		if (taskId.empty())
		{
			listView.ItemsSource(nullptr);
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto& p2p = ::OpenNet::Core::P2PManager::Instance();
		if (!p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore())
		{
			listView.ItemsSource(nullptr);
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto detail = p2p.TorrentCore()->GetTorrentDetail(taskId);

		if (detail.trackers.empty())
		{
			listView.ItemsSource(nullptr);
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto items = winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();

		for (auto const& tracker : detail.trackers)
		{
			auto item = winrt::make<winrt::OpenNet::ViewModels::implementation::TrackerDisplayItem>();
			item.URL(winrt::to_hstring(tracker.url));
			item.Tier(winrt::to_hstring(tracker.tier));
			item.Peers(winrt::to_hstring(tracker.numPeers));
			item.Status(winrt::to_hstring(tracker.status));
			item.Message(winrt::to_hstring(tracker.message));
			items.Append(item);
		}

		listView.ItemsSource(items);
		if (emptyText) emptyText.Visibility(Visibility::Collapsed);
	}
}
