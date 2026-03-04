#include "pch.h"
#include "TaskSummaryPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TaskSummaryPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskSummaryPage.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TaskSummaryPage::TaskSummaryPage()
	{
		InitializeComponent();
	}

	TaskSummaryPage::~TaskSummaryPage()
	{
		Unsubscribe();
	}

	void TaskSummaryPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		Unsubscribe();

		m_viewModel = e.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel)
		{
			// 兜底：如果没传参数，则尝试用页面 DataContext，需要检查页面初次进入初始化情况
			m_viewModel = this->DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		}

		if (m_viewModel)
		{
			this->DataContext(m_viewModel);
			m_vmPropertyChangedToken = m_viewModel.PropertyChanged({ this, &TaskSummaryPage::OnViewModelPropertyChanged });
		}

		UpdateSpeedGraphForSelectedTask();
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

	void TaskSummaryPage::OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const&,
													 winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() == L"SelectedTask")
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
