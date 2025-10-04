#include "pch.h"
#include "TasksPage.xaml.h"
#if __has_include("Pages/TasksPage.g.cpp")
#include "Pages/TasksPage.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.AccessCache.h>
#include "Core/P2PManager.h"
#include "Helpers/WindowHelper.h"
#include <shobjidl.h> // For IInitializeWithWindow

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::OpenNet::Pages::implementation
{
	TasksPage::TasksPage()
	{
		InitializeComponent();
		// Create and attach the view-model
		m_viewModel = winrt::make<winrt::OpenNet::ViewModels::implementation::TasksViewModel>();
		// for runtime binding we may do not need the data context
		this->DataContext(m_viewModel);
		//this->CacheMode();
		// subscribe to AddTaskRequested to show modal dialog
		m_addTaskToken = m_viewModel.AddTaskRequested({ this,&TasksPage::OnAddTaskRequested });
	}

	TasksPage::~TasksPage()
	{
		if (m_viewModel && m_addTaskToken.value)
		{
			m_viewModel.AddTaskRequested(m_addTaskToken);
		}
	}

	void TasksPage::OnAddTaskRequested(IInspectable const&, winrt::hstring const&)
	{
		ShowAddMagnetDialog();
	}

	Windows::Foundation::IAsyncOperation<hstring> TasksPage::PickFolderAsync()
	{
		Windows::Storage::Pickers::FolderPicker picker;
		picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::Desktop);
		picker.FileTypeFilter().Append(L"*");

		if (HWND hwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetNativeWindowHandleForElement(*this))
		{
			// 初始化窗口句柄 (IInitializeWithWindow)
			if (auto init = picker.as<::IInitializeWithWindow>())
			{
				init->Initialize(hwnd);
			}
			OutputDebugString(L"Pick path success");
		}

		Windows::Storage::StorageFolder folder = co_await picker.PickSingleFolderAsync();
		if (folder)
		{
			Windows::Storage::AccessCache::StorageApplicationPermissions::FutureAccessList()
				.AddOrReplace(L"PickedFolderToken", folder);
			co_return folder.Path();
		}
		co_return hstring{};
	}

	fire_and_forget TasksPage::ShowAddMagnetDialog()
	{
		auto lifetime = get_strong();
		ContentDialog dialog;
		dialog.XamlRoot(this->XamlRoot());
		dialog.Title(box_value(L"添加磁力链接 / Add Magnet"));
		dialog.PrimaryButtonText(L"确定");
		dialog.CloseButtonText(L"取消");
		dialog.DefaultButton(ContentDialogButton::Primary);

		StackPanel panel; panel.Spacing(8);

		TextBlock magnetLabel; magnetLabel.Text(L"磁力链接 / Magnet Link:");
		TextBox magnetBox; magnetBox.PlaceholderText(L"magnet:?xt=..."); magnetBox.MinWidth(480);

		TextBlock saveLabel; saveLabel.Text(L"保存路径 / Save Path:");
		StackPanel pathRow; pathRow.Orientation(Orientation::Horizontal); pathRow.Spacing(6);
		TextBox savePathBox; savePathBox.Width(380); savePathBox.PlaceholderText(L"例如: C:\\Downloads (可选)");
		Button browseBtn; browseBtn.Content(box_value(L"浏览..."));

		/*browseBtn.Click([weakPage = get_weak(), savePathBox](IInspectable const&, RoutedEventArgs const&) {
			auto op = [weakPage, savePathBox]() -> winrt::fire_and_forget {
				if (auto page = weakPage.get())
				{
					auto folderPath = co_await page->PickFolderAsync();
					if (!folderPath.empty())
					{
						savePathBox.Text(folderPath);
					}
				}
				}();
						});*/

		// 替换原来的 browseBtn.Click 代码片段
		browseBtn.Click(
			[weakPage = get_weak(), savePathBox](IInspectable const&, RoutedEventArgs const&) -> winrt::fire_and_forget {
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
}
