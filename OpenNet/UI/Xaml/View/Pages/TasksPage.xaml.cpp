#include <shobjidl.h> // For IInitializeWithWindow
#include <shellapi.h> // For ShellExecute

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
#include "UI/Xaml/View/Pages/TaskPeersListPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskTrackersPage.xaml.h"
#include "UI/Xaml/View/Pages/TaskFilesPage.xaml.h"

import OpenNet.Core.DownloadManager;
import OpenNet.Core.HttpStateManager;
import OpenNet.Core.IO.FileSystem;
import OpenNet.Core.P2PManager;
import OpenNet.Extension.DependencyObjectExtensions;
import OpenNet.Factory.Window;
import OpenNet.Helpers.ColumnWidthHelper;
import OpenNet.Helpers.ControlLengthHelper;
import winrt.OpenNet.UI.Xaml.View.Pages.SettingsPages;
import winrt.Windows.Foundation;
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
	}

	TasksPage::TasksPage()
	{
		// Keep page cached to preserve ViewModel when navigating away
		//this->NavigationCacheMode(winrt::Microsoft::UI::Xaml::Navigation::NavigationCacheMode::Enabled);

		// Create and attach the view-model
		m_viewModel = winrt::make<winrt::OpenNet::ViewModels::implementation::TasksViewModel>();

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

	void TasksPage::ViewTasksPageSettingsAppBarButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		auto window = ::OpenNet::Factory::Window::WindowFactory::CreateStandardWindow();
		window.Content(winrt::OpenNet::UI::Xaml::View::Pages::SettingsPages::MainSettingsPage());
		window.Activate();
		return;
	}

	void TasksPage::PortTestKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
	{
	}

	void TasksPage::SettingKeyboardAccelerator_Invoked(winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender, winrt::Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
	{
		ViewTasksPageSettingsAppBarButton_Click(sender, args);
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

	void TasksPage::TasksColumnMenuFlyout_Opening(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Windows::Foundation::IInspectable const&)
	{
		auto const hasItems = HasFilteredTasks();
		ViewPageTasksColumnAutoSizeSelectedWidth().IsEnabled(hasItems && m_contextColumn);
		ViewPageTasksColumnAutoSizeAllWidth().IsEnabled(hasItems);
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
		auto const hasSelection = m_viewModel && m_viewModel.SelectedTask();
		RenameTaskMenuItem().IsEnabled(hasSelection);
		// The current picker-only implementation does not move payload data or
		// update either download backend's persisted save path. Keep it
		// unavailable instead of presenting a command that reports false success.
		MoveTaskMenuItem().IsEnabled(false);
		OpenTaskLocationMenuItem().IsEnabled(hasSelection);
		PropertiesMenuItem().IsEnabled(hasSelection);
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
		AutoSizeAllTaskColumns();
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
		if (!m_viewModel || !m_viewModel.SelectedTask())
		{
			return;
		}

		try
		{
			auto selectedTask = m_viewModel.SelectedTask();
			std::wstring taskPath;

			// 根据任务类型获取下载路径
			auto taskType = selectedTask.TaskType();
			auto taskId = winrt::to_string(selectedTask.TaskId());

			if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
			{
				// BT任务：从 P2PManager 的 StateManager 获取元数据
				auto& p2pMgr = ::OpenNet::Core::P2PManager::Instance();
				auto stateMgr = p2pMgr.StateManager();
				if (stateMgr)
				{
					auto metadata = stateMgr->LoadTaskMetadata(taskId);
					if (metadata && !metadata->savePath.empty())
					{
						// 将 std::string 转换为 std::wstring
						int size = MultiByteToWideChar(CP_UTF8, 0, metadata->savePath.c_str(), -1, nullptr, 0);
						if (size > 0)
						{
							taskPath.resize(size - 1);
							MultiByteToWideChar(CP_UTF8, 0, metadata->savePath.c_str(), -1, &taskPath[0], size);
						}
					}
				}
			}
			else if (taskType == winrt::OpenNet::ViewModels::DownloadTaskType::Http)
			{
				// HTTP任务：从 HttpStateManager 获取记录
				auto& httpMgr = ::OpenNet::Core::HttpStateManager::Instance();
				auto record = httpMgr.FindByRecordId(taskId);
				if (record && !record->savePath.empty())
				{
					// 将 std::string 转换为 std::wstring
					int size = MultiByteToWideChar(CP_UTF8, 0, record->savePath.c_str(), -1, nullptr, 0);
					if (size > 0)
					{
						taskPath.resize(size - 1);
						MultiByteToWideChar(CP_UTF8, 0, record->savePath.c_str(), -1, &taskPath[0], size);
					}
				}
			}

			// 如果未能获取路径，使用 AppData 作为后备
			if (taskPath.empty())
			{
				OutputDebugStringW(L"Failed to get task path, using AppData as fallback\n");
				taskPath = ::winrt::OpenNet::Core::IO::FileSystem::GetAppDataPathW();
			}

			// 验证路径存在
			if (!::winrt::OpenNet::Core::IO::FileSystem::DirectoryExists(taskPath))
			{
				OutputDebugStringW(L"Task path does not exist\n");
				return;
			}

			// 打开文件浏览器
			HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", taskPath.c_str(), nullptr, SW_SHOW);

			if ((intptr_t)result <= 32)
			{
				OutputDebugStringW(L"Failed to open file explorer\n");
			}
			else
			{
				OutputDebugStringW((L"Opened location: " + taskPath + L"\n").c_str());
			}
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
			if (!task)
			{
				co_return;
			}

			auto const taskType = task.TaskType() == winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent
				? L"BitTorrent"
				: L"HTTP";

			TextBlock details;
			details.IsTextSelectionEnabled(true);
			details.TextWrapping(TextWrapping::Wrap);
			details.Text(winrt::hstring{ std::format(
				L"Type: {}\n"
				L"Task ID: {}\n"
				L"GID: {}\n"
				L"Size: {}\n"
				L"Progress: {}\n"
				L"Downloaded: {}\n"
				L"Uploaded: {}\n"
				L"Download rate: {}\n"
				L"Upload rate: {}\n"
				L"Remaining: {}\n"
				L"Added: {}",
				taskType,
				std::wstring_view{ task.TaskId() },
				std::wstring_view{ task.Gid() },
				std::wstring_view{ task.Size() },
				std::wstring_view{ task.Progress() },
				std::wstring_view{ task.DownloadSize() },
				std::wstring_view{ task.UploadSize() },
				std::wstring_view{ task.DownloadRate() },
				std::wstring_view{ task.UploadRate() },
				std::wstring_view{ task.Remaining() },
				std::wstring_view{ task.AddDate() }) });

			ScrollViewer contentScroller;
			contentScroller.MaxHeight(480.0);
			contentScroller.Content(details);

			ContentDialog dialog;
			dialog.XamlRoot(XamlRoot());
			dialog.Title(box_value(task.Name()));
			dialog.Content(contentScroller);
			dialog.CloseButtonText(L"Close");
			co_await dialog.ShowAsync();
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
		std::array<winrt::XamlToolkit::Labs::WinUI::DataColumn, 11> const columns
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
			ColAddDate()
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
