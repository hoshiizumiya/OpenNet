#include "XamlWorkaround.h"
#include "TaskFilesPage.xaml.h"
#include <algorithm>
#include <map>
#include <shellapi.h>
#include <vector>
#if __has_include("UI/Xaml/View/Pages/TaskFilesPage.g.cpp")
#include "UI/Xaml/View/Pages/TaskFilesPage.g.cpp"
#endif

#include "ViewModels/DisplayItems.h"

import Core.Utils.Misc;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.P2PManager;
import OpenNet.Core.DownloadManager;
import OpenNet.Helpers.ColumnWidthHelper;
import OpenNet.UI.Xaml.Control.DataTableColumnVisibilityHelper;
import OpenNet.UI.Xaml.Control.DataTableSortHelper;
import OpenNet.Core.Utils.Message;
import winrt.OpenNet.UI.Xaml.View.Dialog;
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
		m_fileItems = winrt::single_threaded_observable_vector<winrt::OpenNet::ViewModels::FileDisplayItem>();
	}

	void TaskFilesPage::InitializeComponent()
	{
		TaskFilesPageT::InitializeComponent();
		UpdateSortHeaders();
		Unloaded([this](auto, auto)
		{
			m_isActive.store(false, std::memory_order_release);
			StopRefreshTimer();
			Unsubscribe();
			using namespace ::OpenNet::Helpers;
			SaveColumnWidth("Files.Size", ColFileSize());
			SaveColumnWidth("Files.Progress", ColFileProgress());
			SaveColumnWidth("Files.Done", ColFileDone());
			SaveColumnWidth("Files.Priority", ColFilePriority());
		});
	}

	TaskFilesPage::~TaskFilesPage()
	{
	}

	void TaskFilesPage::OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e)
	{
		Unsubscribe();
		m_isActive.store(true, std::memory_order_release);

		m_viewModel = e.Parameter().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		if (!m_viewModel)
		{
			m_viewModel = this->DataContext().try_as<winrt::OpenNet::ViewModels::TasksViewModel>();
		}

		if (m_viewModel)
		{
			this->DataContext(m_viewModel);
			m_vmPropertyChangedToken = m_viewModel.PropertyChanged(
				{ this, &TaskFilesPage::OnViewModelPropertyChanged });
		}

		if (!m_refreshTimer)
		{
			m_refreshTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
			auto weak = get_weak();
			m_timerTickToken = m_refreshTimer.Tick([weak](auto const& sender, auto const& args)
			{
				if (auto self = weak.get()) self->OnRefreshTimerTick(sender, args);
			});
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
		m_isActive.store(false, std::memory_order_release);
		StopRefreshTimer();
		Unsubscribe();
	}

	void TaskFilesPage::StopRefreshTimer() noexcept
	{
		if (!m_refreshTimer) return;
		try
		{
			m_refreshTimer.Stop();
			if (m_timerTickToken.value) m_refreshTimer.Tick(m_timerTickToken);
		}
		catch (...)
		{
		}
		m_timerTickToken = {};
		m_refreshTimer = nullptr;
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

	void TaskFilesPage::OnViewModelPropertyChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& args)
	{
		if (args.PropertyName() == L"SelectedTask")
		{
			RefreshFileList();
		}
	}

	void TaskFilesPage::OnRefreshTimerTick(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::Foundation::IInspectable const&)
	{
		if (!m_isActive.load(std::memory_order_acquire)) return;
		RefreshFileList();
	}

	void TaskFilesPage::ColumnHeader_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
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
			::OpenNet::UI::Xaml::Control::DataTableSortHelper::UpdateHeader(button, m_sortColumn, m_sortDirection);
		};
		update(SortFileNameButton());
		update(SortFileSizeButton());
		update(SortFileProgressButton());
		update(SortFileDoneButton());
		update(SortFilePriorityButton());
	}

	void TaskFilesPage::ColumnHeader_RightTapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args)
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

	void TaskFilesPage::ColumnMenu_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const&)
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

	winrt::XamlToolkit::Labs::WinUI::DataColumn TaskFilesPage::ColumnForTag(winrt::hstring const& tag)
	{
		if (tag == L"Path") return ColFileName();
		if (tag == L"Size") return ColFileSize();
		if (tag == L"Progress") return ColFileProgress();
		if (tag == L"Done") return ColFileDone();
		if (tag == L"Priority") return ColFilePriority();
		return nullptr;
	}

	void TaskFilesPage::ColumnVisibility_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto toggle = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>();
		if (!toggle || !toggle.Tag()) return;
		if (auto column = ColumnForTag(winrt::unbox_value<winrt::hstring>(toggle.Tag())))
		{
			column.Visibility(toggle.IsChecked() ? Visibility::Visible : Visibility::Collapsed);
			SynchronizeFileRows();
		}
	}

	void TaskFilesPage::AutoSizeColumn(winrt::XamlToolkit::Labs::WinUI::DataColumn const& column)
	{
		if (!column) return;
		column.DesiredWidth(GridLengthHelper::Auto());
		column.InvalidateMeasure();
		FilesListView().InvalidateMeasure();
	}

	void TaskFilesPage::AutoSizeSelectedColumn_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		AutoSizeColumn(m_contextColumn);
	}

	void TaskFilesPage::AutoSizeAllColumns_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		for (auto const& column : std::array{
			ColFileName(), ColFileSize(), ColFileProgress(), ColFileDone(), ColFilePriority() })
			AutoSizeColumn(column);
	}

	void TaskFilesPage::ResetColumns_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args)
	{
		m_sortColumn = {};
		m_sortDirection = 0;
		UpdateSortHeaders();
		for (auto const& column : std::array{
			ColFileName(), ColFileSize(), ColFileProgress(), ColFileDone(), ColFilePriority() })
			column.Visibility(Visibility::Visible);
		SynchronizeFileRows();
		AutoSizeAllColumns_Click(sender, args);
		RefreshFileList();
	}

	void TaskFilesPage::FileDataRow_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		std::array const columns{
			ColFileName(), ColFileSize(), ColFileProgress(), ColFileDone(), ColFilePriority() };
		::OpenNet::UI::Xaml::Control::DataTableColumnVisibilityHelper::SynchronizeRow(
			sender.try_as<winrt::XamlToolkit::Labs::WinUI::DataRow>(),
			columns.data(), static_cast<unsigned int>(columns.size()));
	}

	void TaskFilesPage::SynchronizeFileRows()
	{
		std::array const columns{
			ColFileName(), ColFileSize(), ColFileProgress(), ColFileDone(), ColFilePriority() };
		::OpenNet::UI::Xaml::Control::DataTableColumnVisibilityHelper::SynchronizeRealizedRows(
			FilesListView(), columns.data(), static_cast<unsigned int>(columns.size()));
		FilesListView().InvalidateMeasure();
	}

	void TaskFilesPage::RefreshFileList()
	{
		if (!m_isActive.load(std::memory_order_acquire)) return;
		auto const emptyText = EmptyStateText();
		auto clear = [&]
		{
			m_fileItems.Clear();
			m_selectedFile = nullptr;
			m_displayedTaskKey = {};
			if (emptyText) emptyText.Visibility(Visibility::Visible);
		};
		if (!m_viewModel || !m_viewModel.SelectedTask()) { clear(); return; }

		auto const task = m_viewModel.SelectedTask();
		bool const isTorrent = task.TaskType() == winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent;
		bool const isHttp = task.TaskType() == winrt::OpenNet::ViewModels::DownloadTaskType::Http;
		if (!isTorrent && !isHttp) { clear(); return; }
		auto const taskId = winrt::to_string(isHttp ? task.Gid() : task.TaskId());
		if (taskId.empty()) { clear(); return; }

		struct FileSnapshot
		{
			std::wstring path;
			std::uint64_t size{};
			std::uint64_t done{};
			int priority{};
			int index{};
		};
		std::vector<FileSnapshot> files;
		if (isHttp)
		{
			auto const information = ::OpenNet::Core::DownloadManager::Instance().GetHttpTaskInformation(taskId);
			if (!information || information->Files.empty()) { clear(); return; }
			for (auto const& file : information->Files)
				files.push_back({ std::wstring{ winrt::to_hstring(file.Path).c_str() }, file.Length, file.CompletedLength, file.Selected ? 1 : 0, static_cast<int>(file.Index) });
		}
		else
		{
			auto& p2p = ::OpenNet::Core::P2PManager::Instance();
			if (!p2p.IsTorrentCoreInitialized() || !p2p.TorrentCore()) { clear(); return; }
			auto const detail = p2p.TorrentCore()->GetTorrentFilesSnapshot(taskId);
			for (auto const& file : detail.files)
				if (!file.isPadFile)
					files.push_back({ std::wstring{ winrt::to_hstring(file.path).c_str() }, static_cast<std::uint64_t>(file.size), static_cast<std::uint64_t>(file.bytesCompleted), file.priority, file.fileIndex });
		}
		if (files.empty()) { clear(); return; }

		// Reset the tree when the task changes; stable path keys retain expansion during refresh.
		auto const displayKey = winrt::to_hstring(isHttp ? "http:" + taskId : "torrent:" + taskId);
		if (m_displayedTaskKey != displayKey)
		{
			m_fileItems.Clear();
			m_selectedFile = nullptr;
			m_displayedTaskKey = displayKey;
		}
		m_isRefreshing = true;
		using FileItem = winrt::OpenNet::ViewModels::FileDisplayItem;
		struct Node
		{
			FileItem item{ nullptr };
			std::vector<std::wstring> children;
			std::uint64_t size{};
			std::uint64_t done{};
			int priority{};
		};
		std::map<std::wstring, FileItem> previous;
		auto collect = [&](auto&& self, FileItem const& item) -> void
		{
			previous.emplace(std::wstring{ item.IsFolder() ? L"D:" : L"F:" } + std::wstring{ item.Path().c_str() }, item);
			for (auto const& child : item.Children()) self(self, child);
		};
		for (auto const& item : m_fileItems) collect(collect, item);
		std::map<std::wstring, Node> nodes;
		std::vector<std::wstring> roots;
		auto addNode = [&](std::wstring const& key, std::wstring const& path, std::wstring const& name, bool folder, std::wstring const& parent) -> Node&
		{
			auto [it, inserted] = nodes.try_emplace(key);
			if (inserted)
			{
				auto old = previous.find(key);
				it->second.item = old == previous.end() ? winrt::make<winrt::OpenNet::ViewModels::implementation::FileDisplayItem>() : old->second;
				it->second.item.Path(winrt::hstring{ path });
				it->second.item.Name(winrt::hstring{ name });
				it->second.item.IsFolder(folder);
				if (parent.empty()) roots.push_back(key);
				else nodes.at(parent).children.push_back(key);
			}
			return it->second;
		};
		for (auto const& file : files)
		{
			std::wstring path = file.path;
			std::replace(path.begin(), path.end(), L'\\', L'/');
			auto const nameAt = path.find_last_of(L'/');
			auto const name = nameAt == std::wstring::npos ? path : path.substr(nameAt + 1);
			std::wstring parent;
			if (isTorrent)
			{
				std::wstring folderPath;
				std::size_t start = 0;
				while (path.find(L'/', start) != std::wstring::npos)
				{
					auto const slash = path.find(L'/', start);
					auto const part = path.substr(start, slash - start);
					if (!part.empty())
					{
						folderPath += (folderPath.empty() ? L"" : L"/") + part;
						auto const key = L"D:" + folderPath;
						addNode(key, folderPath, part, true, parent);
						parent = key;
					}
					start = slash + 1;
				}
			}
			auto& leaf = addNode(L"F:" + path, isTorrent ? path : file.path, name, false, parent);
			leaf.size = file.size;
			leaf.done = file.done;
			leaf.priority = file.priority;
			leaf.item.FileIndex(file.index);
			leaf.item.PriorityIndex(isHttp ? file.priority : PriorityToComboIndex(file.priority));
			leaf.item.IsPriorityEditable(isTorrent);
		}
		// Children are sorted within their own folder, keeping the tree hierarchy intact.
		auto update = [&](auto&& self, std::wstring const& key) -> void
		{
			auto& node = nodes.at(key);
			for (auto const& child : node.children)
			{
				self(self, child);
				node.size += nodes.at(child).size;
				node.done += nodes.at(child).done;
			}
			node.item.Size(::Core::Utils::Misc::friendlyUnit(node.size));
			node.item.Done(::Core::Utils::Misc::friendlyUnit(node.done));
			node.item.ProgressValue(node.size ? static_cast<double>(node.done) * 100.0 / node.size : 0.0);
		};
		for (auto const& key : roots) update(update, key);
		auto sort = [&](std::vector<std::wstring>& keys)
		{
			if (!m_sortDirection) return;
			std::stable_sort(keys.begin(), keys.end(), [&](auto const& a, auto const& b)
			{
				auto const& left = nodes.at(a);
				auto const& right = nodes.at(b);
				if (left.item.IsFolder() != right.item.IsFolder()) return left.item.IsFolder();
				auto compare = [&]() -> int
				{
					if (m_sortColumn == L"Size" && left.size != right.size) return left.size < right.size ? -1 : 1;
					if (m_sortColumn == L"Done" && left.done != right.done) return left.done < right.done ? -1 : 1;
					if (m_sortColumn == L"Progress" && left.item.ProgressValue() != right.item.ProgressValue()) return left.item.ProgressValue() < right.item.ProgressValue() ? -1 : 1;
					if (m_sortColumn == L"Priority" && left.priority != right.priority) return left.priority < right.priority ? -1 : 1;
					return left.item.Name() < right.item.Name() ? -1 : left.item.Name() == right.item.Name() ? 0 : 1;
				};
				return m_sortDirection == 1 ? compare() < 0 : compare() > 0;
			});
		};
		auto reconcile = [&](auto const& desired, auto const& vector)
		{
			for (std::uint32_t index = 0; index < desired.size(); ++index)
			{
				auto const item = nodes.at(desired[index]).item;
				if (index < vector.Size() && vector.GetAt(index) == item) continue;
				std::uint32_t found = index;
				while (found < vector.Size() && vector.GetAt(found) != item) ++found;
				if (found < vector.Size()) vector.RemoveAt(found);
				vector.InsertAt(index, item);
			}
			while (vector.Size() > desired.size()) vector.RemoveAtEnd();
		};
		auto populate = [&](auto&& self, std::vector<std::wstring>& keys, auto const& vector) -> void
		{
			sort(keys);
			reconcile(keys, vector);
			for (auto const& key : keys)
			{
				auto& node = nodes.at(key);
				if (node.item.IsFolder()) self(self, node.children, node.item.Children());
			}
		};
		populate(populate, roots, m_fileItems);
		if (m_selectedFile && !nodes.contains(std::wstring{ m_selectedFile.IsFolder() ? L"D:" : L"F:" } + std::wstring{ m_selectedFile.Path().c_str() }))
			m_selectedFile = nullptr;
		if (emptyText) emptyText.Visibility(Visibility::Collapsed);
		m_isRefreshing = false;
	}

	std::optional<TaskFilesPage::SelectedFileContext> TaskFilesPage::GetSelectedFileContext()
	{
		if (!m_viewModel || !m_viewModel.SelectedTask()) return std::nullopt;
		auto const item = m_selectedFile;
		if (!item || item.IsFolder()) return std::nullopt;
		if (m_viewModel.SelectedTask().TaskType() == winrt::OpenNet::ViewModels::DownloadTaskType::Http)
		{
			auto const fullPath = std::filesystem::path{ item.Path().c_str() };
			return SelectedFileContext{ winrt::to_string(m_viewModel.SelectedTask().Gid()), item.FileIndex(), fullPath.filename(), fullPath };
		}

		auto const taskId = winrt::to_string(m_viewModel.SelectedTask().TaskId());
		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (!core || taskId.empty()) return std::nullopt;
		auto const detail = core->GetTorrentFilesSnapshot(taskId);
		for (auto const& file : detail.files)
		{
			if (file.fileIndex != item.FileIndex()) continue;
			auto const relativePath = std::filesystem::path{
				winrt::to_hstring(file.path).c_str() };
			return SelectedFileContext{
				taskId,
				file.fileIndex,
				relativePath,
				std::filesystem::path{ winrt::to_hstring(detail.savePath).c_str() } /
					relativePath };
		}
		return std::nullopt;
	}

	bool TaskFilesPage::LaunchSelectedFile(wchar_t const* verb)
	{
		auto const context = GetSelectedFileContext();
		if (!context) return false;
		std::error_code error;
		if (!std::filesystem::is_regular_file(context->fullPath, error) || error)
			return false;
		return reinterpret_cast<std::intptr_t>(ShellExecuteW(
			nullptr, verb, context->fullPath.c_str(), nullptr, nullptr, SW_SHOW)) > 32;
	}

	void TaskFilesPage::SelectFileFromSource(winrt::Windows::Foundation::IInspectable const& originalSource)
	{
		m_selectedFile = nullptr;
		auto source = originalSource.try_as<DependencyObject>();
		while (source && source != FilesListView())
		{
			if (auto const item = source.try_as<winrt::Microsoft::UI::Xaml::Controls::TreeViewItem>())
			{
				m_selectedFile = item.DataContext().try_as<winrt::OpenNet::ViewModels::FileDisplayItem>();
				if (!m_selectedFile) m_selectedFile = item.Content().try_as<winrt::OpenNet::ViewModels::FileDisplayItem>();
				if (!m_selectedFile)
					if (auto const content = item.Content().try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>())
						m_selectedFile = content.DataContext().try_as<winrt::OpenNet::ViewModels::FileDisplayItem>();
				break;
			}
			source = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(source);
		}
	}

	void TaskFilesPage::FilesListView_RightTapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args)
	{
		SelectFileFromSource(args.OriginalSource());
	}

	void TaskFilesPage::FilesListView_DoubleTapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& args)
	{
		SelectFileFromSource(args.OriginalSource());
		if (m_selectedFile && !m_selectedFile.IsFolder()) LaunchSelectedFile(L"open");
	}

	void TaskFilesPage::FileContextMenu_Opening(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::Foundation::IInspectable const&)
	{
		auto const context = GetSelectedFileContext();
		std::error_code error;
		auto const exists = context && std::filesystem::is_regular_file(context->fullPath, error) && !error;
		OpenFileMenuItem().IsEnabled(exists);
		PlayFileMenuItem().IsEnabled(exists);
		OpenFileLocationMenuItem().IsEnabled(context.has_value());
		RenameFileMenuItem().IsEnabled(context.has_value());
	}

	void TaskFilesPage::OpenFile_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		LaunchSelectedFile(L"open");
	}

	void TaskFilesPage::PlayFile_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		if (!LaunchSelectedFile(L"play"))
			LaunchSelectedFile(L"open");
	}

	void TaskFilesPage::OpenFileLocation_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto const context = GetSelectedFileContext();
		if (!context) return;
		std::wstring arguments = L"/select,\"" + context->fullPath.wstring() + L"\"";
		ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOW);
	}

	winrt::Windows::Foundation::IAsyncAction TaskFilesPage::RenameFile_ClickAsync(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		auto lifetime = get_strong();
		auto const context = GetSelectedFileContext();
		if (!context) co_return;

		winrt::OpenNet::UI::Xaml::View::Dialog::TextInputDialog dialog;
		dialog.XamlRoot(XamlRoot());
		dialog.Configure(ResourceGetString(L"ViewTaskFilesPageRenameFileTitle"), {}, winrt::hstring{ context->relativePath.filename().wstring() }, {}, ResourceGetString(L"CommonRename"), ResourceGetString(L"CommonCancel"), false);
		if (co_await dialog.ShowAsync() != winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
		{
			co_return;
		}

		auto const newName = std::wstring{ dialog.InputText() };
		if (newName.empty() || newName == context->relativePath.filename().wstring() ||
			newName.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos)
		{
			co_return;
		}

		auto const newRelativePath = context->relativePath.parent_path() / newName;
		if (m_viewModel.SelectedTask().TaskType() == winrt::OpenNet::ViewModels::DownloadTaskType::Http)
		{
			std::error_code error;
			std::filesystem::rename(context->fullPath, context->fullPath.parent_path() / newName, error);
			RefreshFileList();
		}
		else if (auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore())
		{
			core->RenameFile(
				context->taskId,
				context->fileIndex,
				winrt::to_string(winrt::hstring{ newRelativePath.wstring() }));
		}
	}

	void TaskFilesPage::FilePriority_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& /*args*/)
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
		auto detail = p2p.TorrentCore()->GetTorrentFilesSnapshot(taskId);
		if (detail.files.empty()) return;

		std::vector<int> priorities;
		priorities.reserve(detail.files.size());
		for (auto const& f : detail.files)
		{
			priorities.push_back(f.priority);
		}

		// Update the changed file's priority
		for (std::size_t index = 0; index < detail.files.size(); ++index)
		{
			if (detail.files[index].fileIndex != fileIndex) continue;
			auto const priority = ComboIndexToPriority(selectedIndex);
			if (PriorityToComboIndex(priorities[index]) == selectedIndex) return;
			priorities[index] = priority;
			p2p.TorrentCore()->SetFilePriorities(taskId, priorities);
			break;
		}
	}

	void TaskFilesPage::DataTable_Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		using namespace ::OpenNet::Helpers;
		RestoreColumn(ColFileSize(), "Files.Size");
		RestoreColumn(ColFileProgress(), "Files.Progress");
		RestoreColumn(ColFileDone(), "Files.Done");
		RestoreColumn(ColFilePriority(), "Files.Priority");
	}
}
