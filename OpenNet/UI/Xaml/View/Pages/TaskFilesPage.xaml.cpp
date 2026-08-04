#include "XamlWorkaround.h"
#include "TaskFilesPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskFilesPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskFilesPage.g.cpp"
#endif

#include "ViewModels/DisplayItems.h"

import Core.Utils.Misc;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.P2PManager;
import OpenNet.Helpers.ColumnWidthHelper;
import OpenNet.UI.Xaml.Control.DataTableSortHelper;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Windows.Foundation.Collections;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	// Map libtorrent priority (0-7) to ComboBox index (0=skip, 1=low, 2=normal, 3=high)
	static int PriorityToComboIndex(int priority)
	{
		if (priority == 0) return 0;       // skip
		if (priority <= 2) return 1;       // low (1-2)
		if (priority <= 5) return 2;       // normal (3-5)
		return 3;                          // high (6-7)
	}

	// Map ComboBox index back to libtorrent priority value
	static int ComboIndexToPriority(int index)
	{
		switch (index)
		{
			case 0: return 0;  // skip
			case 1: return 1;  // low
			case 2: return 4;  // normal
			case 3: return 7;  // high
			default: return 4;
		}
	}

	TaskFilesPage::TaskFilesPage()
	{
		m_fileItems = winrt::single_threaded_observable_vector<
			winrt::Windows::Foundation::IInspectable>();
	}

	void TaskFilesPage::InitializeComponent()
	{
		TaskFilesPageT::InitializeComponent();
		FilesListView().ItemsSource(m_fileItems);
		UpdateSortHeaders();
		Loaded([this](auto, auto)
		{
			using namespace ::OpenNet::Helpers;
			RestoreColumn(ColFileSize(), "Files.Size");
			RestoreColumn(ColFileProgress(), "Files.Progress");
			RestoreColumn(ColFileDone(), "Files.Done");
			RestoreColumn(ColFilePriority(), "Files.Priority");
		});
		Unloaded([this](auto, auto)
		{
			using namespace ::OpenNet::Helpers;
			SaveColumnWidth("Files.Size", ColFileSize());
			SaveColumnWidth("Files.Progress", ColFileProgress());
			SaveColumnWidth("Files.Done", ColFileDone());
			SaveColumnWidth("Files.Priority", ColFilePriority());
		});
	}

	TaskFilesPage::~TaskFilesPage()
	{
		Unsubscribe();
		if (m_refreshTimer)
		{
			m_refreshTimer.Stop();
			m_refreshTimer.Tick(m_timerTickToken);
			m_refreshTimer = nullptr;
		}
	}

	void TaskFilesPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		Unsubscribe();

		m_viewModel = e.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel)
			m_viewModel = this->DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();

		if (m_viewModel)
		{
			this->DataContext(m_viewModel);
			m_vmPropertyChangedToken = m_viewModel.PropertyChanged(
				{ this, &TaskFilesPage::OnViewModelPropertyChanged });
		}

		if (!m_refreshTimer)
		{
			m_refreshTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
			m_timerTickToken = m_refreshTimer.Tick(
				{ this, &TaskFilesPage::OnRefreshTimerTick });
		}
		auto& database = ::OpenNet::Core::AppSettingsDatabase::Instance();
		database.Initialize();
		m_refreshTimer.Interval(std::chrono::milliseconds(
			std::clamp<std::int64_t>(
				database.GetInt("ui", "refresh_interval_ms").value_or(1000),
				100,
				60000)));
		m_refreshTimer.Start();

		RefreshFileList();
	}

	void TaskFilesPage::OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		if (m_refreshTimer)
		{
			m_refreshTimer.Stop();
		}
		Unsubscribe();
	}

	void TaskFilesPage::Unsubscribe()
	{
		if (m_viewModel && m_vmPropertyChangedToken.value)
		{
			m_viewModel.PropertyChanged(m_vmPropertyChangedToken);
			m_vmPropertyChangedToken = {};
		}
		m_viewModel = nullptr;
	}

	void TaskFilesPage::OnViewModelPropertyChanged(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() == L"SelectedTask")
		{
			RefreshFileList();
		}
	}

	void TaskFilesPage::OnRefreshTimerTick(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Windows::Foundation::IInspectable const&)
	{
		RefreshFileList();
	}

	void TaskFilesPage::ColumnHeader_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto button = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::Button>();
		if (!button || !button.Tag())
			return;
		auto const column = winrt::unbox_value<winrt::hstring>(button.Tag());
		if (m_sortColumn == column)
			m_sortDirection = (m_sortDirection + 1) % 3;
		else
		{
			m_sortColumn = column;
			m_sortDirection = 1;
		}
		UpdateSortHeaders();
		RefreshFileList();
	}

	void TaskFilesPage::UpdateSortHeaders()
	{
		auto update = [this](auto const& button)
		{
			::OpenNet::UI::Xaml::Control::DataTableSortHelper::UpdateHeader(
				button, m_sortColumn, m_sortDirection);
		};
		update(SortFileNameButton());
		update(SortFileSizeButton());
		update(SortFileProgressButton());
		update(SortFileDoneButton());
		update(SortFilePriorityButton());
	}

	void TaskFilesPage::ColumnHeader_RightTapped(
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

	void TaskFilesPage::ColumnMenu_Opening(
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

	winrt::XamlToolkit::Labs::WinUI::DataColumn TaskFilesPage::ColumnForTag(
		winrt::hstring const& tag)
	{
		if (tag == L"Path") return ColFileName();
		if (tag == L"Size") return ColFileSize();
		if (tag == L"Progress") return ColFileProgress();
		if (tag == L"Done") return ColFileDone();
		if (tag == L"Priority") return ColFilePriority();
		return nullptr;
	}

	void TaskFilesPage::ColumnVisibility_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto toggle = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>();
		if (!toggle || !toggle.Tag()) return;
		if (auto column = ColumnForTag(winrt::unbox_value<winrt::hstring>(toggle.Tag())))
			column.Visibility(toggle.IsChecked() ? Visibility::Visible : Visibility::Collapsed);
	}

	void TaskFilesPage::AutoSizeColumn(
		winrt::XamlToolkit::Labs::WinUI::DataColumn const& column)
	{
		if (!column) return;
		column.DesiredWidth(GridLengthHelper::Auto());
		column.InvalidateMeasure();
		FilesListView().InvalidateMeasure();
	}

	void TaskFilesPage::AutoSizeSelectedColumn_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		AutoSizeColumn(m_contextColumn);
	}

	void TaskFilesPage::AutoSizeAllColumns_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		for (auto const& column : std::array{
			ColFileName(), ColFileSize(), ColFileProgress(), ColFileDone(), ColFilePriority() })
			AutoSizeColumn(column);
	}

	void TaskFilesPage::ResetColumns_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
	{
		m_sortColumn = {};
		m_sortDirection = 0;
		UpdateSortHeaders();
		for (auto const& column : std::array{
			ColFileName(), ColFileSize(), ColFileProgress(), ColFileDone(), ColFilePriority() })
			column.Visibility(Visibility::Visible);
		FileProgressColumnToggle().IsChecked(true);
		FilePriorityColumnToggle().IsChecked(true);
		AutoSizeAllColumns_Click(sender, args);
		RefreshFileList();
	}

	void TaskFilesPage::RefreshFileList()
	{
		auto listView = FilesListView();
		auto emptyText = EmptyStateText();
		if (!listView) return;

		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			m_fileItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto selectedTask = m_viewModel.SelectedTask();
		auto taskType = selectedTask.TaskType();

		if (taskType != winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
		{
			m_fileItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto taskId = winrt::to_string(selectedTask.TaskId());
		if (taskId.empty())
		{
			m_fileItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto& p2p = ::OpenNet::Core::P2PManager::Instance();
		if (!p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore())
		{
			m_fileItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		auto detail = p2p.TorrentCore()->GetTorrentDetail(taskId);

		if (detail.files.empty())
		{
			m_fileItems.Clear();
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		m_isRefreshing = true; // Suppress ComboBox events during list rebuild

		if (m_sortDirection != 0)
		{
			auto const direction = m_sortDirection;
			auto const column = m_sortColumn;
			std::stable_sort(detail.files.begin(), detail.files.end(),
							 [direction, column](auto const& left, auto const& right)
			{
				bool less = false;
				if (column == L"Path") less = left.path < right.path;
				else if (column == L"Size") less = left.size < right.size;
				else if (column == L"Progress")
				{
					auto const lp = left.size > 0
						? static_cast<double>(left.bytesCompleted) / left.size : 0.0;
					auto const rp = right.size > 0
						? static_cast<double>(right.bytesCompleted) / right.size : 0.0;
					less = lp < rp;
				}
				else if (column == L"Done") less = left.bytesCompleted < right.bytesCompleted;
				else if (column == L"Priority") less = left.priority < right.priority;
				return direction == 1 ? less :
					(column == L"Path" ? right.path < left.path :
					 column == L"Size" ? right.size < left.size :
					 column == L"Progress"
					 ? (right.size > 0 ? static_cast<double>(right.bytesCompleted) / right.size : 0.0) <
					 (left.size > 0 ? static_cast<double>(left.bytesCompleted) / left.size : 0.0)
					 : column == L"Done" ? right.bytesCompleted < left.bytesCompleted
					 : right.priority < left.priority);
			});
		}

		for (std::uint32_t index = 0;
			 index < static_cast<std::uint32_t>(detail.files.size());
			 ++index)
		{
			auto const& file = detail.files[index];
			winrt::OpenNet::ViewModels::FileDisplayItem item{ nullptr };
			std::uint32_t existingIndex = index;
			while (existingIndex < m_fileItems.Size())
			{
				auto candidate = m_fileItems.GetAt(existingIndex)
					.try_as<winrt::OpenNet::ViewModels::FileDisplayItem>();
				if (candidate && candidate.FileIndex() == file.fileIndex)
				{
					item = candidate;
					break;
				}
				++existingIndex;
			}

			if (item && existingIndex != index)
			{
				m_fileItems.RemoveAt(existingIndex);
				m_fileItems.InsertAt(index, item);
			}
			else if (!item)
			{
				item = winrt::make<
					winrt::OpenNet::ViewModels::implementation::FileDisplayItem>();
				m_fileItems.InsertAt(index, item);
			}
			item.Path(winrt::to_hstring(file.path));
			item.Size(::Core::Utils::Misc::friendlyUnit(file.size));

			double progressPct = (file.size > 0)
				? (static_cast<double>(file.bytesCompleted) / file.size * 100.0)
				: 0.0;
			item.ProgressValue(progressPct);
			item.Done(::Core::Utils::Misc::friendlyUnit(file.bytesCompleted));
			item.PriorityIndex(PriorityToComboIndex(file.priority));
			item.FileIndex(file.fileIndex);
		}
		while (m_fileItems.Size() > detail.files.size())
			m_fileItems.RemoveAtEnd();

		if (emptyText) emptyText.Visibility(Visibility::Collapsed);

		m_isRefreshing = false;
	}

	void TaskFilesPage::FilePriority_SelectionChanged(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*args*/)
	{
		if (m_isRefreshing) return; // Ignore events during list refresh

		auto comboBox = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ComboBox>();
		if (!comboBox) return;

		auto selectedIndex = comboBox.SelectedIndex();
		if (selectedIndex < 0) return;

		// Read the file index from the ComboBox Tag
		auto tagObj = comboBox.Tag();
		if (!tagObj) return;
		int fileIndex = winrt::unbox_value<int>(tagObj);

		if (!m_viewModel || !m_viewModel.SelectedTask()) return;

		auto taskId = winrt::to_string(m_viewModel.SelectedTask().TaskId());
		if (taskId.empty()) return;

		auto& p2p = ::OpenNet::Core::P2PManager::Instance();
		if (!p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore()) return;

		// Get current file list to build accurate priority vector
		auto detail = p2p.TorrentCore()->GetTorrentDetail(taskId);
		if (detail.files.empty()) return;

		std::vector<int> priorities;
		priorities.reserve(detail.files.size());
		for (auto const& f : detail.files)
		{
			priorities.push_back(f.priority);
		}

		// Update the changed file's priority
		if (fileIndex >= 0 && fileIndex < static_cast<int>(priorities.size()))
		{
			priorities[fileIndex] = ComboIndexToPriority(selectedIndex);
			p2p.TorrentCore()->SetFilePriorities(taskId, priorities);
		}
	}
}
