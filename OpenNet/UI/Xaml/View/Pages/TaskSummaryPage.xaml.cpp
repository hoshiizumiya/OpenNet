#include "pch.h"
#include "TaskSummaryPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskSummaryPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskSummaryPage.g.cpp"
#endif
#include "Core/DataGraph/SpeedGraphDatabase.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TaskSummaryPage::TaskSummaryPage()
	{
		InitializeComponent();
		// Keep the page in cache so the SpeedGraph data persists across
		// SelectorBar tab switches (TaskSummaryPage ↔ PeersListPage etc.)
		this->NavigationCacheMode(winrt::Microsoft::UI::Xaml::Navigation::NavigationCacheMode::Required);
	}

	TaskSummaryPage::~TaskSummaryPage()
	{
		UnsubscribeTask();
		Unsubscribe();
	}

	void TaskSummaryPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		UnsubscribeTask();
		Unsubscribe();

		m_viewModel = e.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel)
		{
			m_viewModel = this->DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		}

		if (m_viewModel)
		{
			this->DataContext(m_viewModel);
			m_vmPropertyChangedToken = m_viewModel.PropertyChanged({ this, &TaskSummaryPage::OnViewModelPropertyChanged });

			// Subscribe to the current selected task's property changes
			auto selectedTask = m_viewModel.SelectedTask();
			if (selectedTask)
			{
				SubscribeToTask(selectedTask);
			}
		}

		UpdateSpeedGraphForSelectedTask();
	}

	void TaskSummaryPage::OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		UnsubscribeTask();
		Unsubscribe();
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

	void TaskSummaryPage::UnsubscribeTask()
	{
		if (m_currentSubscribedTask && m_taskPropertyChangedToken.value)
		{
			m_currentSubscribedTask.PropertyChanged(m_taskPropertyChangedToken);
			m_taskPropertyChangedToken = {};
		}
		m_currentSubscribedTask = nullptr;
	}

	void TaskSummaryPage::SubscribeToTask(winrt::OpenNet::ViewModels::TaskViewModel const& task)
	{
		UnsubscribeTask();
		if (!task) return;
		m_currentSubscribedTask = task;
		m_taskPropertyChangedToken = task.PropertyChanged({ this, &TaskSummaryPage::OnTaskPropertyChanged });
	}

	void TaskSummaryPage::OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const&,
													 winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() == L"SelectedTask")
		{
			auto selectedTask = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
			SubscribeToTask(selectedTask);

			// Reset graph for newly-selected task
			auto speedGraph = TaskSpeedGraph();
			if (speedGraph)
			{
				speedGraph.Reset();

				// Load historical speed data from SQLite and replay into graph
				if (selectedTask)
				{
					auto taskId = winrt::to_string(selectedTask.TaskId());
					if (!taskId.empty())
					{
						auto points = ::OpenNet::Core::SpeedGraphDatabase::Instance().LoadPoints(taskId);
						for (auto const& pt : points)
						{
							speedGraph.SetSpeed(static_cast<double>(pt.percent), pt.speedKB * 1024);
						}
					}
				}
			}
			UpdateSpeedGraphForSelectedTask();
		}
	}

	void TaskSummaryPage::OnTaskPropertyChanged(winrt::Windows::Foundation::IInspectable const&,
												winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		auto propName = args.PropertyName();
		if (propName == L"ProgressPercent" || propName == L"DownloadSpeedKB")
		{
			UpdateSpeedGraphForSelectedTask();
		}
	}

	void TaskSummaryPage::TaskSpeedGraph_SizeChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& /*e*/)
	{
		// Update the speed graph when size changes
		UpdateSpeedGraphForSelectedTask();
	}

	void TaskSummaryPage::UpdateSpeedGraphForSelectedTask()
	{
		if (!m_viewModel) return;

		auto selectedTask = m_viewModel.SelectedTask();
		if (!selectedTask) return;

		// Get the SpeedGraph control
		auto speedGraph = TaskSpeedGraph();
		if (!speedGraph) return;

		// Update the speed graph with the selected task's current speed data
		auto percent = selectedTask.ProgressPercent();
		auto speedKB = selectedTask.DownloadSpeedKB();
		speedGraph.SetSpeed(percent, speedKB * 1024);  // Convert KB to bytes
	}

}
