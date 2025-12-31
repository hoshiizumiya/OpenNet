#include "pch.h"
#include "TasksPage.xaml.h"
#if __has_include("Pages/TasksPage.g.cpp")
#include "Pages/TasksPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.Resources.h>
#include <winrt/Windows.UI.Xaml.Navigation.h>

#include <shobjidl.h> // For IInitializeWithWindow
#include "Core/P2PManager.h"
#include "Controls/SpeedGraph/SpeedGraph.xaml.h"
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::Pages::implementation
{
	TasksPage::TasksPage()
	{
		InitializeComponent();

		// 保持页面缓存，避免切换页面时被销毁重建导致 ViewModel 丢失
		this->NavigationCacheMode(winrt::Microsoft::UI::Xaml::Navigation::NavigationCacheMode::Enabled);

		// Create and attach the view-model
		m_viewModel = winrt::make<winrt::OpenNet::ViewModels::implementation::TasksViewModel>();
		// we do not need the data context, we use x:Bind
		// this->DataContext(m_viewModel);
		// subscribe to AddTaskRequested to show modal dialog
		m_addTaskToken = m_viewModel.AddTaskRequested({ this,&TasksPage::OnAddTaskRequested });
		// 让下方面板默认显示 Summary，并把 VM 传给子页
		if (auto frame = ContentFrame())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskSummaryPage>(), m_viewModel);
		}
	}

	TasksPage::~TasksPage()
	{
		UnsubscribeFromSelectedTaskChanges();
		if (m_viewModel && m_addTaskToken.value)
		{
			m_viewModel.AddTaskRequested(m_addTaskToken);
		}
	}

	void TasksPage::OnAddTaskRequested(IInspectable const&, winrt::hstring const&)
	{
		make<winrt::OpenNet::UI::Xaml::View::Windows::implementation::TorrentCheckModalWindow>().Activate();
	}

	void TasksPage::FilterNavView_SelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const& sender,
												   Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args)
	{
		(void)sender;
		auto item = args.SelectedItem().try_as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
		if (!item) return;
		auto tag = unbox_value_or<winrt::hstring>(item.Tag(), L"");
		if (tag.empty()) return;
		if (m_viewModel)
		{
			m_viewModel.ApplyFilter(tag);
		}
	}

	void TasksPage::TasksList_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender,
											   winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args)
	{
		(void)sender;
		(void)args;

		auto listView = TasksList();
		if (!listView || !m_viewModel) return;

		auto selectedItem = listView.SelectedItem();
		auto taskVm = selectedItem.try_as<winrt::OpenNet::ViewModels::TaskViewModel>();

		m_viewModel.SelectedTask(taskVm);
	}

	void TasksPage::SubscribeToSelectedTaskChanges(winrt::OpenNet::ViewModels::TaskViewModel const& task)
	{
		if (!task) return;

		m_currentSubscribedTask = task;
		m_selectedTaskPropertyChangedToken = task.PropertyChanged({ this, &TasksPage::OnSelectedTaskPropertyChanged });
	}

	void TasksPage::UnsubscribeFromSelectedTaskChanges()
	{
		if (m_currentSubscribedTask && m_selectedTaskPropertyChangedToken.value)
		{
			m_currentSubscribedTask.PropertyChanged(m_selectedTaskPropertyChangedToken);
			m_selectedTaskPropertyChangedToken = {};
		}
		m_currentSubscribedTask = nullptr;
	}

	void TasksPage::OnSelectedTaskPropertyChanged(winrt::Windows::Foundation::IInspectable const& sender,
												  winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		(void)sender;

		auto propertyName = args.PropertyName();
		// Update speed graph when progress or speed changes
		if (propertyName == L"ProgressPercent" || propertyName == L"DownloadSpeedKB")
		{
			// need investigate how to update SpeedGraph control
			//UpdateSpeedGraphForSelectedTask();
		}
	}
}
