#include "pch.h"
#include "TasksPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TasksPage.g.cpp")
#include "UI/Xaml/View/Pages/TasksPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.Resources.h>
#include <winrt/Windows.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>
#include <format>
#include <limits>

#include <shobjidl.h> // For IInitializeWithWindow
#include <shellapi.h> // For ShellExecute
#include <winrt/Microsoft.UI.Windowing.h>
#include "Core/P2PManager.h"
#include "Core/DownloadManager.h"
#include "Core/HttpStateManager.h"
#include "Core/IO/FileSystem.h"
#include "Core/AppEnvironment.h"
#include "Controls/SpeedGraph/SpeedGraph.xaml.h"
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.xaml.h"
#include "UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.xaml.h"
#include "UI/Xaml/View/Dialog/HttpDownloadDialog.xaml.h"
#include "UI/Xaml/View/Pages/TaskSummaryPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskPeersListPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskTrackersPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskFilesPage.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::Windows::Storage::Pickers;

// https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.windows.storage.pickers#remarks

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	TasksPage::TasksPage()
	{
		InitializeComponent();

		// Keep page cached to preserve ViewModel when navigating away
		//this->NavigationCacheMode(winrt::Microsoft::UI::Xaml::Navigation::NavigationCacheMode::Enabled);

		// Create and attach the view-model
		m_viewModel = winrt::make<winrt::OpenNet::ViewModels::implementation::TasksViewModel>();

		// Subscribe to AddTaskRequested event (currently not used, but kept for compatibility)
		m_addTaskToken = m_viewModel.AddTaskRequested({this, &TasksPage::OnAddTaskRequested});

		// Set up bottom panel to show Summary by default
		if (auto frame = ContentFrame())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskSummaryPage>(), m_viewModel);
		}
	}

	TasksPage::~TasksPage()
	{
		if (m_viewModel && m_addTaskToken.value)
		{
			m_viewModel.AddTaskRequested(m_addTaskToken);
		}
	}

	// Handler invoked when the ViewModel requests adding a new task
	// Currently not used, kept for backward compatibility
	winrt::Windows::Foundation::IAsyncAction TasksPage::OnAddTaskRequested(IInspectable const &, winrt::hstring const &)
	{
		co_return;
	}

	// Show dialog for user to enter or paste a magnet link
	winrt::Windows::Foundation::IAsyncAction TasksPage::MenuItemAddFromLink_ClickAsync(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e)
	{
		try
		{
			auto dialog = make<winrt::OpenNet::UI::Xaml::View::Dialog::implementation::TorrentMetaDataDownloadDialog>();
			dialog.XamlRoot(this->XamlRoot());

			auto result = co_await dialog.ShowAsync();

			// If user clicked OK (Primary button), process the validated magnet link
			if (result == ContentDialogResult::Primary)
			{
				try
				{
					auto impl = dialog.as<winrt::OpenNet::UI::Xaml::View::Dialog::implementation::TorrentMetaDataDownloadDialog>();
					if (impl)
					{
						auto magnetLink = impl->GetMagnetLink();
						if (!magnetLink.empty())
						{
							ProcessAndShowTorrentMetadataWindow(magnetLink);
						}
					}
				}
				catch (const std::exception &ex)
				{
					OutputDebugStringW((L"ShowAddMagnetLinkDialog: GetMagnetLink error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
				}
			}
		}
		catch (const std::exception &ex)
		{
			OutputDebugStringW((L"ShowAddMagnetLinkDialog error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("ShowAddMagnetLinkDialog unknown error\n");
		}
	}

	// Show file picker for user to select a .torrent file
	winrt::Windows::Foundation::IAsyncAction TasksPage::MenuItemAddFromFile_ClickAsync(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e)
	{
		if (sender == MenuFlyoutItem())
		{
			OutputDebugStringA("MenuItemAddFromFile_Click: sender is MenuFlyoutItem, which may cause picker to not show. This is a known issue with WinUI 3. Consider using a different event source if picker fails to appear.\n");
			// Note: In some cases, if the sender is the MenuFlyoutItem itself, the file picker may fail to show due to focus issues. This is a quirk of WinUI 3. If you encounter this, consider using a different event source (like a button) to trigger the file picker.
			// However, we will still attempt to show the picker as is for compatibility.
		}
		try
		{
			auto control = sender.try_as<FrameworkElement>();
			auto picker = FileOpenPicker(control.XamlRoot().ContentIslandEnvironment().AppWindowId());
			picker.ViewMode(PickerViewMode::List);
			picker.SuggestedStartLocation(PickerLocationId::Downloads);
			picker.FileTypeFilter().Append(L".torrent");

			auto files = co_await picker.PickMultipleFilesAsync();
			if (files.Size() > 1)
			{
				ContentDialog multiFileCheckDialog = ContentDialog();
				multiFileCheckDialog.XamlRoot(control.XamlRoot());
				multiFileCheckDialog.RequestedTheme(control.ActualTheme());

				Microsoft::Windows::ApplicationModel::Resources::ResourceLoader resourceLoader = Microsoft::Windows::ApplicationModel::Resources::ResourceLoader();
				// resourceLoader.GetString(L"MultipleFilesSelectedMessage");
				multiFileCheckDialog.Title(box_value(L"Multiple Files Selected"));
				multiFileCheckDialog.Content(box_value(L"You picked multiple files. What do you want to do next?"));

				multiFileCheckDialog.PrimaryButtonText(L"Check in new windows");
				auto btnStyle = Microsoft::UI::Xaml::Style(xaml_typename<Button>());
				auto baseStyle = Application::Current().Resources().Lookup(box_value(L"DefaultButtonStyle")).as<winrt::Microsoft::UI::Xaml::Style>();

				btnStyle.BasedOn(baseStyle);
				// btnStyle.Setters().Append(Setter(Microsoft::UI::Xaml::FrameworkElement::WidthProperty(), box_value(800.0)));
				// btnStyle.Setters().Append(Setter(Microsoft::UI::Xaml::FrameworkElement::MaxWidthProperty(), box_value(1800.0)));
				multiFileCheckDialog.PrimaryButtonStyle(btnStyle);
				multiFileCheckDialog.SecondaryButtonText(L"Add to list");
				multiFileCheckDialog.DefaultButton(ContentDialogButton::Primary);
				multiFileCheckDialog.CloseButtonText(L"Cancel");

				auto result = co_await multiFileCheckDialog.ShowAsync();
				if (result == ContentDialogResult::Primary)
				{
					multiFileCheckDialog.Hide();
					for (auto const &file : files)
					{
						ProcessAndShowTorrentMetadataWindow(file.Path());
					}
				}
				else if (result == ContentDialogResult::Secondary)
				{
				}
				else
				{
					co_return;
				}
			}
			else if (files.Size() == 1)
			{
				ProcessAndShowTorrentMetadataWindow(files.GetAt(0).Path());
			}
			else
			{
#ifdef DEBUG
				OutputDebugStringW(L"MenuItemAddFromFile_ClickAsync: No file selected or user cancelled the picker\n");
#endif // DEBUG
			}
		}
		catch (const std::exception &ex)
		{
			OutputDebugStringW((L"ShowAddTorrentFileDialog error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("ShowAddTorrentFileDialog unknown error\n");
		}
	}

	// Show HTTP download dialog for adding HTTP/HTTPS/FTP downloads
	winrt::Windows::Foundation::IAsyncAction TasksPage::MenuItemAddFromHttp_ClickAsync(
		winrt::Windows::Foundation::IInspectable const & /*sender*/,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const & /*e*/)
	{
		try
		{
			auto dialog = make<winrt::OpenNet::UI::Xaml::View::Dialog::implementation::HttpDownloadDialog>();
			dialog.XamlRoot(this->XamlRoot());

			auto result = co_await dialog.ShowAsync();

			if (result == ContentDialogResult::Primary)
			{
				try
				{
					auto url = winrt::to_string(dialog.Url());
					auto dir = winrt::to_string(dialog.SaveDirectory());
					auto fileName = winrt::to_string(dialog.FileName());

					if (!url.empty())
					{
						auto &dlMgr = ::OpenNet::Core::DownloadManager::Instance();
						if (dlMgr.IsAria2Available())
						{
							// Move off UI thread – SimplePost blocks with .get()
							co_await winrt::resume_background();
							auto gid = dlMgr.AddHttpDownload(url, dir, fileName);
							if (!gid.empty())
							{
								OutputDebugStringW((L"HTTP download added with GID: " + winrt::to_hstring(gid) + L"\n").c_str());
							}
							else
							{
								OutputDebugStringW(L"Failed to add HTTP download\n");
							}
						}
						else
						{
							// Show error: aria2 not available
							ContentDialog errorDialog;
							errorDialog.XamlRoot(this->XamlRoot());
							errorDialog.Title(box_value(L"HTTP Download Unavailable"));
							errorDialog.Content(box_value(L"The aria2 download engine is not available. Please ensure aria2c.exe is present alongside the application."));
							errorDialog.CloseButtonText(L"OK");
							co_await errorDialog.ShowAsync();
						}
					}
				}
				catch (const std::exception &ex)
				{
					OutputDebugStringW((L"HTTP download add error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
				}
			}
		}
		catch (const std::exception &ex)
		{
			OutputDebugStringW((L"HttpDownloadDialog error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("HttpDownloadDialog unknown error\n");
		}
	}

	// Process the torrent link/file and show the metadata check window
	void TasksPage::ProcessAndShowTorrentMetadataWindow(hstring const &torrentLink)
	{
		if (torrentLink.empty())
		{
			return;
		}

		try
		{
			// Create a shared_ptr to keep the window alive during async operations
			auto checkWindow = winrt::make_self<winrt::OpenNet::UI::Xaml::View::Windows::implementation::TorrentCheckModalWindow>(torrentLink);
			checkWindow->Activate();
			// The window manages its own lifetime - it will close when user closes it or operations complete
		}
		catch (const std::exception &ex)
		{
			// Log error if needed
			OutputDebugStringW(L"Error creating torrent check window: ");
			OutputDebugStringW(winrt::to_hstring(ex.what()).c_str());
		}
		catch (...)
		{
			OutputDebugStringW(L"Unknown error creating torrent check window");
		}
	}

	void TasksPage::FilterNavView_SelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const & /*sender*/, Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const &args)
	{
		auto item = args.SelectedItem().try_as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
		if (!item)
			return;
		auto tag = unbox_value_or<winrt::hstring>(item.Tag(), L"");
		if (tag.empty())
			return;
		if (m_viewModel)
		{
			m_viewModel.ApplyFilter(tag);
		}
	}

	void TasksPage::TasksList_SelectionChanged(winrt::Windows::Foundation::IInspectable const & /*sender*/, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const & /*args*/)
	{
		auto listView = TasksList();
		if (!listView || !m_viewModel)
			return;

		auto selectedItem = listView.SelectedItem();
		auto taskVm = selectedItem.try_as<winrt::OpenNet::ViewModels::TaskViewModel>();

		m_viewModel.SelectedTask(taskVm);
		// SpeedGraph subscription is handled by TaskSummaryPage via ViewModel.PropertyChanged("SelectedTask").
	}

	void TasksPage::TasksList_RightTapped(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const &args)
	{
		// Context flyout is handled automatically by XAML
	}

	void TasksPage::SearchBox_TextChanged(winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const &sender, winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const & /*args*/)
	{
		if (m_viewModel)
		{
			m_viewModel.SetSearchFilter(sender.Text());
		}
	}

	void TasksPage::Task_SelectBar_SelectionChanged(
		winrt::Microsoft::UI::Xaml::Controls::SelectorBar const &sender,
		winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const & /*args*/)
	{
		auto selectedItem = sender.SelectedItem();
		auto frame = ContentFrame();
		if (!selectedItem || !frame)
			return;

		if (selectedItem == SummaryContent())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskSummaryPage>(), m_viewModel);
		}
		else if (selectedItem == PeersList())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskPeersListPage>(), m_viewModel);
		}
		else if (selectedItem == TrackersList())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskTrackersPage>(), m_viewModel);
		}
		else if (selectedItem == FilesList())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskFilesPage>(), m_viewModel);
		}
	}

	winrt::Windows::Foundation::IAsyncAction TasksPage::RenameTaskMenuItem_ClickAsync(
		winrt::Windows::Foundation::IInspectable const& /*sender*/,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*args*/)
	{
		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			co_return;
		}

		auto task = m_viewModel.SelectedTask();
		auto currentName = task.Name();

		// Build rename dialog
		TextBox inputBox;
		inputBox.Text(currentName);
		inputBox.PlaceholderText(L"Enter new name");
		inputBox.AcceptsReturn(false);
		inputBox.SelectAll();

		ContentDialog renameDialog;
		renameDialog.XamlRoot(this->XamlRoot());
		renameDialog.Title(box_value(L"Rename"));
		renameDialog.Content(inputBox);
		renameDialog.PrimaryButtonText(L"OK");
		renameDialog.CloseButtonText(L"Cancel");
		renameDialog.DefaultButton(ContentDialogButton::Primary);

		auto result = co_await renameDialog.ShowAsync();
		if (result != ContentDialogResult::Primary)
			co_return;

		auto newName = inputBox.Text();
		if (newName.empty() || newName == currentName)
			co_return;

		// Update display
		task.Name(newName);

		// Persist based on task type
		auto taskType = task.TaskType();
		auto taskId = winrt::to_string(task.TaskId());
		auto newNameStr = winrt::to_string(newName);

		co_await winrt::resume_background();

		try
		{
			if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
			{
				auto& p2p = ::OpenNet::Core::P2PManager::Instance();
				if (p2p.StateManager())
				{
					p2p.StateManager()->UpdateTaskName(taskId, newNameStr);
				}
			}
			else if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::Http)
			{
				// TaskId now holds the stable recordId (not GID).
				// If taskId looks like a GID (not a recordId), try looking up the real recordId.
				auto& httpMgr = ::OpenNet::Core::HttpStateManager::Instance();
				auto rec = httpMgr.FindByRecordId(taskId);
				if (rec.has_value())
				{
					httpMgr.UpdateRecordName(taskId, newNameStr);
				}
				else
				{
					// Fallback: taskId might still be a GID from old data
					auto gidStr = winrt::to_string(task.Gid());
					auto recordId = ::OpenNet::Core::DownloadManager::Instance().GetRecordIdForGid(
						gidStr.empty() ? taskId : gidStr);
					if (!recordId.empty())
						httpMgr.UpdateRecordName(recordId, newNameStr);
				}
			}
		}
		catch (...)
		{
			OutputDebugStringA("RenameTaskMenuItem_ClickAsync: Error persisting new name\n");
		}
	}

	void TasksPage::MoveTaskMenuItem_Click(winrt::Windows::Foundation::IInspectable const & /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const & /*args*/)
	{
		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			return;
		}

		// 调用异步方法 - 不要等待，让它后台运行
		// fire_and_forget
		PerformMoveTaskAsync();
	}

	winrt::Windows::Foundation::IAsyncAction TasksPage::PerformMoveTaskAsync()
	{
		try
		{
			auto selectedTask = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
			if (!selectedTask)
			{
				co_return;
			}

			auto folderPicker = FolderPicker(XamlRoot().ContentIslandEnvironment().AppWindowId());
			folderPicker.ViewMode(PickerViewMode::List);
			folderPicker.SuggestedStartLocation(PickerLocationId::ComputerFolder);

			auto selectedFolder = co_await folderPicker.PickSingleFolderAsync();
			if (!selectedFolder)
			{
				OutputDebugStringA("PerformMoveTaskAsync: User cancelled folder selection\n");
				co_return; // 用户取消
			}

			auto newPath = winrt::to_string(selectedFolder.Path());
			OutputDebugStringW((L"PerformMoveTaskAsync: Selected path: " + std::wstring(selectedFolder.Path().c_str()) + L"\n").c_str());

			// TODO: Step 2: 获取当前任务的下载路径
			// std::string currentPath = m_currentSubscribedTask.GetDownloadPath();

			// TODO: Step 3: 验证磁盘空间
			// auto requiredSpace = FileOperation::GetDirectorySize(currentPath);
			// auto availableSpace = FileOperation::GetAvailableSpace(newPath);
			// if (availableSpace < requiredSpace) { 显示错误; co_return; }

			// Step 4: 执行移动操作（带进度回调）
			auto progressCallback = [this](size_t current, size_t total)
			{
				OutputDebugStringW(std::format(L"Move progress: {}/{}\n", current, total).c_str());
				// TODO: 更新进度条UI
			};

			// TODO: bool success = FileOperation::MoveDirectory(currentPath, newPath, progressCallback);

			// Step 5: 更新数据库和UI
			// if (success) {
			//   database.UpdateTaskPath(taskId, newPath);
			//   viewModel.RefreshTask(taskId);
			// }

			OutputDebugStringW((L"Move task completed to: " + std::wstring(newPath.begin(), newPath.end()) + L"\n").c_str());
		}
		catch (const std::exception &ex)
		{
			OutputDebugStringW((L"PerformMoveTaskAsync error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("PerformMoveTaskAsync unknown error\n");
		}

		co_return;
	}

	void TasksPage::OpenTaskLocationMenuItem_Click(winrt::Windows::Foundation::IInspectable const & /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const & /*args*/)
	{
		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			return;
		}

		try
		{
			// TODO: 获取任务的下载路径
			// std::string taskPath = m_currentSubscribedTask.GetDownloadPath();

			// 临时使用AppData路径作为示例
			std::string taskPath = ::winrt::OpenNet::Core::IO::FileSystem::GetAppDataPath();

			// 验证路径存在
			if (!::winrt::OpenNet::Core::IO::FileSystem::DirectoryExists(taskPath))
			{
				OutputDebugStringW(L"Task path does not exist\n");
				return;
			}

			// 打开文件浏览器
			std::wstring wpath(taskPath.begin(), taskPath.end());
			HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", wpath.c_str(), nullptr, SW_SHOW);

			if ((intptr_t)result <= 32)
			{
				OutputDebugStringW(L"Failed to open file explorer\n");
			}
			else
			{
				OutputDebugStringW((L"Opened location: " + wpath + L"\n").c_str());
			}
		}
		catch (const std::exception &ex)
		{
			OutputDebugStringW(winrt::to_hstring(ex.what()).c_str());
		}
		catch (...)
		{
			OutputDebugStringW(L"Unknown error opening task location\n");
		}
	}

	winrt::Windows::Foundation::IAsyncAction TasksPage::PropertiesMenuItem_Click(winrt::Windows::Foundation::IInspectable const & /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const & /*args*/)
	{
		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			co_return;
		}

		co_await ShowTaskPropertiesAsync();
	}

	winrt::Windows::Foundation::IAsyncAction TasksPage::ShowTaskPropertiesAsync()
	{
		try
		{
			auto task = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
			if (!task)
			{
				co_return;
			}

			// TODO: 创建并显示任务属性对话框
			// auto propertiesDialog = make<TaskPropertiesDialog>(task);
			// auto result = co_await propertiesDialog.ShowAsync();

			// 临时实现：显示调试输出
			auto taskName = task.Name();
			auto size = task.Size();
			auto progress = task.Progress();

			auto message = std::format(
				L"Task Properties:\nName: {}\nSize: {}\nProgress: {}\n",
				std::wstring_view{ taskName },
				std::wstring_view{ size },
				std::wstring_view{ progress });

			OutputDebugStringW(message.c_str());

			// TODO: 显示为对话框而不是调试输出
		}
		catch (const std::exception &ex)
		{
			OutputDebugStringW(winrt::to_hstring(ex.what()).c_str());
		}
		catch (...)
		{
			OutputDebugStringW(L"Unknown error showing task properties\n");
		}

		co_return;
	}
}