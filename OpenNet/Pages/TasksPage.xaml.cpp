#include "pch.h"
#include "TasksPage.xaml.h"
#if __has_include("Pages/TasksPage.g.cpp")
#include "Pages/TasksPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.Resources.h>
#include <winrt/Windows.UI.Xaml.Navigation.h>

#include <shobjidl.h> // For IInitializeWithWindow
#include "Core/P2PManager.h"
#include "Helpers/WindowHelper.h"
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
		// for runtime binding we may do not need the data context
		this->DataContext(m_viewModel);
		// subscribe to AddTaskRequested to show modal dialog
		m_addTaskToken = m_viewModel.AddTaskRequested({ this,&TasksPage::OnAddTaskRequested });
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

	Windows::Foundation::IAsyncOperation<hstring> TasksPage::PickFolderAsync()
	{
		if (auto appWindow = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetAppWindowForElement(*this))
		{
			// update to WASdk https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.windows.storage.pickers#remarks
			Microsoft::Windows::Storage::Pickers::FolderPicker folderPicker(appWindow.Id());
			// folderPicker.CommitButtonText(L"Pick a folder");
			folderPicker.SuggestedStartLocation(Microsoft::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
			folderPicker.ViewMode(Microsoft::Windows::Storage::Pickers::PickerViewMode::List);

			// Show the picker dialog window
			auto&& folderRef = co_await folderPicker.PickSingleFolderAsync();
			if (folderRef)
			{
				auto path{ folderRef.Path() };
#ifdef _DEBUG
				OutputDebugString(L"Pick path success");
#endif
				co_return path;
			}
		}
		else
		{
			// error handling logic

		}
		//folderPicker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::Desktop);
		//folderPicker.FileTypeFilter().Append(L"*");

		// 初始化窗口句柄 (IInitializeWithWindow)
		//if (auto init = picker.as<::IInitializeWithWindow>())
		//{
		//	init->Initialize(hwnd);
		//}

		co_return hstring{};
	}

	fire_and_forget TasksPage::ShowAddMagnetDialog()
	{
		auto lifetime = get_strong();

		// 使用 ResourceLoader 运行时读取本地化字符串（在 Resources.resw 中添加对应键和值）
		auto loader = winrt::Windows::ApplicationModel::Resources::ResourceLoader::GetForViewIndependentUse();

		ContentDialog dialog;
		dialog.XamlRoot(this->XamlRoot());

		// 对话框文本从资源加载（键名可以自由命名，但需在资源文件中一致）
		dialog.Title(box_value(loader.GetString(L"AddMagnetDialog_Title")));            // e.g. "添加磁力链接 / Add Magnet"
		dialog.PrimaryButtonText(loader.GetString(L"Ok"));                              // e.g. "确定"
		dialog.CloseButtonText(loader.GetString(L"Cancel"));                            // e.g. "取消"
		dialog.DefaultButton(ContentDialogButton::Primary);

		StackPanel panel;
		panel.Spacing(8);

		TextBlock magnetLabel;
		magnetLabel.Text(loader.GetString(L"AddMagnet_MagnetLabel")); // e.g. "磁力链接 / Magnet Link:"
		TextBox magnetBox;
		magnetBox.PlaceholderText(loader.GetString(L"AddMagnet_MagnetPlaceholder")); // e.g. "magnet:?xt=..."; magnetBox.MinWidth(480);
		TextBlock saveLabel;
		saveLabel.Text(loader.GetString(L"AddMagnet_SaveLabel")); // e.g. "保存路径 / Save Path:"

		StackPanel pathRow;
		pathRow.Orientation(Orientation::Horizontal); pathRow.Spacing(6);
		TextBox savePathBox;
		savePathBox.Width(380);
		savePathBox.PlaceholderText(loader.GetString(L"AddMagnet_SavePlaceholder")); // e.g. "例如: C:\\Downloads (可选)"
		Button browseBtn;
		browseBtn.Content(box_value(loader.GetString(L"AddMagnet_Browse"))); // e.g. "浏览..."

		// 保留异步打开文件夹的逻辑，使用异常捕获防止协程异常导致问题
		browseBtn.Click(
			[weakPage = get_weak(), savePathBox](IInspectable const&, RoutedEventArgs const&) -> winrt::fire_and_forget
			{
				// 避免协程内未捕获异常导致早期结束 + 框架试图重复调度
				try
				{
					if (auto page = weakPage.get())
					{
						// 可选：如果存在跨线程恢复失败情况，强制回到 UI 线程（WinUI 3 用 DispatcherQueue）
						// co_await winrt::resume_foreground(page->DispatcherQueue());

						auto folderPath = co_await page->PickFolderAsync();
						if (!folderPath.empty())
						{
							savePathBox.Text(folderPath);
						}
					}
				}
				catch (...)
				{
					// 记录日志或忽略，避免异常穿透导致协程框架不一致
					OutputDebugString(L"[TasksPage] browseBtn.Click coroutine threw.\n");
				}
			});

		pathRow.Children().Append(savePathBox);
		pathRow.Children().Append(browseBtn);

		panel.Children().Append(magnetLabel);
		panel.Children().Append(magnetBox);
		panel.Children().Append(saveLabel);
		panel.Children().Append(pathRow);

		dialog.Content(panel);

		auto result = co_await dialog.ShowAsync();
		if (result == ContentDialogResult::Primary)
		{
			auto magnet = to_string(magnetBox.Text());
			if (!magnet.empty())
			{
				auto save = to_string(savePathBox.Text());
				if (save.empty()) save = ".";
				co_await ::OpenNet::Core::P2PManager::Instance().AddMagnetAsync(magnet, save);
			}
		}
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
		if (!listView) return;

		auto speedGraph = TaskSpeedGraph();

		// Unsubscribe from previous task's property changes
		UnsubscribeFromSelectedTaskChanges();

		auto selectedItem = listView.SelectedItem();
		if (selectedItem)
		{
			auto taskVm = selectedItem.try_as<winrt::OpenNet::ViewModels::TaskViewModel>();
			if (taskVm && m_viewModel)
			{
				// Reset the speed graph when selecting a different task
				if (speedGraph)
				{
					speedGraph.Reset();
				}

				m_viewModel.SelectedTask(taskVm);

				// Subscribe to the new task's property changes
				SubscribeToSelectedTaskChanges(taskVm);

				UpdateSpeedGraphForSelectedTask();
			}
		}
		else
		{
			if (m_viewModel)
			{
				m_viewModel.SelectedTask(nullptr);
			}
			// Reset speed graph when no task is selected
			if (speedGraph)
			{
				speedGraph.Reset();
			}
		}
	}

	void TasksPage::TaskSpeedGraph_SizeChanged(winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e)
	{
		(void)sender;
		(void)e;
		// Update the speed graph when size changes
		UpdateSpeedGraphForSelectedTask();
	}

	void TasksPage::UpdateSpeedGraphForSelectedTask()
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
			UpdateSpeedGraphForSelectedTask();
		}
	}
}
