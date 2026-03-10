#include "pch.h"
#include "TaskFilesPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskFilesPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskFilesPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "Core/Utils/Misc.h"
#include "Core/P2PManager.h"
#include "ViewModels/DisplayItems.h"

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
		InitializeComponent();
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
			m_refreshTimer.Interval(std::chrono::seconds(5)); // Files change less often
			m_timerTickToken = m_refreshTimer.Tick(
				{ this, &TaskFilesPage::OnRefreshTimerTick });
		}
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

	void TaskFilesPage::RefreshFileList()
	{
		auto listView = FilesListView();
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

		if (detail.files.empty())
		{
			listView.ItemsSource(nullptr);
			if (emptyText) emptyText.Visibility(Visibility::Visible);
			return;
		}

		m_isRefreshing = true; // Suppress ComboBox events during list rebuild

		auto items = winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();

		for (auto const& file : detail.files)
		{
			auto item = winrt::make<winrt::OpenNet::ViewModels::implementation::FileDisplayItem>();
			item.Path(winrt::to_hstring(file.path));
			item.Size(::Core::Utils::Misc::friendlyUnit(file.size));

			double progressPct = (file.size > 0)
				? (static_cast<double>(file.bytesCompleted) / file.size * 100.0)
				: 0.0;
			item.ProgressValue(progressPct);
			item.Done(::Core::Utils::Misc::friendlyUnit(file.bytesCompleted));
			item.PriorityIndex(PriorityToComboIndex(file.priority));
			item.FileIndex(file.fileIndex);
			items.Append(item);
		}

		listView.ItemsSource(items);
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
