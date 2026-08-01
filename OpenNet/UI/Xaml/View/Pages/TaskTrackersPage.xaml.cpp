#include "XamlWorkaround.h"
import winrt.XamlToolkit.Labs.WinUI;
#include "TaskTrackersPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskTrackersPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskTrackersPage.g.cpp"
#endif
#include "ViewModels/DisplayItems.h"

import OpenNet.Core.P2PManager;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.ColumnWidthHelper;
import OpenNet.UI.Xaml.Control.DataTableSortHelper;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.Foundation.Collections;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace ::OpenNet::Helpers;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TaskTrackersPage::TaskTrackersPage()
	{
		m_trackerItems = winrt::single_threaded_observable_vector<
			winrt::Windows::Foundation::IInspectable>();
	}

	void TaskTrackersPage::InitializeComponent()
	{
		TaskTrackersPageT::InitializeComponent();
		TrackersListView().ItemsSource(m_trackerItems);
		UpdateSortHeaders();
		Loaded([this](auto, auto)
		{
			RestoreColumn(ColTrackerTier(), "Trackers.Tier");
			RestoreColumn(ColTrackerPeers(), "Trackers.Peers");
			RestoreColumn(ColTrackerStatus(), "Trackers.Status");
			RestoreColumn(ColTrackerMessage(), "Trackers.Message");
		});
		Unloaded([this](auto, auto)
		{
			SaveColumnWidth("Trackers.Tier", ColTrackerTier());
			SaveColumnWidth("Trackers.Peers", ColTrackerPeers());
			SaveColumnWidth("Trackers.Status", ColTrackerStatus());
			SaveColumnWidth("Trackers.Message", ColTrackerMessage());
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
			m_timerTickToken = m_refreshTimer.Tick(
				{ this, &TaskTrackersPage::OnRefreshTimerTick });
		}
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		m_refreshTimer.Interval(std::chrono::milliseconds(
			std::clamp<std::int64_t>(
				database.GetInt("ui", "refresh_interval_ms").value_or(1000),
				100,
				60000)));
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

	void TaskTrackersPage::ColumnHeader_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto button = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::Button>();
		if (!button || !button.Tag()) return;
		auto const column = winrt::unbox_value<winrt::hstring>(button.Tag());
		if (m_sortColumn == column)
			m_sortDirection = (m_sortDirection + 1) % 3;
		else
		{
			m_sortColumn = column;
			m_sortDirection = 1;
		}
		UpdateSortHeaders();
		RefreshTrackerList();
	}

	void TaskTrackersPage::UpdateSortHeaders()
	{
		auto update = [this](auto const& button)
		{
			::OpenNet::UI::Xaml::Control::DataTableSortHelper::UpdateHeader(
				button, m_sortColumn, m_sortDirection);
		};
		update(SortTrackerUrlButton());
		update(SortTrackerTierButton());
		update(SortTrackerPeersButton());
		update(SortTrackerStatusButton());
		update(SortTrackerMessageButton());
	}

	void TaskTrackersPage::ColumnHeader_RightTapped(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args)
	{
		m_contextColumn = nullptr;
		auto source = args.OriginalSource().try_as<DependencyObject>();
		while (source)
		{
			if (auto column = source.try_as<winrt::XamlToolkit::Labs::WinUI::DataColumn>())
			{
				m_contextColumn = column;
				break;
			}
			source = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(source);
		}
	}

	void TaskTrackersPage::ColumnMenu_Opening(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Windows::Foundation::IInspectable const&)
	{
		AutoSizeSelectedColumnItem().IsEnabled(m_contextColumn != nullptr);
		if (auto menu = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::MenuFlyout>())
		{
			for (auto const& entry : menu.Items())
			{
				if (auto toggle = entry.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>())
				{
					auto column = ColumnForTag(
						winrt::unbox_value<winrt::hstring>(toggle.Tag()));
					toggle.IsChecked(column && column.Visibility() == Visibility::Visible);
				}
			}
		}
	}

	winrt::XamlToolkit::Labs::WinUI::DataColumn TaskTrackersPage::ColumnForTag(
		winrt::hstring const& tag)
	{
		if (tag == L"URL") return ColTrackerURL();
		if (tag == L"Tier") return ColTrackerTier();
		if (tag == L"Peers") return ColTrackerPeers();
		if (tag == L"Status") return ColTrackerStatus();
		if (tag == L"Message") return ColTrackerMessage();
		return nullptr;
	}

	void TaskTrackersPage::ColumnVisibility_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto toggle = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>();
		if (!toggle || !toggle.Tag()) return;
		if (auto column = ColumnForTag(winrt::unbox_value<winrt::hstring>(toggle.Tag())))
			column.Visibility(toggle.IsChecked() ? Visibility::Visible : Visibility::Collapsed);
	}

	void TaskTrackersPage::AutoSizeColumn(
		winrt::XamlToolkit::Labs::WinUI::DataColumn const& column)
	{
		if (!column) return;
		column.DesiredWidth(GridLengthHelper::Auto());
		column.InvalidateMeasure();
		TrackersListView().InvalidateMeasure();
	}

	void TaskTrackersPage::AutoSizeSelectedColumn_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		AutoSizeColumn(m_contextColumn);
	}

	void TaskTrackersPage::AutoSizeAllColumns_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		for (auto const& column : std::array{
			ColTrackerURL(), ColTrackerTier(), ColTrackerPeers(),
			ColTrackerStatus(), ColTrackerMessage() })
			AutoSizeColumn(column);
	}

	void TaskTrackersPage::ResetColumns_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
	{
		m_sortColumn = {};
		m_sortDirection = 0;
		UpdateSortHeaders();
		for (auto const& column : std::array{
			ColTrackerURL(), ColTrackerTier(), ColTrackerPeers(),
			ColTrackerStatus(), ColTrackerMessage() })
			column.Visibility(Visibility::Visible);
		AutoSizeAllColumns_Click(sender, args);
		RefreshTrackerList();
	}

	void TaskTrackersPage::RefreshTrackerList()
	{
		auto listView = TrackersListView();
		auto emptyText = EmptyStateText();
		if (!listView) return;

		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			m_trackerItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto selectedTask = m_viewModel.SelectedTask();
		auto taskType = selectedTask.TaskType();

		if (taskType != winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
		{
			m_trackerItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto taskId = winrt::to_string(selectedTask.TaskId());
		if (taskId.empty())
		{
			m_trackerItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto& p2p = ::OpenNet::Core::P2PManager::Instance();
		if (!p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore())
		{
			m_trackerItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto detail = p2p.TorrentCore()->GetTorrentDetail(taskId);

		if (detail.trackers.empty())
		{
			m_trackerItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		if (m_sortDirection != 0)
		{
			auto const direction = m_sortDirection;
			auto const column = m_sortColumn;
			std::stable_sort(detail.trackers.begin(), detail.trackers.end(),
							 [direction, column](auto const& left, auto const& right)
			{
				auto ascending = [&]()
				{
					if (column == L"URL") return left.url < right.url;
					if (column == L"Tier") return left.tier < right.tier;
					if (column == L"Peers") return left.numPeers < right.numPeers;
					if (column == L"Status") return left.status < right.status;
					return left.message < right.message;
				};
				auto descending = [&]()
				{
					if (column == L"URL") return right.url < left.url;
					if (column == L"Tier") return right.tier < left.tier;
					if (column == L"Peers") return right.numPeers < left.numPeers;
					if (column == L"Status") return right.status < left.status;
					return right.message < left.message;
				};
				return direction == 1 ? ascending() : descending();
			});
		}

		for (std::uint32_t index = 0;
			 index < static_cast<std::uint32_t>(detail.trackers.size());
			 ++index)
		{
			auto const& tracker = detail.trackers[index];
			auto const trackerUrl = winrt::to_hstring(tracker.url);
			winrt::OpenNet::ViewModels::TrackerDisplayItem item{ nullptr };
			std::uint32_t existingIndex = index;
			while (existingIndex < m_trackerItems.Size())
			{
				auto candidate = m_trackerItems.GetAt(existingIndex)
					.try_as<winrt::OpenNet::ViewModels::TrackerDisplayItem>();
				if (candidate && candidate.URL() == trackerUrl)
				{
					item = candidate;
					break;
				}
				++existingIndex;
			}

			if (item && existingIndex != index)
			{
				m_trackerItems.RemoveAt(existingIndex);
				m_trackerItems.InsertAt(index, item);
			}
			else if (!item)
			{
				item = winrt::make<
					winrt::OpenNet::ViewModels::implementation::TrackerDisplayItem>();
				m_trackerItems.InsertAt(index, item);
			}
			item.URL(trackerUrl);
			item.Tier(winrt::to_hstring(tracker.tier));
			item.Peers(winrt::to_hstring(tracker.numPeers));
			item.Status(winrt::to_hstring(tracker.status));
			item.Message(winrt::to_hstring(tracker.message));
		}
		while (m_trackerItems.Size() > detail.trackers.size())
			m_trackerItems.RemoveAtEnd();

		if (emptyText) emptyText.Visibility(Visibility::Collapsed);
	}
}
