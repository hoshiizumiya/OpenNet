#include "XamlWorkaround.h"
import winrt.XamlToolkit.Labs.WinUI;
#include "TaskTrackersPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskTrackersPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskTrackersPage.g.cpp"
#endif
#include "ViewModels/DisplayItems.h"
#include "../Windows/TrackerLogWindow.xaml.h"

import OpenNet.Core.P2PManager;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Helpers.ColumnWidthHelper;
import OpenNet.UI.Xaml.Control.DataTableColumnVisibilityHelper;
import OpenNet.UI.Xaml.Control.DataTableSortHelper;
import OpenNet.Core.Utils.Message;
import OpenNet.Helpers.WindowHelper;
import winrt.OpenNet.UI.Xaml.View.Dialog;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.Foundation;
import winrt.Windows.Foundation.Collections;
import winrt.Windows.Globalization.DateTimeFormatting;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace ::OpenNet::Helpers;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	namespace
	{
		winrt::hstring FormatOptionalNumber(int const value)
		{
			return value < 0 ? L"—" : winrt::to_hstring(value);
		}

		winrt::hstring FormatRemaining(int seconds)
		{
			if (seconds < 0)
				return L"—";
			auto const hours = seconds / 3600;
			seconds %= 3600;
			auto const minutes = seconds / 60;
			seconds %= 60;
			if (hours > 0)
				return winrt::to_hstring(std::format("{}:{:02}:{:02}",
													 hours, minutes, seconds));
			return winrt::to_hstring(std::format("{}:{:02}", minutes, seconds));
		}

		std::string Trim(std::string value)
		{
			auto const notSpace = [](unsigned char c)
			{
				return !std::isspace(c);
			};
			value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
			value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
			return value;
		}

		winrt::hstring FormatLogStatus(
			std::int64_t const timestamp, std::string const& content)
		{
			auto const formatter = winrt::Windows::Globalization::
				DateTimeFormatting::DateTimeFormatter{ L"shortdate longtime" };
			return formatter.Format(winrt::clock::from_time_t(
				static_cast<std::time_t>(timestamp)))
				+ L": " + winrt::to_hstring(content);
		}
	}

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
			RestoreColumn(ColTrackerURL(), "Trackers.URL");
			RestoreColumn(ColTrackerRetries(), "Trackers.Retries");
			RestoreColumn(ColTrackerTimeRemaining(), "Trackers.TimeRemaining");
			RestoreColumn(ColTrackerSeeders(), "Trackers.Seeders");
			RestoreColumn(ColTrackerLeechers(), "Trackers.Leechers");
			RestoreColumn(ColTrackerDownloaded(), "Trackers.Downloaded");
			RestoreColumn(ColTrackerStatus(), "Trackers.Status");
		});
		Unloaded([this](auto, auto)
		{
			SaveColumnWidth("Trackers.URL", ColTrackerURL());
			SaveColumnWidth("Trackers.Retries", ColTrackerRetries());
			SaveColumnWidth("Trackers.TimeRemaining", ColTrackerTimeRemaining());
			SaveColumnWidth("Trackers.Seeders", ColTrackerSeeders());
			SaveColumnWidth("Trackers.Leechers", ColTrackerLeechers());
			SaveColumnWidth("Trackers.Downloaded", ColTrackerDownloaded());
			SaveColumnWidth("Trackers.Status", ColTrackerStatus());
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
			m_lastTaskId.clear();
			m_hasTrackerSnapshot = false;
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
		update(SortTrackerRetriesButton());
		update(SortTrackerTimeRemainingButton());
		update(SortTrackerSeedersButton());
		update(SortTrackerLeechersButton());
		update(SortTrackerDownloadedButton());
		update(SortTrackerStatusButton());
	}

	void TaskTrackersPage::ColumnHeader_RightTapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args)
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

	void TaskTrackersPage::ColumnMenu_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const&)
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

	winrt::XamlToolkit::Labs::WinUI::DataColumn TaskTrackersPage::ColumnForTag(winrt::hstring const& tag)
	{
		if (tag == L"URL") return ColTrackerURL();
		if (tag == L"Log") return ColTrackerLog();
		if (tag == L"Retries") return ColTrackerRetries();
		if (tag == L"TimeRemaining") return ColTrackerTimeRemaining();
		if (tag == L"Seeders") return ColTrackerSeeders();
		if (tag == L"Leechers") return ColTrackerLeechers();
		if (tag == L"Downloaded") return ColTrackerDownloaded();
		if (tag == L"Status") return ColTrackerStatus();
		return nullptr;
	}

	void TaskTrackersPage::ColumnVisibility_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto toggle = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>();
		if (!toggle || !toggle.Tag()) return;
		if (auto column = ColumnForTag(winrt::unbox_value<winrt::hstring>(toggle.Tag())))
		{
			column.Visibility(toggle.IsChecked() ? Visibility::Visible : Visibility::Collapsed);
			SynchronizeTrackerRows();
		}
	}

	void TaskTrackersPage::AutoSizeColumn(winrt::XamlToolkit::Labs::WinUI::DataColumn const& column)
	{
		if (!column) return;
		column.DesiredWidth(GridLengthHelper::Auto());
		column.InvalidateMeasure();
		TrackersListView().InvalidateMeasure();
	}

	void TaskTrackersPage::AutoSizeSelectedColumn_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		AutoSizeColumn(m_contextColumn);
	}

	void TaskTrackersPage::AutoSizeAllColumns_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		for (auto const& column : std::array{
			ColTrackerURL(), ColTrackerLog(), ColTrackerRetries(),
			ColTrackerTimeRemaining(), ColTrackerSeeders(),
			ColTrackerLeechers(), ColTrackerDownloaded(), ColTrackerStatus() })
			AutoSizeColumn(column);
	}

	void TaskTrackersPage::ResetColumns_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
	{
		m_sortColumn = {};
		m_sortDirection = 0;
		UpdateSortHeaders();
		for (auto const& column : std::array{
			ColTrackerURL(), ColTrackerLog(), ColTrackerRetries(),
			ColTrackerTimeRemaining(), ColTrackerSeeders(),
			ColTrackerLeechers(), ColTrackerDownloaded(), ColTrackerStatus() })
			column.Visibility(Visibility::Visible);
		SynchronizeTrackerRows();
		AutoSizeAllColumns_Click(sender, args);
		RefreshTrackerList();
	}

	void TaskTrackersPage::TrackerDataRow_Loaded(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		std::array const columns{
			ColTrackerURL(), ColTrackerLog(), ColTrackerRetries(),
			ColTrackerTimeRemaining(), ColTrackerSeeders(), ColTrackerLeechers(),
			ColTrackerDownloaded(), ColTrackerStatus() };
		::OpenNet::UI::Xaml::Control::DataTableColumnVisibilityHelper::SynchronizeRow(
			sender.try_as<winrt::XamlToolkit::Labs::WinUI::DataRow>(),
			columns.data(), static_cast<unsigned int>(columns.size()));
	}

	void TaskTrackersPage::SynchronizeTrackerRows()
	{
		std::array const columns{
			ColTrackerURL(), ColTrackerLog(), ColTrackerRetries(),
			ColTrackerTimeRemaining(), ColTrackerSeeders(), ColTrackerLeechers(),
			ColTrackerDownloaded(), ColTrackerStatus() };
		::OpenNet::UI::Xaml::Control::DataTableColumnVisibilityHelper::SynchronizeRealizedRows(
			TrackersListView(), columns.data(), static_cast<unsigned int>(columns.size()));
		TrackersListView().InvalidateMeasure();
	}

	bool TaskTrackersPage::TryGetTaskContext(std::string& taskId, winrt::hstring& taskName) const
	{
		if (!m_viewModel || !m_viewModel.SelectedTask()
			|| m_viewModel.SelectedTask().TaskType()
			!= winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
			return false;
		taskId = winrt::to_string(m_viewModel.SelectedTask().TaskId());
		taskName = m_viewModel.SelectedTask().Name();
		auto& p2p = ::OpenNet::Core::P2PManager::Instance();
		return !taskId.empty() && p2p.IsTorrentCoreInitialized()
			&& p2p.TorrentCore();
	}

	winrt::hstring TaskTrackersPage::SelectedTrackerUrl()
	{
		if (auto item = TrackersListView().SelectedItem().try_as<winrt::OpenNet::ViewModels::TrackerDisplayItem>())
		{
			return item.URL();
		}
		return {};
	}

	void TaskTrackersPage::TrackerRow_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const&)
	{
		if (auto element = sender.try_as<FrameworkElement>())
		{
			if (auto item = element.DataContext().try_as<
				winrt::OpenNet::ViewModels::TrackerDisplayItem>())
				TrackersListView().SelectedItem(item);
		}
	}

	void TaskTrackersPage::TrackerMenu_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const&)
	{
		auto const hasSelection = !SelectedTrackerUrl().empty();
		auto const selectedTask = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
		auto const state = selectedTask
			? selectedTask.State()
			: winrt::OpenNet::ViewModels::DownloadTaskState::Pending;
		auto const canAnnounce = hasSelection
			&& (state == winrt::OpenNet::ViewModels::DownloadTaskState::Downloading
				|| state == winrt::OpenNet::ViewModels::DownloadTaskState::Seeding);
		UpdateTrackerMenuItem().IsEnabled(canAnnounce);
		if (auto menu = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::MenuFlyout>())
		{
			for (auto const& entry : menu.Items())
			{
				if (auto item = entry.try_as<
					winrt::Microsoft::UI::Xaml::Controls::MenuFlyoutItem>())
				{
					auto const text = item.Text();
					if (item != UpdateTrackerMenuItem()
						&& (text == L"Remove tracker"
							|| text == L"View log" || text == L"Clear log"))
						item.IsEnabled(hasSelection);
				}
			}
		}
	}

	void TaskTrackersPage::OpenTrackerLog(winrt::hstring const& trackerUrl)
	{
		std::string taskId;
		winrt::hstring taskName;
		if (trackerUrl.empty() || !TryGetTaskContext(taskId, taskName))
			return;
		auto window = winrt::make<winrt::OpenNet::UI::Xaml::View::Windows::
			implementation::TrackerLogWindow>(
				winrt::to_hstring(taskId), taskName, trackerUrl);
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
		window.Activate();
	}

	void TaskTrackersPage::TrackerLogButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (auto button = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::Button>())
		{
			if (auto item = button.Tag().try_as<
				winrt::OpenNet::ViewModels::TrackerDisplayItem>())
			{
				TrackersListView().SelectedItem(item);
				OpenTrackerLog(item.URL());
			}
		}
	}

	void TaskTrackersPage::ViewLog_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		OpenTrackerLog(SelectedTrackerUrl());
	}

	winrt::Windows::Foundation::IAsyncAction TaskTrackersPage::EditTrackers_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		std::string taskId;
		winrt::hstring taskName;
		if (!TryGetTaskContext(taskId, taskName))
			co_return;
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		auto const trackers = core->GetTorrentTrackers(taskId);
		winrt::hstring existingText;
		for (auto const& tracker : trackers)
		{
			if (!existingText.empty()) existingText = existingText + L"\r\n";
			existingText = existingText + winrt::to_hstring(tracker.url);
		}
		winrt::OpenNet::UI::Xaml::View::Dialog::TextInputDialog dialog;
		dialog.Configure(ResourceGetString(L"ViewTaskTrackersPageEditTrackersDialog.Title"), {}, existingText, ResourceGetString(L"ViewTaskTrackersPageTrackerEditorTextBox.PlaceholderText"), ResourceGetString(L"ViewTaskTrackersPageEditTrackersDialog.PrimaryButtonText"), ResourceGetString(L"ViewTaskTrackersPageEditTrackersDialog.CloseButtonText"), true);
		dialog.XamlRoot(XamlRoot());
		if (co_await dialog.ShowAsync()
			!= winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
			co_return;

		std::vector<std::string> requested;
		std::unordered_set<std::string> unique;
		std::istringstream lines(winrt::to_string(dialog.InputText()));
		std::string line;
		winrt::hstring invalid;
		while (std::getline(lines, line))
		{
			line = Trim(std::move(line));
			if (line.empty()) continue;
			try
			{
				winrt::Windows::Foundation::Uri uri{ winrt::to_hstring(line) };
				auto const scheme = uri.SchemeName();
				if (scheme != L"http" && scheme != L"https" && scheme != L"udp")
					invalid = winrt::to_hstring(line);
			}
			catch (...)
			{
				invalid = winrt::to_hstring(line);
			}
			if (!invalid.empty()) break;
			if (unique.insert(line).second)
				requested.push_back(line);
		}

		if (!invalid.empty())
		{
			winrt::OpenNet::UI::Xaml::View::Dialog::ConfirmationDialog error;
			error.XamlRoot(XamlRoot());
			error.Configure(ResourceGetString(L"ViewTaskTrackersPageInvalidTrackerUrlTitle"), {}, invalid, {}, ResourceGetString(L"ViewTaskTrackersPageMessageDialog.CloseButtonText"), false, false, {});
			co_await error.ShowAsync();
			co_return;
		}

		std::unordered_set<std::string> current;
		for (auto const& tracker : trackers) current.insert(tracker.url);
		std::vector<std::string> removed;
		for (auto const& value : current)
			if (!unique.contains(value)) removed.push_back(value);
		std::vector<std::string> added;
		for (auto const& value : requested)
			if (!current.contains(value)) added.push_back(value);
		if (!removed.empty())
		{
			core->RemoveTrackers(taskId, removed);
			for (auto const& value : removed)
				core->ClearTrackerLog(taskId, value);
		}
		if (!added.empty()) core->AddTrackers(taskId, added);
		RefreshTrackerList();
	}

	void TaskTrackersPage::UpdateTracker_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (!m_viewModel || !m_viewModel.SelectedTask()) return;
		auto const state = m_viewModel.SelectedTask().State();
		if (state != winrt::OpenNet::ViewModels::DownloadTaskState::Downloading
			&& state != winrt::OpenNet::ViewModels::DownloadTaskState::Seeding)
			return;
		std::string taskId;
		winrt::hstring taskName;
		auto const url = SelectedTrackerUrl();
		if (url.empty() || !TryGetTaskContext(taskId, taskName)) return;
		::OpenNet::Core::P2PManager::Instance().TorrentCore()->
			ForceReannounceTracker(taskId, winrt::to_string(url));
	}

	winrt::Windows::Foundation::IAsyncAction TaskTrackersPage::RemoveTracker_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		std::string taskId;
		winrt::hstring taskName;
		auto const url = SelectedTrackerUrl();
		if (url.empty() || !TryGetTaskContext(taskId, taskName)) co_return;
		winrt::OpenNet::UI::Xaml::View::Dialog::ConfirmationDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Configure(ResourceGetString(L"ViewTaskTrackersPageRemoveTrackerDialog.Title"), {}, url, ResourceGetString(L"ViewTaskTrackersPageRemoveTrackerDialog.PrimaryButtonText"), ResourceGetString(L"ViewTaskTrackersPageRemoveTrackerDialog.CloseButtonText"), true, false, {});
		if (co_await dialog.ShowAsync()
			!= winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
			co_return;
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		core->RemoveTrackers(taskId, { winrt::to_string(url) });
		core->ClearTrackerLog(taskId, winrt::to_string(url));
		RefreshTrackerList();
	}

	void TaskTrackersPage::ClearTrackerLog_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		std::string taskId;
		winrt::hstring taskName;
		auto const url = SelectedTrackerUrl();
		if (url.empty() || !TryGetTaskContext(taskId, taskName)) return;
		::OpenNet::Core::P2PManager::Instance().TorrentCore()->
			ClearTrackerLog(taskId, winrt::to_string(url));
		RefreshTrackerList();
	}

	void TaskTrackersPage::ClearTaskTrackerLogs_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		std::string taskId;
		winrt::hstring taskName;
		if (!TryGetTaskContext(taskId, taskName)) return;
		::OpenNet::Core::P2PManager::Instance().TorrentCore()->
			ClearTrackerLogs(taskId);
		RefreshTrackerList();
	}

	winrt::Windows::Foundation::IAsyncAction TaskTrackersPage::ClearAllTrackerLogs_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		std::string taskId;
		winrt::hstring taskName;
		if (!TryGetTaskContext(taskId, taskName)) co_return;
		winrt::OpenNet::UI::Xaml::View::Dialog::ConfirmationDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Configure(ResourceGetString(L"ViewTaskTrackersPageClearAllTrackerLogsDialog.Title"), ResourceGetString(L"ViewTaskTrackersPageClearAllTrackerLogsMessage.Text"), {}, ResourceGetString(L"ViewTaskTrackersPageClearAllTrackerLogsDialog.PrimaryButtonText"), ResourceGetString(L"ViewTaskTrackersPageClearAllTrackerLogsDialog.CloseButtonText"), true, false, {});
		if (co_await dialog.ShowAsync()
			== winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
		{
			::OpenNet::Core::P2PManager::Instance().TorrentCore()->
				ClearAllTrackerLogs();
			RefreshTrackerList();
		}
	}

	winrt::Windows::Foundation::IAsyncAction TaskTrackersPage::RemoveUnreachableTrackers_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		std::string taskId;
		winrt::hstring taskName;
		if (!TryGetTaskContext(taskId, taskName)) co_return;
		winrt::OpenNet::UI::Xaml::View::Dialog::NumberInputDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Configure(ResourceGetString(L"ViewTaskTrackersPageRemoveUnreachableTrackersDialog.Title"), ResourceGetString(L"ViewTaskTrackersPageRemoveUnreachablePromptText.Text"), 3, 1, 255, ResourceGetString(L"ViewTaskTrackersPageRemoveUnreachableTrackersDialog.PrimaryButtonText"), ResourceGetString(L"ViewTaskTrackersPageRemoveUnreachableTrackersDialog.CloseButtonText"));
		if (co_await dialog.ShowAsync()
			!= winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
			co_return;
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		auto const trackers = core->GetTorrentTrackers(taskId);
		auto const rawThreshold = dialog.Value();
		if (std::isnan(rawThreshold)) co_return;
		auto const threshold = std::clamp(
			static_cast<int>(std::round(rawThreshold)), 1, 255);
		std::vector<std::string> removed;
		for (auto const& tracker : trackers)
			if (tracker.retries >= threshold) removed.push_back(tracker.url);
		if (!removed.empty())
		{
			core->RemoveTrackers(taskId, removed);
			for (auto const& tracker : removed)
				core->ClearTrackerLog(taskId, tracker);
			RefreshTrackerList();
		}
	}

	void TaskTrackersPage::RefreshTrackerList()
	{
		auto listView = TrackersListView();
		auto emptyPanel = EmptyStatePanel();
		if (!listView) return;

		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			m_trackerItems.Clear();
			if (emptyPanel) emptyPanel.Visibility(Visibility::Visible);
			return;
		}

		auto selectedTask = m_viewModel.SelectedTask();
		auto taskType = selectedTask.TaskType();

		if (taskType != winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
		{
			m_trackerItems.Clear();
			if (emptyPanel) emptyPanel.Visibility(Visibility::Visible);
			return;
		}

		auto taskId = winrt::to_string(selectedTask.TaskId());
		if (taskId.empty())
		{
			m_trackerItems.Clear();
			if (emptyPanel) emptyPanel.Visibility(Visibility::Visible);
			return;
		}
		if (taskId != m_lastTaskId)
		{
			m_lastTaskId = taskId;
			m_hasTrackerSnapshot = false;
		}
		auto const nowSteady = std::chrono::steady_clock::now();
		if (m_hasTrackerSnapshot && m_lastTrackerSnapshotHash == 0
			&& nowSteady - m_lastTrackerRefresh < std::chrono::seconds(5))
		{
			return;
		}

		auto& p2p = ::OpenNet::Core::P2PManager::Instance();
		if (!p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore())
		{
			m_trackerItems.Clear();
			if (emptyPanel) emptyPanel.Visibility(Visibility::Visible);
			return;
		}

		auto trackers = p2p.TorrentCore()->GetTorrentTrackers(taskId);
		// Some imported torrents and legacy custom-tracker settings may contain
		// an entire newline-delimited tracker list in one libtorrent URL field.
		// Rendering that value directly makes a single DataRow hundreds of pixels
		// tall. Flatten it to actual tracker rows before sorting or virtualization.
		decltype(trackers) normalizedTrackers;
		for (auto const& tracker : trackers)
		{
			std::istringstream lines{ tracker.url };
			std::string url;
			while (std::getline(lines, url))
			{
				url = Trim(std::move(url));
				if (url.empty()) continue;
				auto normalized = tracker;
				normalized.url = std::move(url);
				normalizedTrackers.push_back(std::move(normalized));
			}
		}
		trackers = std::move(normalizedTrackers);
		// libtorrent may expose the same announce URL more than once for
		// different internal endpoint/hash states. The table represents a tracker
		// URL, so fold exact duplicates before updating the realized rows.
		std::unordered_set<std::string> trackerUrls;
		std::erase_if(trackers, [&trackerUrls](auto const& tracker)
		{
			return !trackerUrls.insert(tracker.url).second;
		});

		if (trackers.empty())
		{
			if (!m_hasTrackerSnapshot || m_lastTrackerSnapshotHash != 0)
				m_trackerItems.Clear();
			m_hasTrackerSnapshot = true;
			m_lastTrackerSnapshotHash = 0;
			m_lastTrackerRefresh = nowSteady;
			if (emptyPanel) emptyPanel.Visibility(Visibility::Visible);
			return;
		}

		if (m_sortDirection != 0)
		{
			auto const direction = m_sortDirection;
			auto const column = m_sortColumn;
			std::stable_sort(trackers.begin(), trackers.end(),
							 [direction, column](auto const& left, auto const& right)
			{
				auto ascending = [&]()
				{
					if (column == L"URL") return left.url < right.url;
					if (column == L"Retries") return left.retries < right.retries;
					if (column == L"TimeRemaining") return left.nextAnnounceSeconds < right.nextAnnounceSeconds;
					if (column == L"Seeders") return left.seeders < right.seeders;
					if (column == L"Leechers") return left.leechers < right.leechers;
					if (column == L"Downloaded") return left.downloaded < right.downloaded;
					return left.status < right.status;
				};
				auto descending = [&]()
				{
					if (column == L"URL") return right.url < left.url;
					if (column == L"Retries") return right.retries < left.retries;
					if (column == L"TimeRemaining") return right.nextAnnounceSeconds < left.nextAnnounceSeconds;
					if (column == L"Seeders") return right.seeders < left.seeders;
					if (column == L"Leechers") return right.leechers < left.leechers;
					if (column == L"Downloaded") return right.downloaded < left.downloaded;
					return right.status < left.status;
				};
				return direction == 1 ? ascending() : descending();
			});
		}

		std::size_t snapshotHash = trackers.size();
		auto combineHash = [&snapshotHash](auto const& value)
		{
			snapshotHash ^= std::hash<std::decay_t<decltype(value)>>{}(value)
				+0x9e3779b9u + (snapshotHash << 6) + (snapshotHash >> 2);
		};
		for (auto const& tracker : trackers)
		{
			combineHash(tracker.url);
			combineHash(tracker.tier);
			combineHash(tracker.numPeers);
			combineHash(tracker.retries);
			combineHash(tracker.nextAnnounceSeconds);
			combineHash(tracker.seeders);
			combineHash(tracker.leechers);
			combineHash(tracker.downloaded);
			combineHash(tracker.status);
			combineHash(tracker.message);
			if (auto const latest = p2p.TorrentCore()->GetLatestTrackerLog(
				taskId, tracker.url))
			{
				combineHash(latest->timestamp);
				combineHash(latest->content);
			}
		}
		if (m_hasTrackerSnapshot && snapshotHash == m_lastTrackerSnapshotHash)
		{
			m_lastTrackerRefresh = nowSteady;
			return;
		}
		m_hasTrackerSnapshot = true;
		m_lastTrackerSnapshotHash = snapshotHash;
		m_lastTrackerRefresh = nowSteady;

		for (std::uint32_t index = 0;
			 index < static_cast<std::uint32_t>(trackers.size());
			 ++index)
		{
			auto const& tracker = trackers[index];
			auto const trackerUrl = winrt::to_hstring(tracker.url);
			winrt::OpenNet::ViewModels::TrackerDisplayItem item{ nullptr };
			if (index < m_trackerItems.Size())
			{
				item = m_trackerItems.GetAt(index)
					.try_as<winrt::OpenNet::ViewModels::TrackerDisplayItem>();
			}
			if (!item)
			{
				item = winrt::make<
					winrt::OpenNet::ViewModels::implementation::TrackerDisplayItem>();
				if (index < m_trackerItems.Size())
					m_trackerItems.SetAt(index, item);
				else
					m_trackerItems.Append(item);
			}
			item.URL(trackerUrl);
			item.Tier(winrt::to_hstring(tracker.tier));
			item.Peers(winrt::to_hstring(tracker.numPeers));
			item.Retries(winrt::to_hstring(tracker.retries));
			item.TimeRemaining(FormatRemaining(tracker.nextAnnounceSeconds));
			item.Seeders(FormatOptionalNumber(tracker.seeders));
			item.Leechers(FormatOptionalNumber(tracker.leechers));
			item.Downloaded(FormatOptionalNumber(tracker.downloaded));
			auto const latest = p2p.TorrentCore()->GetLatestTrackerLog(
				taskId, tracker.url);
			if (latest)
				item.Status(FormatLogStatus(
					latest->timestamp, latest->content));
			else if (!tracker.message.empty())
				item.Status(winrt::to_hstring(tracker.message));
			else
				item.Status(winrt::to_hstring(tracker.status));
			item.Message(winrt::to_hstring(tracker.message));
		}
		while (m_trackerItems.Size() > trackers.size())
			m_trackerItems.RemoveAtEnd();

		if (emptyPanel) emptyPanel.Visibility(Visibility::Collapsed);
	}
}
