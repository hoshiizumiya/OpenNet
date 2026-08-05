#include <shobjidl.h> // For IInitializeWithWindow
#include <shellapi.h> // For ShellExecute
#include <cstdlib>

#include "XamlWorkaround.h"
#include "TasksPage.xaml.h"
#if __has_include("UI/Xaml/View/Pages/TasksPage.g.cpp")
#include "UI/Xaml/View/Pages/TasksPage.g.cpp"
#endif

#include "Core/AppEnvironment.h"
#include "Controls/SpeedGraph/SpeedGraph.xaml.h"
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.xaml.h"
#include "UI/Xaml/View/Dialog/TorrentMetaDataDownloadDialog.xaml.h"
#include "UI/Xaml/View/Dialog/HttpDownloadDialog.xaml.h"
#include "UI/Xaml/View/Pages/TaskSummaryPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskSpeedGraphPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskPieceMapPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskPeersListPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskTrackersPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskFilesPage.xaml.h"

import OpenNet.App;
import OpenNet.Core.DownloadManager;
import OpenNet.Core.HttpStateManager;
import OpenNet.Core.IO.FileSystem;
import OpenNet.Core.P2PManager;
import OpenNet.Core.torrentCore.LibtorrentHandle;
import OpenNet.Core.torrentCore.TorrentStateManager;
import OpenNet.Extension.DependencyObjectExtensions;
import OpenNet.Factory.Window;
import OpenNet.Helpers.ColumnWidthHelper;
import OpenNet.UI.Xaml.Control.DataTableColumnVisibilityHelper;
import OpenNet.Helpers.ControlLengthHelper;
import OpenNet.Helpers.WindowHelper;
import OpenNet.UI.Xaml.Control.DataTableSortHelper;
import winrt.OpenNet.UI.Xaml.View.Pages.SettingsPages;
import winrt.OpenNet.UI.Xaml.View.Windows;
import winrt.Windows.ApplicationModel.DataTransfer;
import winrt.Windows.Foundation;
import winrt.Windows.System;
import winrt.Windows.UI.Xaml.Navigation;
import winrt.Microsoft.UI.Content;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Controls;
import winrt.Microsoft.UI.Xaml.Data;
import winrt.Microsoft.UI.Xaml.Input;
import winrt.Microsoft.UI.Xaml.Media;
import winrt.Microsoft.UI.Windowing;
import winrt.Microsoft.Windows.ApplicationModel.Resources;
import winrt.Microsoft.Windows.Storage.Pickers;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::Windows::Storage::Pickers;


// https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.windows.storage.pickers#remarks

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	std::map<std::wstring, TasksPage::PersistedScrollState> TasksPage::s_persistedScrollStates;

	namespace
	{
		using TorrentDetailInfo =
			::OpenNet::Core::Torrent::LibtorrentHandle::TorrentDetailInfo;

		hstring GetTaskPersistenceKey(winrt::OpenNet::ViewModels::TaskViewModel const& item)
		{
			if (!item)
			{
				return {};
			}

			if (auto const taskId = item.TaskId(); !taskId.empty())
			{
				return taskId;
			}

			if (auto const gid = item.Gid(); !gid.empty())
			{
				return gid;
			}

			return item.Name();
		}

		bool IsBitTorrentTask(
			winrt::OpenNet::ViewModels::TaskViewModel const& task)
		{
			return task && task.TaskType() ==
				winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent;
		}

		std::optional<TorrentDetailInfo> GetTorrentDetail(
			winrt::OpenNet::ViewModels::TaskViewModel const& task)
		{
			if (!IsBitTorrentTask(task))
			{
				return std::nullopt;
			}

			auto const taskId = winrt::to_string(task.TaskId());
			auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
			if (!core || taskId.empty())
			{
				return std::nullopt;
			}

			auto detail = core->GetTorrentDetail(taskId);
			if (detail.name.empty() && detail.infoHash.empty() &&
				detail.savePath.empty() && detail.files.empty())
			{
				return std::nullopt;
			}
			return detail;
		}

		std::optional<::OpenNet::Core::Torrent::TaskMetadata> GetTaskMetadata(
			winrt::OpenNet::ViewModels::TaskViewModel const& task)
		{
			if (!IsBitTorrentTask(task))
			{
				return std::nullopt;
			}
			auto* manager = ::OpenNet::Core::P2PManager::Instance().StateManager();
			if (!manager)
			{
				return std::nullopt;
			}
			return manager->LoadTaskMetadata(winrt::to_string(task.TaskId()));
		}

		std::filesystem::path GetTaskSavePath(
			winrt::OpenNet::ViewModels::TaskViewModel const& task,
			std::optional<TorrentDetailInfo> const& detail)
		{
			if (task && task.TaskType() ==
				winrt::OpenNet::ViewModels::DownloadTaskType::Http)
			{
				auto const record =
					::OpenNet::Core::HttpStateManager::Instance().FindByRecordId(
						winrt::to_string(task.TaskId()));
				if (record && !record->savePath.empty())
				{
					return std::filesystem::path{
						winrt::to_hstring(record->savePath).c_str() };
				}
				return {};
			}
			if (detail && !detail->savePath.empty())
			{
				return std::filesystem::path{ winrt::to_hstring(detail->savePath).c_str() };
			}
			if (auto metadata = GetTaskMetadata(task); metadata && !metadata->savePath.empty())
			{
				return std::filesystem::path{ winrt::to_hstring(metadata->savePath).c_str() };
			}
			return {};
		}

		std::filesystem::path GetTaskSavePath(
			winrt::OpenNet::ViewModels::TaskViewModel const& task)
		{
			return GetTaskSavePath(task, GetTorrentDetail(task));
		}

		std::filesystem::path GetTaskFilePath(
			TorrentDetailInfo const& detail,
			bool preview)
		{
			auto const isPreviewable = [](std::filesystem::path const& path)
			{
				auto extension = path.extension().wstring();
				std::ranges::transform(
					extension, extension.begin(),
					[](wchar_t value)
				{
					return static_cast<wchar_t>(::towlower(value));
				});
				static constexpr std::array<std::wstring_view, 13> extensions{
					L".mp4", L".mkv", L".avi", L".mov", L".wmv",
					L".webm", L".mp3", L".flac", L".wav", L".aac",
					L".ogg", L".jpg", L".png"
				};
				return std::ranges::find(extensions, extension) != extensions.end();
			};

			for (auto const& file : detail.files)
			{
				if (file.priority <= 0 ||
					(preview ? file.bytesCompleted <= 0
					 : file.bytesCompleted < file.size))
				{
					continue;
				}
				auto relativePath = std::filesystem::path{
					winrt::to_hstring(file.path).c_str() };
				if (preview && !isPreviewable(relativePath))
				{
					continue;
				}
				auto path = std::filesystem::path{
					winrt::to_hstring(detail.savePath).c_str() } / relativePath;
				std::error_code error;
				if (std::filesystem::exists(path, error) && !error)
				{
					return path;
				}
			}
			return {};
		}

		std::filesystem::path GetTaskFilePath(
			winrt::OpenNet::ViewModels::TaskViewModel const& task,
			bool preview)
		{
			auto const detail = GetTorrentDetail(task);
			return detail ? GetTaskFilePath(*detail, preview)
				: std::filesystem::path{};
		}

		void CopyTextToClipboard(winrt::hstring const& text)
		{
			if (text.empty())
			{
				return;
			}
			winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
			package.SetText(text);
			winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
		}

		winrt::hstring GetMagnetUri(
			winrt::OpenNet::ViewModels::TaskViewModel const& task,
			std::optional<TorrentDetailInfo> const& detail)
		{
			if (auto metadata = GetTaskMetadata(task); metadata && !metadata->magnetUri.empty())
			{
				return winrt::to_hstring(metadata->magnetUri);
			}
			if (detail && !detail->infoHash.empty())
			{
				auto const name = winrt::Windows::Foundation::Uri::EscapeComponent(
					winrt::to_hstring(detail->name));
				return L"magnet:?xt=urn:btih:" + winrt::to_hstring(detail->infoHash) +
					L"&dn=" + name;
			}
			return {};
		}

		winrt::hstring GetMagnetUri(
			winrt::OpenNet::ViewModels::TaskViewModel const& task)
		{
			return GetMagnetUri(task, GetTorrentDetail(task));
		}

		bool OpenShellPath(std::filesystem::path const& path)
		{
			std::error_code error;
			if (path.empty() || !std::filesystem::exists(path, error) || error)
			{
				return false;
			}
			return reinterpret_cast<std::intptr_t>(ShellExecuteW(
				nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOW)) > 32;
		}

		winrt::hstring FormatByteCount(std::uint64_t value)
		{
			static constexpr std::array<std::wstring_view, 5> units{
				L"B", L"KB", L"MB", L"GB", L"TB" };
			double displayValue = static_cast<double>(value);
			std::size_t unit{};
			while (displayValue >= 1024.0 && unit + 1 < units.size())
			{
				displayValue /= 1024.0;
				++unit;
			}
			if (unit == 0)
			{
				return winrt::hstring{ std::format(
					L"{:.0f} {}", displayValue, units[unit]) };
			}
			return winrt::hstring{ std::format(
				L"{:.2f} {}", displayValue, units[unit]) };
		}

		double ParseLeadingNumber(winrt::hstring const& value)
		{
			wchar_t* end{};
			return std::wcstod(value.c_str(), &end);
		}
	}

	TasksPage::TasksPage()
	{
		// Keep page cached to preserve ViewModel when navigating away
		//this->NavigationCacheMode(winrt::Microsoft::UI::Xaml::Navigation::NavigationCacheMode::Enabled);

		// Create and attach the view-model
		m_viewModel = winrt::make<winrt::OpenNet::ViewModels::implementation::TasksViewModel>();
		m_filteredTasksChangedToken = m_viewModel.FilteredTasks().VectorChanged(
			[this](auto const&, auto const&)
		{
			if (m_isApplyingSort || m_sortDirection == 0 || m_sortPending)
				return;
			m_sortPending = true;
			DispatcherQueue().TryEnqueue([weak = get_weak()]()
			{
				if (auto self = weak.get())
				{
					self->m_sortPending = false;
					self->SortFilteredTasks();
				}
			});
		});

		// Subscribe to AddTaskRequested event (currently not used, but kept for compatibility)
		m_addTaskToken = m_viewModel.AddTaskRequested({ this, &TasksPage::OnAddTaskRequested });

		Unloaded([this](IInspectable const&, RoutedEventArgs const&)
		{
			SaveColumnWidths();
		});
	}

	TasksPage::~TasksPage()
	{
		if (m_viewModel && m_addTaskToken.value)
		{
			m_viewModel.AddTaskRequested(m_addTaskToken);
		}
		if (m_viewModel && m_filteredTasksChangedToken.value)
		{
			m_viewModel.FilteredTasks().VectorChanged(m_filteredTasksChangedToken);
		}
	}

	// https://github.com/microsoft/cppwinrt/blob/master/nuget/readme.md#initializecomponent
	void TasksPage::InitializeComponent()
	{
		TasksPageT::InitializeComponent();
		// Set up bottom panel to show Summary by default
		if (auto frame = ContentFrame())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskSummaryPage>(), m_viewModel);
		}
		UpdateTaskSortHeaders();
	}

	winrt::fire_and_forget TasksPage::Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto strong = get_strong();
		auto const tasksListHeight = ::OpenNet::Helpers::GetControlHeight("TasksPage_ContentFrame_Height");
		if (tasksListHeight > 0.0)
		{
			TasksListRow().Height(GridLength(tasksListHeight, GridUnitType::Pixel));
		}
		RestoreScrollPositionAsync(m_currentFilterKey);
		co_return;
	}

	// Save the current scroll position and selected item when navigating away
	void TasksPage::OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const&)
	{
		SaveScrollPosition(m_currentFilterKey);
	}

	// Restore saved column widths
	void TasksPage::DataTable_Loaded(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		RestoreColumnWidths();
	}

	void TasksPage::GridSplitter_PointerReleased(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& /*e*/)
	{
		::OpenNet::Helpers::SaveControlHeight("TasksPage_ContentFrame_Height", TasksListRow().ActualHeight());
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingNameTextBlock()
	{
		return m_textWrappingNameTextBlock;
	}
	void TasksPage::TextWrappingNameTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingNameTextBlock, value, L"TextWrappingNameTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingSizeTextBlock()
	{
		return m_textWrappingSizeTextBlock;
	}
	void TasksPage::TextWrappingSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingSizeTextBlock, value, L"TextWrappingSizeTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingProgressTextBlock()
	{
		return m_textWrappingProgressTextBlock;
	}
	void TasksPage::TextWrappingProgressTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingProgressTextBlock, value, L"TextWrappingProgressTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingDownloadSizeTextBlock()
	{
		return m_textWrappingDownloadSizeTextBlock;
	}
	void TasksPage::TextWrappingDownloadSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingDownloadSizeTextBlock, value, L"TextWrappingDownloadSizeTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingUploadSizeTextBlock()
	{
		return m_textWrappingUploadSizeTextBlock;
	}

	void TasksPage::TextWrappingUploadSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingUploadSizeTextBlock, value, L"TextWrappingUploadSizeTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingTotalDownloadSizeTextBlock()
	{
		return m_textWrappingTotalDownloadSizeTextBlock;
	}

	void TasksPage::TextWrappingTotalDownloadSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingTotalDownloadSizeTextBlock, value, L"TextWrappingTotalDownloadSizeTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingTotalUploadSizeTextBlock()
	{
		return m_textWrappingTotalUploadSizeTextBlock;
	}

	void TasksPage::TextWrappingTotalUploadSizeTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingTotalUploadSizeTextBlock, value, L"TextWrappingTotalUploadSizeTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingDLRateTextBlock()
	{
		return m_textWrappingDLRateTextBlock;
	}
	void TasksPage::TextWrappingDLRateTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingDLRateTextBlock, value, L"TextWrappingDLRateTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingULRateTextBlock()
	{
		return m_textWrappingULRateTextBlock;
	}
	void TasksPage::TextWrappingULRateTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingULRateTextBlock, value, L"TextWrappingULRateTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingRemainingTextBlock()
	{
		return m_textWrappingRemainingTextBlock;
	}
	void TasksPage::TextWrappingRemainingTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingRemainingTextBlock, value, L"TextWrappingRemainingTextBlock");
	}

	winrt::Microsoft::UI::Xaml::TextWrapping TasksPage::TextWrappingAddDateTextBlock()
	{
		return m_textWrappingAddDateTextBlock;
	}
	void TasksPage::TextWrappingAddDateTextBlock(winrt::Microsoft::UI::Xaml::TextWrapping const& value)
	{
		SetProperty(m_textWrappingAddDateTextBlock, value, L"TextWrappingAddDateTextBlock");
	}

	// Handler invoked when the ViewModel requests adding a new task
	// Currently not used, kept for backward compatibility
	winrt::Windows::Foundation::IAsyncAction TasksPage::OnAddTaskRequested(IInspectable const&, winrt::hstring const&)
	{
		co_return;
	}

	// Show dialog for user to enter or paste a magnet link
	winrt::Windows::Foundation::IAsyncAction TasksPage::MenuItemAddFromLink_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
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
				catch (const std::exception& ex)
				{
					OutputDebugStringW((L"ShowAddMagnetLinkDialog: GetMagnetLink error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
				}
			}
		}
		catch (const std::exception& ex)
		{
			OutputDebugStringW((L"ShowAddMagnetLinkDialog error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("ShowAddMagnetLinkDialog unknown error\n");
		}
	}

	// Show file picker for user to select a .torrent file
	winrt::Windows::Foundation::IAsyncAction TasksPage::MenuItemAddFromFile_ClickAsync(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
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
				auto baseStyle = Microsoft::UI::Xaml::Application::Current().Resources().Lookup(box_value(L"DefaultButtonStyle")).as<winrt::Microsoft::UI::Xaml::Style>();

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
					for (auto const& file : files)
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
		catch (const std::exception& ex)
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
		winrt::Windows::Foundation::IInspectable const& /*sender*/,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
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
						auto& dlMgr = ::OpenNet::Core::DownloadManager::Instance();
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
				catch (const std::exception& ex)
				{
					OutputDebugStringW((L"HTTP download add error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
				}
			}
		}
		catch (const std::exception& ex)
		{
			OutputDebugStringW((L"HttpDownloadDialog error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("HttpDownloadDialog unknown error\n");
		}
	}

	void TasksPage::ViewTasksPagePortTestAppBarButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		auto window = winrt::OpenNet::UI::Xaml::View::Windows::NATDetectorWindow();
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
		window.Activate();
	}

	void TasksPage::ViewTasksPageSettingsAppBarButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		auto window = ::OpenNet::Factory::Window::WindowFactory::CreateStandardWindow();
		window.Content(winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::MainSettingsPage());
		window.Activate();
		return;
	}

	void TasksPage::PortTestKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
	{
		auto window = winrt::OpenNet::UI::Xaml::View::Windows::NATDetectorWindow();
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(window);
		window.Activate();
		args.Handled(true);
	}

	void TasksPage::SettingKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
	{
		ViewTasksPageSettingsAppBarButton_Click(nullptr, nullptr);
	}

	void TasksPage::SearchKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const&, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
	{
		auto strong = get_strong();
		SearchBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Keyboard);

		if (auto textBox = ::OpenNet::Extension::DependencyObjectEx::FindDescendant<winrt::Microsoft::UI::Xaml::Controls::TextBox>(SearchBox()))
		{
			textBox.SelectAll();
		}

		args.Handled(true);
	}

	// Process the torrent link/file and show the metadata check window
	void TasksPage::ProcessAndShowTorrentMetadataWindow(hstring const& torrentLink)
	{
		if (torrentLink.empty())
		{
			return;
		}

		try
		{
			// Create a shared_ptr to keep the window alive during async operations
			auto checkWindow = winrt::make_self<winrt::OpenNet::UI::Xaml::View::Windows::implementation::TorrentCheckModalWindow>(torrentLink);
			::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(*checkWindow);
			checkWindow->Activate();
			// The window manages its own lifetime - it will close when user closes it or operations complete
		}
		catch (const std::exception& ex)
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

	void TasksPage::FilterNavView_SelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const& /*sender*/, Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args)
	{
		auto item = args.SelectedItem().try_as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
		if (!item)
			return;
		auto tag = unbox_value_or<winrt::hstring>(item.Tag(), L"");
		if (tag.empty())
			return;
		if (m_viewModel && tag != m_currentFilterKey)
		{
			SaveScrollPosition(m_currentFilterKey);
			m_currentFilterKey = tag;
			m_viewModel.ApplyFilter(tag);

			// ApplyFilter queues the collection update. Queue restoration after it
			// so GetItem sees the items belonging to the newly selected filter.
			auto weak = get_weak();
			DispatcherQueue().TryEnqueue([weak, tag]()
			{
				if (auto self = weak.get())
				{
					self->RestoreScrollPositionAsync(tag);
				}
			});
		}
	}

	void TasksPage::TasksList_ContainerContentChanging(winrt::Microsoft::UI::Xaml::Controls::ListViewBase const&, winrt::Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const& args)
	{
		// This function manually sets the height of the item ListViewPersistenceHelper is attempting to scroll to. We need to set the height
		// because if the item is not fully rendered at the time of scrolling, it can return an incorrect height and cause ListViewPersistenceHelper 
		// to overscroll. 
		// A recycled container can retain a locally set Height. Always clear it
		// before the container is reused, regardless of which item it held.
		if (args.InRecycleQueue())
		{
			args.ItemContainer().ClearValue(FrameworkElement::HeightProperty());
			return;
		}

		if (!m_isRestoringScrollPosition || m_restoringFilterKey.empty())
		{
			return;
		}

		auto const stateIt = s_persistedScrollStates.find(std::wstring{ m_restoringFilterKey.c_str() });
		if (stateIt == s_persistedScrollStates.end())
		{
			return;
		}

		auto const& state = stateIt->second;
		auto item = args.Item().try_as<winrt::OpenNet::ViewModels::TaskViewModel>();
		if (item &&
			!state.itemKey.empty() &&
			state.itemContainerHeight > 0.0 &&
			GetTaskPersistenceKey(item) == state.itemKey)
		{
			// The stored height is only applied while ListViewPersistenceHelper
			// is restoring this filter's relative position.
			args.ItemContainer().Height(state.itemContainerHeight);
		}
	}

	void TasksPage::TasksList_SelectionChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*args*/)
	{
		auto listView = TasksList();
		if (!listView || !m_viewModel)
			return;

		auto selectedItem = listView.SelectedItem();
		auto taskVm = selectedItem.try_as<winrt::OpenNet::ViewModels::TaskViewModel>();

		m_viewModel.SelectedTask(taskVm);
		// SpeedGraph subscription is handled by TaskSummaryPage via ViewModel.PropertyChanged("SelectedTask").
	}

	void TasksPage::TasksList_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args)
	{
		auto listView = sender.try_as<ListView>();
		if (!listView)
		{
			return;
		}

		auto source = args.OriginalSource().try_as<DependencyObject>();
		while (source)
		{
			if (auto container = source.try_as<ListViewItem>())
			{
				listView.SelectedItem(container.Content());
				return;
			}

			source = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(source);
		}

		// A right-click on the empty list area must not leave an old task active.
		listView.SelectedItem(nullptr);
	}

	void TasksPage::SearchBox_TextChanged(winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& sender, winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& /*args*/)
	{
		if (m_viewModel)
		{
			CancelScrollRestore();
			m_viewModel.SetSearchFilter(sender.Text());
		}
	}

	void TasksPage::Task_SelectBar_SelectionChanged(
		winrt::Microsoft::UI::Xaml::Controls::SelectorBar const& sender,
		winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const& /*args*/)
	{
		auto selectedItem = sender.SelectedItem();
		auto frame = ContentFrame();
		if (!selectedItem || !frame)
			return;

		if (selectedItem == SummaryContent())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskSummaryPage>(), m_viewModel);
		}
		else if (selectedItem == SpeedGraphContent())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskSpeedGraphPage>(), m_viewModel);
		}
		else if (selectedItem == PieceMapContent())
		{
			frame.Navigate(winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TaskPieceMapPage>(), m_viewModel);
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

	void TasksPage::TasksColumnHeader_RightTapped(
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

			source = VisualTreeHelper::GetParent(source);
		}
	}

	void TasksPage::TasksColumnHeader_Click(
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
		UpdateTaskSortHeaders();
		SortFilteredTasks();
	}

	void TasksPage::UpdateTaskSortHeaders()
	{
		auto update = [this](auto const& button)
		{
			::OpenNet::UI::Xaml::Control::DataTableSortHelper::UpdateHeader(
				button, m_sortColumn, m_sortDirection);
		};
		update(SortTaskNameButton());
		update(SortTaskSizeButton());
		update(SortTaskProgressButton());
		update(SortTaskDownloadSizeButton());
		update(SortTaskUploadSizeButton());
		update(SortTaskTotalDownloadButton());
		update(SortTaskTotalUploadButton());
		update(SortTaskDlRateButton());
		update(SortTaskUlRateButton());
		update(SortTaskRemainingButton());
		update(SortTaskAddDateButton());
		update(SortTaskCompletedDateButton());
		update(SortTaskShareRatioButton());
		update(SortTaskSeedsButton());
		update(SortTaskPeersButton());
	}

	void TasksPage::SortFilteredTasks()
	{
		if (!m_viewModel || m_isApplyingSort) return;
		auto filtered = m_viewModel.FilteredTasks();
		if (!filtered) return;
		m_isApplyingSort = true;

		std::vector<winrt::OpenNet::ViewModels::TaskViewModel> items;
		items.reserve(filtered.Size());
		for (auto const& item : filtered)
			items.push_back(item);

		if (m_sortDirection == 0)
		{
			auto all = m_viewModel.Tasks();
			auto originalIndex = [all](auto const& item)
			{
				for (uint32_t index = 0; index < all.Size(); ++index)
					if (all.GetAt(index) == item) return index;
				return (std::numeric_limits<std::uint32_t>::max)();
			};
			std::stable_sort(items.begin(), items.end(),
							 [originalIndex](auto const& left, auto const& right)
			{
				return originalIndex(left) < originalIndex(right);
			});
		}
		else
		{
			auto const column = m_sortColumn;
			auto const direction = m_sortDirection;
			auto textValue = [column](auto const& item)
			{
				if (column == L"Name") return std::wstring(item.Name());
				if (column == L"Size") return std::wstring(item.Size());
				if (column == L"DownloadSize") return std::wstring(item.DownloadSize());
				if (column == L"UploadSize") return std::wstring(item.UploadSize());
				if (column == L"TotalDownloadSize") return std::wstring(item.TotalDownloadSize());
				if (column == L"TotalUploadSize") return std::wstring(item.TotalUploadSize());
				if (column == L"DLRate") return std::wstring(item.DownloadRate());
				if (column == L"ULRate") return std::wstring(item.UploadRate());
				if (column == L"Remaining") return std::wstring(item.Remaining());
				if (column == L"AddDate") return std::wstring(item.AddDate());
				if (column == L"CompletedDate") return std::wstring(item.CompletedDate());
				if (column == L"ShareRatio") return std::wstring(item.ShareRatio());
				if (column == L"Seeds") return std::wstring(item.Seeds());
				return std::wstring(item.Peers());
			};
			std::stable_sort(items.begin(), items.end(),
							 [column, direction, textValue](auto const& left, auto const& right)
			{
				bool less;
				if (column == L"Progress")
					less = left.ProgressPercent() < right.ProgressPercent();
				else if (column == L"DLRate")
					less = left.DownloadSpeedKB() < right.DownloadSpeedKB();
				else if (column == L"ShareRatio")
					less = ParseLeadingNumber(left.ShareRatio()) < ParseLeadingNumber(right.ShareRatio());
				else if (column == L"Seeds")
					less = ParseLeadingNumber(left.Seeds()) < ParseLeadingNumber(right.Seeds());
				else if (column == L"Peers")
					less = ParseLeadingNumber(left.Peers()) < ParseLeadingNumber(right.Peers());
				else
					less = textValue(left) < textValue(right);
				if (direction == 1) return less;
				if (column == L"Progress")
					return right.ProgressPercent() < left.ProgressPercent();
				if (column == L"DLRate")
					return right.DownloadSpeedKB() < left.DownloadSpeedKB();
				if (column == L"ShareRatio")
					return ParseLeadingNumber(right.ShareRatio()) < ParseLeadingNumber(left.ShareRatio());
				if (column == L"Seeds")
					return ParseLeadingNumber(right.Seeds()) < ParseLeadingNumber(left.Seeds());
				if (column == L"Peers")
					return ParseLeadingNumber(right.Peers()) < ParseLeadingNumber(left.Peers());
				return textValue(right) < textValue(left);
			});
		}

		// Reorder incrementally so retained rows keep their identity. Clearing the
		// vector made every row look newly created and defeated RepositionThemeTransition.
		auto const selectedTask = TasksList()
			? TasksList().SelectedItem().try_as<
			winrt::OpenNet::ViewModels::TaskViewModel>()
			: nullptr;
		for (std::uint32_t targetIndex = 0;
			 targetIndex < static_cast<std::uint32_t>(items.size());
			 ++targetIndex)
		{
			if (targetIndex < filtered.Size() &&
				filtered.GetAt(targetIndex) == items[targetIndex])
			{
				continue;
			}

			std::uint32_t sourceIndex = targetIndex;
			while (sourceIndex < filtered.Size() &&
				   filtered.GetAt(sourceIndex) != items[targetIndex])
			{
				++sourceIndex;
			}

			if (sourceIndex < filtered.Size())
			{
				auto item = filtered.GetAt(sourceIndex);
				filtered.RemoveAt(sourceIndex);
				filtered.InsertAt(targetIndex, item);
			}
			else
			{
				filtered.InsertAt(targetIndex, items[targetIndex]);
			}
		}
		while (filtered.Size() > items.size())
		{
			filtered.RemoveAtEnd();
		}
		if (selectedTask && TasksList())
		{
			TasksList().SelectedItem(selectedTask);
		}
		m_isApplyingSort = false;
	}

	void TasksPage::TasksColumnMenuFlyout_Opening(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Windows::Foundation::IInspectable const&)
	{
		auto const hasItems = HasFilteredTasks();
		ViewPageTasksColumnAutoSizeSelectedWidth().IsEnabled(hasItems && m_contextColumn);
		ViewPageTasksColumnAutoSizeAllWidth().IsEnabled(hasItems);
		if (auto const menu = sender.try_as<MenuFlyout>())
		{
			for (auto const& entry : menu.Items())
			{
				if (auto const toggle = entry.try_as<ToggleMenuFlyoutItem>();
					toggle && toggle.Tag())
				{
					auto const column = ColumnForTag(
						winrt::unbox_value<winrt::hstring>(toggle.Tag()));
					if (column)
						toggle.IsChecked(column.Visibility() == Visibility::Visible);
				}
			}
		}
	}

	winrt::XamlToolkit::Labs::WinUI::DataColumn TasksPage::ColumnForTag(
		winrt::hstring const& tag)
	{
		if (tag == L"Name") return ColName();
		if (tag == L"Size") return ColSize();
		if (tag == L"Progress") return ColProgress();
		if (tag == L"DownloadSize") return ColDownloadSize();
		if (tag == L"UploadSize") return ColUploadSize();
		if (tag == L"TotalDownloadSize") return ColumnTotalDownloadSize();
		if (tag == L"TotalUploadSize") return ColumnTotalUploadSize();
		if (tag == L"DLRate") return ColDLRate();
		if (tag == L"ULRate") return ColULRate();
		if (tag == L"Remaining") return ColRemaining();
		if (tag == L"AddDate") return ColAddDate();
		if (tag == L"CompletedDate") return ColCompletedDate();
		if (tag == L"ShareRatio") return ColShareRatio();
		if (tag == L"Seeds") return ColSeeds();
		if (tag == L"Peers") return ColPeers();
		return nullptr;
	}

	void TasksPage::SetColumnSetting(winrt::hstring const& tag, bool value)
	{
		if (!m_viewModel) return;
		if (tag == L"Size") m_viewModel.IsColSizeLoad(value);
		else if (tag == L"Progress") m_viewModel.IsColProgressLoad(value);
		else if (tag == L"DownloadSize") m_viewModel.IsColDownloadSizeLoad(value);
		else if (tag == L"UploadSize") m_viewModel.IsColUploadSizeLoad(value);
		else if (tag == L"TotalDownloadSize") m_viewModel.IsColumnTotalDownloadSizeLoad(value);
		else if (tag == L"TotalUploadSize") m_viewModel.IsColumnTotalUploadSizeLoad(value);
		else if (tag == L"DLRate") m_viewModel.IsColDLRateLoad(value);
		else if (tag == L"ULRate") m_viewModel.IsColULRateLoad(value);
		else if (tag == L"Remaining") m_viewModel.IsColRemainingLoad(value);
		else if (tag == L"AddDate") m_viewModel.IsColAddDateLoad(value);
		else if (tag == L"CompletedDate") m_viewModel.IsColCompletedDateLoad(value);
		else if (tag == L"ShareRatio") m_viewModel.IsColShareRatioLoad(value);
		else if (tag == L"Seeds") m_viewModel.IsColSeedsLoad(value);
		else if (tag == L"Peers") m_viewModel.IsColPeersLoad(value);
	}

	void TasksPage::TasksColumnVisibility_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto const toggle = sender.try_as<ToggleMenuFlyoutItem>();
		if (!toggle || !toggle.Tag()) return;
		auto const tag = winrt::unbox_value<winrt::hstring>(toggle.Tag());
		auto const column = ColumnForTag(tag);
		if (!column) return;
		auto const visible = toggle.IsChecked();
		SetColumnSetting(tag, visible);
		column.Visibility(visible ? Visibility::Visible : Visibility::Collapsed);
		SynchronizeTaskRows();
	}

	void TasksPage::TaskDataRow_Loaded(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		std::array const columns{
			ColName(), ColSize(), ColProgress(), ColDownloadSize(), ColUploadSize(),
			ColumnTotalDownloadSize(), ColumnTotalUploadSize(), ColDLRate(), ColULRate(),
			ColRemaining(), ColAddDate(), ColCompletedDate(), ColShareRatio(), ColSeeds(),
			ColPeers() };
		::OpenNet::UI::Xaml::Control::DataTableColumnVisibilityHelper::SynchronizeRow(
			sender.try_as<winrt::XamlToolkit::Labs::WinUI::DataRow>(),
			columns.data(), static_cast<unsigned int>(columns.size()));
	}

	void TasksPage::SynchronizeTaskRows()
	{
		std::array const columns{
			ColName(), ColSize(), ColProgress(), ColDownloadSize(), ColUploadSize(),
			ColumnTotalDownloadSize(), ColumnTotalUploadSize(), ColDLRate(), ColULRate(),
			ColRemaining(), ColAddDate(), ColCompletedDate(), ColShareRatio(), ColSeeds(),
			ColPeers() };
		::OpenNet::UI::Xaml::Control::DataTableColumnVisibilityHelper::SynchronizeRealizedRows(
			TasksList(), columns.data(), static_cast<unsigned int>(columns.size()));
		TasksList().InvalidateMeasure();
	}

	void TasksPage::TasksColumnMenuFlyout_Closed(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Windows::Foundation::IInspectable const&)
	{
		m_contextColumn = nullptr;
	}

	void TasksPage::TasksContextMenuFlyout_Opening(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Windows::Foundation::IInspectable const&)
	{
		auto const task = m_viewModel
			? m_viewModel.SelectedTask()
			: winrt::OpenNet::ViewModels::TaskViewModel{ nullptr };
		auto const hasSelection = static_cast<bool>(task);
		auto const isBitTorrent = IsBitTorrentTask(task);
		auto const detail = GetTorrentDetail(task);
		auto const state = hasSelection
			? task.State()
			: winrt::OpenNet::ViewModels::DownloadTaskState::Pending;
		auto const isActive = state ==
			winrt::OpenNet::ViewModels::DownloadTaskState::Downloading ||
			state == winrt::OpenNet::ViewModels::DownloadTaskState::Seeding;
		auto const hasFile = detail && !GetTaskFilePath(*detail, false).empty();
		auto const hasPreview = detail && !GetTaskFilePath(*detail, true).empty();
		auto const magnetUri = GetMagnetUri(task, detail);
		auto const savePath = GetTaskSavePath(task, detail);

		StartTaskMenuItem().IsEnabled(hasSelection && !isActive);
		StopTaskMenuItem().IsEnabled(hasSelection && isActive);
		PreviewTaskMenuItem().IsEnabled(isBitTorrent && hasPreview);
		UpdateTrackerMenuItem().IsEnabled(isBitTorrent && detail.has_value());
		SuperSeedModeMenuItem().IsEnabled(
			isBitTorrent && detail.has_value() && task.ProgressPercent() >= 100.0);
		SuperSeedModeMenuItem().IsChecked(
			detail.has_value() && detail->isSuperSeeding);
		SequentialDownloadMenuItem().IsEnabled(
			isBitTorrent && detail.has_value() && task.ProgressPercent() < 100.0);
		SequentialDownloadMenuItem().IsChecked(
			detail.has_value() && detail->isSequential);
		OpenTaskFileMenuItem().IsEnabled(isBitTorrent && hasFile);
		ManualHashCheckMenuItem().IsEnabled(isBitTorrent && detail.has_value());
		SaveTorrentAsMenuItem().IsEnabled(
			isBitTorrent && detail.has_value() && !detail->files.empty());
		DeleteTaskMenuItem().IsEnabled(hasSelection);
		RenameTaskMenuItem().IsEnabled(hasSelection);
		MoveTaskMenuItem().IsEnabled(isBitTorrent && detail.has_value());
		TagsMenuItem().IsEnabled(hasSelection);
		OpenTaskLocationMenuItem().IsEnabled(
			hasSelection && !savePath.empty());
		SearchOnlineMenuItem().IsEnabled(hasSelection);
		CopyMagnetUriMenuItem().IsEnabled(
			isBitTorrent && !magnetUri.empty());
		CopyTaskHashMenuItem().IsEnabled(
			isBitTorrent && detail.has_value() && !detail->infoHash.empty());
		DiskUsageInfoMenuItem().IsEnabled(
			hasSelection && !savePath.empty());
		SendToMenuItem().IsEnabled(isBitTorrent && hasFile);
		SendToDefaultApplicationMenuItem().IsEnabled(isBitTorrent && hasFile);
		CopyTaskMenuItem().IsEnabled(hasSelection);
		PropertiesMenuItem().IsEnabled(isBitTorrent);
	}

	void TasksPage::TasksColumnAutoSizeSelectedWidth_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (HasFilteredTasks() && m_contextColumn)
		{
			AutoSizeTaskColumn(m_contextColumn);
		}
	}

	void TasksPage::TasksColumnAutoSizeAllWidth_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (HasFilteredTasks())
		{
			AutoSizeAllTaskColumns();
		}
	}

	void TasksPage::TasksColumnDisplayItemsReset_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (!m_viewModel)
		{
			return;
		}

		m_viewModel.IsColNameLoad(true);
		m_viewModel.IsColSizeLoad(true);
		m_viewModel.IsColProgressLoad(true);
		m_viewModel.IsColDownloadSizeLoad(true);
		m_viewModel.IsColUploadSizeLoad(true);
		m_viewModel.IsColumnTotalDownloadSizeLoad(true);
		m_viewModel.IsColumnTotalUploadSizeLoad(true);
		m_viewModel.IsColDLRateLoad(true);
		m_viewModel.IsColULRateLoad(true);
		m_viewModel.IsColRemainingLoad(true);
		m_viewModel.IsColAddDateLoad(true);
		m_viewModel.IsColCompletedDateLoad(true);
		m_viewModel.IsColShareRatioLoad(true);
		m_viewModel.IsColSeedsLoad(true);
		m_viewModel.IsColPeersLoad(true);
		for (auto const& column : std::array{
			ColName(), ColSize(), ColProgress(), ColDownloadSize(), ColUploadSize(),
			ColumnTotalDownloadSize(), ColumnTotalUploadSize(), ColDLRate(), ColULRate(),
			ColRemaining(), ColAddDate(), ColCompletedDate(), ColShareRatio(), ColSeeds(),
			ColPeers() })
		{
			column.Visibility(Visibility::Visible);
		}
		SynchronizeTaskRows();
		m_sortColumn = {};
		m_sortDirection = 0;
		UpdateTaskSortHeaders();
		SortFilteredTasks();
		AutoSizeAllTaskColumns();
	}

	void TasksPage::StartTaskMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel && m_viewModel.SelectedTask())
		{
			auto command = m_viewModel.StartCommand();
			if (command && command.CanExecute(nullptr))
			{
				command.Execute(nullptr);
			}
		}
	}

	void TasksPage::StopTaskMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel && m_viewModel.SelectedTask())
		{
			auto command = m_viewModel.PauseCommand();
			if (command && command.CanExecute(nullptr))
			{
				command.Execute(nullptr);
			}
		}
	}

	void TasksPage::PreviewTaskMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel)
		{
			OpenShellPath(GetTaskFilePath(m_viewModel.SelectedTask(), true));
		}
	}

	void TasksPage::UpdateTrackerMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto const task = m_viewModel
			? m_viewModel.SelectedTask()
			: winrt::OpenNet::ViewModels::TaskViewModel{ nullptr };
		if (IsBitTorrentTask(task))
		{
			if (auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
			{
				core->ForceReannounce(winrt::to_string(task.TaskId()));
			}
		}
	}

	void TasksPage::SuperSeedModeMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto const task = m_viewModel
			? m_viewModel.SelectedTask()
			: winrt::OpenNet::ViewModels::TaskViewModel{ nullptr };
		auto const item = sender.try_as<ToggleMenuFlyoutItem>();
		if (IsBitTorrentTask(task) && item)
		{
			if (auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
			{
				core->SetSuperSeeding(
					winrt::to_string(task.TaskId()), item.IsChecked());
			}
		}
	}

	void TasksPage::SequentialDownloadMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto const task = m_viewModel
			? m_viewModel.SelectedTask()
			: winrt::OpenNet::ViewModels::TaskViewModel{ nullptr };
		if (IsBitTorrentTask(task))
		{
			if (auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
			{
				core->ToggleSequentialDownload(winrt::to_string(task.TaskId()));
			}
		}
	}

	void TasksPage::OpenTaskFileMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel)
		{
			OpenShellPath(GetTaskFilePath(m_viewModel.SelectedTask(), false));
		}
	}

	void TasksPage::ManualHashCheckMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto const task = m_viewModel
			? m_viewModel.SelectedTask()
			: winrt::OpenNet::ViewModels::TaskViewModel{ nullptr };
		if (IsBitTorrentTask(task))
		{
			if (auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
			{
				core->ForceRecheck(winrt::to_string(task.TaskId()));
			}
		}
	}

	winrt::Windows::Foundation::IAsyncAction
		TasksPage::SaveTorrentAsMenuItem_ClickAsync(
			winrt::Windows::Foundation::IInspectable const&,
			winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		auto const task = m_viewModel
			? m_viewModel.SelectedTask()
			: winrt::OpenNet::ViewModels::TaskViewModel{ nullptr };
		if (!IsBitTorrentTask(task))
		{
			co_return;
		}

		FileSavePicker picker(XamlRoot().ContentIslandEnvironment().AppWindowId());
		picker.SuggestedStartLocation(PickerLocationId::Downloads);
		picker.SuggestedFileName(task.Name() + L".torrent");
		picker.DefaultFileExtension(L".torrent");
		auto fileTypes = winrt::single_threaded_vector<hstring>();
		fileTypes.Append(L".torrent");
		picker.FileTypeChoices().Insert(L"BitTorrent file", fileTypes);
		auto file = co_await picker.PickSaveFileAsync();
		if (!file)
		{
			co_return;
		}

		auto const taskId = winrt::to_string(task.TaskId());
		auto const filePath = std::filesystem::path{ file.Path().c_str() };
		co_await winrt::resume_background();
		if (auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
		{
			auto const content = core->ExportTorrentFile(taskId);
			if (!content.empty())
			{
				std::ofstream output(filePath, std::ios::binary | std::ios::trunc);
				output.write(
					reinterpret_cast<char const*>(content.data()),
					static_cast<std::streamsize>(content.size()));
			}
		}
	}

	void TasksPage::DeleteTaskMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel && m_viewModel.SelectedTask())
		{
			auto command = m_viewModel.DeleteCommand();
			if (command && command.CanExecute(nullptr))
			{
				command.Execute(nullptr);
			}
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

	void TasksPage::MoveTaskMenuItem_Click(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*args*/)
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
		auto lifetime = get_strong();
		try
		{
			auto selectedTask = m_viewModel ? m_viewModel.SelectedTask() : nullptr;
			if (!IsBitTorrentTask(selectedTask))
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

			auto const newPath = winrt::to_string(selectedFolder.Path());
			auto const taskId = winrt::to_string(selectedTask.TaskId());
			co_await winrt::resume_background();
			if (auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
			{
				core->MoveStorage(taskId, newPath);
				if (auto* state =
					::OpenNet::Core::P2PManager::Instance().StateManager())
				{
					state->UpdateTaskSavePath(taskId, newPath);
				}
			}
		}
		catch (const std::exception& ex)
		{
			OutputDebugStringW((L"PerformMoveTaskAsync error: " + std::wstring(winrt::to_hstring(ex.what()).c_str()) + L"\n").c_str());
		}
		catch (...)
		{
			OutputDebugStringA("PerformMoveTaskAsync unknown error\n");
		}

		co_return;
	}

	void TasksPage::OpenTaskLocationMenuItem_Click(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*args*/)
	{
		auto const task = m_viewModel
			? m_viewModel.SelectedTask()
			: winrt::OpenNet::ViewModels::TaskViewModel{ nullptr };
		if (!task)
		{
			return;
		}

		try
		{
			OpenShellPath(GetTaskSavePath(task));
		}
		catch (const std::exception& ex)
		{
			OutputDebugStringW(winrt::to_hstring(ex.what()).c_str());
		}
		catch (...)
		{
			OutputDebugStringW(L"Unknown error opening task location\n");
		}
	}

	winrt::fire_and_forget TasksPage::SearchOnlineMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		auto const task = m_viewModel
			? m_viewModel.SelectedTask()
			: winrt::OpenNet::ViewModels::TaskViewModel{ nullptr };
		if (!task)
		{
			co_return;
		}
		try
		{
			auto const query =
				winrt::Windows::Foundation::Uri::EscapeComponent(task.Name());
			co_await winrt::Windows::System::Launcher::LaunchUriAsync(
				winrt::Windows::Foundation::Uri{
					L"https://www.google.com/search?q=" + query });
		}
		catch (...)
		{
		}
	}

	void TasksPage::CopyMagnetUriMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel)
		{
			CopyTextToClipboard(GetMagnetUri(m_viewModel.SelectedTask()));
		}
	}

	void TasksPage::CopyTaskNameMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel && m_viewModel.SelectedTask())
		{
			CopyTextToClipboard(m_viewModel.SelectedTask().Name());
		}
	}

	void TasksPage::CopyTaskHashMenuItem_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel)
		{
			if (auto detail = GetTorrentDetail(m_viewModel.SelectedTask()))
			{
				CopyTextToClipboard(winrt::to_hstring(detail->infoHash));
			}
		}
	}

	void TasksPage::CopyTaskPathMenuItem_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (m_viewModel)
		{
			auto const path = GetTaskSavePath(m_viewModel.SelectedTask());
			if (!path.empty())
			{
				CopyTextToClipboard(winrt::hstring{ path.c_str() });
			}
		}
	}

	winrt::Windows::Foundation::IAsyncAction TasksPage::DiskUsageInfoMenuItem_ClickAsync(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		auto const task = m_viewModel
			? m_viewModel.SelectedTask()
			: winrt::OpenNet::ViewModels::TaskViewModel{ nullptr };
		auto const path = GetTaskSavePath(task);
		if (!task || path.empty())
		{
			co_return;
		}

		ULARGE_INTEGER available{};
		ULARGE_INTEGER total{};
		ULARGE_INTEGER free{};
		if (!GetDiskFreeSpaceExW(
			path.c_str(), &available, &total, &free))
		{
			co_return;
		}

		auto const used = total.QuadPart - free.QuadPart;
		TextBlock details;
		details.IsTextSelectionEnabled(true);
		details.TextWrapping(TextWrapping::Wrap);
		details.Text(
			L"Location: " + winrt::hstring{ path.c_str() } + L"\n" +
			L"Task size: " + task.Size() + L"\n" +
			L"Used on volume: " + FormatByteCount(used) + L"\n" +
			L"Free on volume: " + FormatByteCount(free.QuadPart) + L"\n" +
			L"Total capacity: " + FormatByteCount(total.QuadPart));

		ContentDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Style(Microsoft::UI::Xaml::Application::Current().Resources().Lookup(winrt::box_value(L"DefaultContentDialogStyle")).try_as<Microsoft::UI::Xaml::Style>());
		dialog.Title(box_value(L"Disk Usage Information"));
		dialog.Content(details);
		dialog.CloseButtonText(L"Close");
		co_await dialog.ShowAsync();
	}

	winrt::Windows::Foundation::IAsyncAction TasksPage::PropertiesMenuItem_Click(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*args*/)
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
			if (!IsBitTorrentTask(task))
			{
				co_return;
			}
			auto propertiesWindow = winrt::make<
				winrt::OpenNet::UI::Xaml::View::Windows::implementation::
				TorrentCheckModalWindow>(task);
			::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::TrackWindow(
				propertiesWindow);
			propertiesWindow.Activate();
		}
		catch (const std::exception& ex)
		{
			OutputDebugStringW(winrt::to_hstring(ex.what()).c_str());
		}
		catch (...)
		{
			OutputDebugStringW(L"Unknown error showing task properties\n");
		}

		co_return;
	}

	void TasksPage::RestoreColumnWidths()
	{
		using namespace ::OpenNet::Helpers;
		RestoreColumn(ColName(), "Tasks.Name");
		RestoreColumn(ColSize(), "Tasks.Size");
		RestoreColumn(ColProgress(), "Tasks.Progress");
		RestoreColumn(ColDownloadSize(), "Tasks.DownloadSize");
		RestoreColumn(ColUploadSize(), "Tasks.UploadSize");
		RestoreColumn(ColumnTotalDownloadSize(), "Tasks.TotalDownloadSize");
		RestoreColumn(ColumnTotalUploadSize(), "Tasks.TotalUploadSize");
		RestoreColumn(ColDLRate(), "Tasks.DLRate");
		RestoreColumn(ColULRate(), "Tasks.ULRate");
		RestoreColumn(ColRemaining(), "Tasks.Remaining");
		RestoreColumn(ColAddDate(), "Tasks.AddDate");
		RestoreColumn(ColCompletedDate(), "Tasks.CompletedDate");
		RestoreColumn(ColShareRatio(), "Tasks.ShareRatio");
		RestoreColumn(ColSeeds(), "Tasks.Seeds");
		RestoreColumn(ColPeers(), "Tasks.Peers");
	}

	void TasksPage::SaveColumnWidths()
	{
		using namespace ::OpenNet::Helpers;
		SaveColumnWidth("Tasks.Name", ColName());
		SaveColumnWidth("Tasks.Size", ColSize());
		SaveColumnWidth("Tasks.Progress", ColProgress());
		SaveColumnWidth("Tasks.DownloadSize", ColDownloadSize());
		SaveColumnWidth("Tasks.UploadSize", ColUploadSize());
		SaveColumnWidth("Tasks.TotalDownloadSize", ColumnTotalDownloadSize());
		SaveColumnWidth("Tasks.TotalUploadSize", ColumnTotalUploadSize());
		SaveColumnWidth("Tasks.DLRate", ColDLRate());
		SaveColumnWidth("Tasks.ULRate", ColULRate());
		SaveColumnWidth("Tasks.Remaining", ColRemaining());
		SaveColumnWidth("Tasks.AddDate", ColAddDate());
		SaveColumnWidth("Tasks.CompletedDate", ColCompletedDate());
		SaveColumnWidth("Tasks.ShareRatio", ColShareRatio());
		SaveColumnWidth("Tasks.Seeds", ColSeeds());
		SaveColumnWidth("Tasks.Peers", ColPeers());
	}

	void TasksPage::AutoSizeTaskColumn(winrt::XamlToolkit::Labs::WinUI::DataColumn const& column)
	{
		if (!column)
		{
			return;
		}

		column.DesiredWidth(GridLengthHelper::Auto());
		column.InvalidateMeasure();
		TasksList().InvalidateMeasure();
	}

	void TasksPage::AutoSizeAllTaskColumns()
	{
		std::array<winrt::XamlToolkit::Labs::WinUI::DataColumn, 15> const columns
		{
			ColName(),
			ColSize(),
			ColProgress(),
			ColDownloadSize(),
			ColUploadSize(),
			ColumnTotalDownloadSize(),
			ColumnTotalUploadSize(),
			ColDLRate(),
			ColULRate(),
			ColRemaining(),
			ColAddDate(),
			ColCompletedDate(),
			ColShareRatio(),
			ColSeeds(),
			ColPeers()
		};

		for (auto const& column : columns)
		{
			AutoSizeTaskColumn(column);
		}
	}

	bool TasksPage::HasFilteredTasks()
	{
		if (!m_viewModel)
		{
			return false;
		}

		auto const items = m_viewModel.FilteredTasks();
		return items && items.Size() != 0;
	}

	TasksPage::PersistedScrollState& TasksPage::ScrollStateFor(hstring const& filterKey)
	{
		return s_persistedScrollStates[std::wstring{ filterKey.c_str() }];
	}

	void TasksPage::CancelScrollRestore()
	{
		ClearRestoredItemContainerHeight();
		++m_scrollRestoreGeneration;
		m_isRestoringScrollPosition = false;
		m_restoringFilterKey = {};
	}

	void TasksPage::ClearRestoredItemContainerHeight()
	{
		if (m_restoringFilterKey.empty() || !m_viewModel)
		{
			return;
		}

		auto const stateIt = s_persistedScrollStates.find(std::wstring{ m_restoringFilterKey.c_str() });
		if (stateIt == s_persistedScrollStates.end() || stateIt->second.itemKey.empty())
		{
			return;
		}

		auto const items = m_viewModel.FilteredTasks();
		if (!items)
		{
			return;
		}

		auto const found = std::find_if(items.begin(), items.end(), [&](auto const& item)
		{
			return GetTaskPersistenceKey(item) == stateIt->second.itemKey;
		});
		if (found != items.end())
		{
			if (auto container = TasksList().ContainerFromItem(*found).try_as<ListViewItem>())
			{
				container.ClearValue(FrameworkElement::HeightProperty());
			}
		}
	}

	void TasksPage::SaveScrollPosition(hstring const& filterKey)
	{
		if (filterKey.empty() || !TasksList())
		{
			return;
		}

		CancelScrollRestore();
		auto& state = ScrollStateFor(filterKey);
		state = {};

		m_savingFilterKey = filterKey;
		try
		{
			state.position = ListViewPersistenceHelper::GetRelativeScrollPosition(
				TasksList(),
				{ this, &TasksPage::GetKey });
		}
		catch (winrt::hresult_error const& error)
		{
			state = {};
			OutputDebugStringW((L"Failed to save task list scroll position: " + std::wstring{ error.message().c_str() } + L"\n").c_str());
		}
		m_savingFilterKey = {};
	}

	winrt::fire_and_forget TasksPage::RestoreScrollPositionAsync(hstring filterKey)
	{
		auto strong = get_strong();
		auto const generation = ++m_scrollRestoreGeneration;

		if (filterKey.empty() || filterKey != m_currentFilterKey)
		{
			co_return;
		}

		auto const stateIt = s_persistedScrollStates.find(std::wstring{ filterKey.c_str() });
		if (stateIt == s_persistedScrollStates.end() || stateIt->second.position.empty())
		{
			co_return;
		}

		m_restoringFilterKey = filterKey;
		m_isRestoringScrollPosition = true;
		auto const position = stateIt->second.position;

		try
		{
			co_await ListViewPersistenceHelper::SetRelativeScrollPositionAsync(
				TasksList(),
				position,
				{ this, &TasksPage::GetItem });
		}
		catch (winrt::hresult_error const& error)
		{
			OutputDebugStringW((L"Failed to restore task list scroll position: " + std::wstring{ error.message().c_str() } + L"\n").c_str());
		}

		if (generation == m_scrollRestoreGeneration)
		{
			ClearRestoredItemContainerHeight();
			m_isRestoringScrollPosition = false;
			m_restoringFilterKey = {};
		}
	}

	winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::IInspectable> TasksPage::GetItem(hstring const& key)
	{
		if (!m_viewModel || key.empty())
		{
			co_return nullptr;
		}

		auto items = m_viewModel.FilteredTasks();
		if (!items)
		{
			co_return nullptr;
		}

		auto found = std::find_if(items.begin(), items.end(), [&](auto&& item)
		{
			return GetTaskPersistenceKey(item) == key;
		});
		co_return found == items.end() ? nullptr : *found;
	}

	hstring TasksPage::GetKey(IInspectable const& object)
	{
		if (m_savingFilterKey.empty())
		{
			return {};
		}

		auto item = object.try_as<winrt::OpenNet::ViewModels::TaskViewModel>();
		if (item)
		{
			auto& state = ScrollStateFor(m_savingFilterKey);
			state.itemKey = GetTaskPersistenceKey(item);
			if (state.itemKey.empty())
			{
				return {};
			}

			if (auto container = TasksList().ContainerFromItem(item).try_as<ListViewItem>())
			{
				state.itemContainerHeight = container.ActualHeight();
			}
			else
			{
				state.itemContainerHeight = -1.0;
			}

			return state.itemKey;
		}

		return {};
	}
}
