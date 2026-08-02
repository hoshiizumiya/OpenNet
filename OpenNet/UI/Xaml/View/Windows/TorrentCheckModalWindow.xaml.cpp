#include <Windows.h>

//warning C4251
#include "XamlWorkaround.h"
import winrt.XamlToolkit.Labs.WinUI;
#include "TorrentCheckModalWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/TorrentCheckModalWindow.g.cpp")
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.g.cpp"
#endif

#include "ViewModels/TorrentMetadataViewModel.h"
#include "../Pages/TorrentCheckGeneralPage.xaml.h"

import OpenNet.App;
import Core.Utils.Misc;
import OpenNet.Core.AppSettingsDatabase;
import OpenNet.Core.IO.FileSystem;
import OpenNet.Core.P2PManager;
import OpenNet.Core.Torrent.TrackerManager;
import OpenNet.Helpers.ThemeHelper;
import OpenNet.Helpers.WindowHelper;
import winrt.Windows.Graphics;
import winrtplus.Microsoft.UI.Interop;
import winrt.Microsoft.UI.Windowing;
import winrt.Microsoft.UI.Xaml.Media.Animation;

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;
using namespace winrt::Microsoft::UI::Windowing;
using namespace winrt::OpenNet::UI::Xaml::View::Pages;
using namespace winrt::Windows::Foundation;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	TorrentCheckModalWindow::TorrentCheckModalWindow()
	{
		InitializeComponent();
		InitializeWindow();
	}

	TorrentCheckModalWindow::TorrentCheckModalWindow(winrt::hstring const& torrentLink)
		: m_torrentLink(torrentLink)
	{
		InitializeComponent();
		InitializeWindow();
	}

	TorrentCheckModalWindow::TorrentCheckModalWindow(
		winrt::OpenNet::ViewModels::TaskViewModel const& task)
		: m_existingTaskMode(true)
	{
		InitializeComponent();
		InitializeWindow();
		LoadExistingTask(task);
	}

	void TorrentCheckModalWindow::InitializeWindow()
	{
		// AppWindow().Resize(winrt::Windows::Graphics::SizeInt32(1500, 1800));
		::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Enable(*this);

		SetTitleBar(TorrentCheckModalWindowTitleBar());
		ExtendsContentIntoTitleBar(true);
		AppWindow().TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);
		SetWindowOwner();

		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(*this);
		::OpenNet::Helpers::ThemeHelper::ApplyWindowAppearanceFromSettings(*this);

		if (auto presenter = winrt::Microsoft::UI::Windowing::OverlappedPresenter::CreateForDialog())
		{
			presenter.IsModal(true);
			presenter.IsResizable(true);
			presenter.IsMaximizable(true);
			AppWindow().SetPresenter(presenter);
		}
		AppWindow().Show();

		Closed({ this, &TorrentCheckModalWindow::ModalWindow_Closed });

		// Initialize metadata fetcher
		m_metadataFetcher = std::make_unique<::OpenNet::Core::Torrent::TorrentMetadataFetcher>();

		// The general page is available immediately. Magnet metadata fills in
		// the content section later, but save-path and task settings remain
		// usable from the moment the window opens.
		m_metadataViewModel =
			winrt::make<winrt::OpenNet::ViewModels::implementation::
			TorrentMetadataViewModel>();
		m_metadataViewModel.MetadataState(L"Loading");
		m_metadataViewModel.MetadataStatus(L"Connecting to peers...");
		m_metadataViewModel.TorrentName(L"Magnet download");
		auto const defaultPath =
			winrt::OpenNet::Core::IO::FileSystem::GetDownloadsPathW().GetResults();
		if (!defaultPath.empty())
		{
			m_metadataViewModel.SavePath(defaultPath);
		}

		auto& settingsDb = ::OpenNet::Core::AppSettingsDatabase::Instance();
		settingsDb.Initialize();
		SaveTorrentCopyCheckBox().IsChecked(settingsDb.GetBool(
			::OpenNet::Core::AppSettingsDatabase::CAT_TORRENT,
			"saveTorrentCopyToDownloadDirectory").value_or(false));
	}

	void TorrentCheckModalWindow::LoadExistingTask(
		winrt::OpenNet::ViewModels::TaskViewModel const& task)
	{
		if (!task || task.TaskType() !=
			winrt::OpenNet::ViewModels::DownloadTaskType::BitTorrent)
		{
			ShowError(L"The selected item is not a BitTorrent task.");
			return;
		}

		TorrentCheckModalWindowTitleBar().Title(task.Name());
		TorrentCheckModalWindowTitleBar().Subtitle(
			L"Torrent properties · " + task.Progress() +
			L" · ↓ " + task.DownloadRate() + L" · ↑ " + task.UploadRate());
		DownloadOptionsPanel().Visibility(Visibility::Collapsed);
		StartDownloadButton().Visibility(Visibility::Collapsed);
		CloseButton().Content(box_value(L"Close"));
		TrackerListTextBox().IsReadOnly(true);

		auto* core = ::OpenNet::Core::P2PManager::Instance().TorrentCore();
		if (!core)
		{
			ShowError(L"The BitTorrent engine is not available.");
			return;
		}

		auto const taskId = winrt::to_string(task.TaskId());
		auto const detail = core->GetTorrentDetail(taskId);
		if (detail.name.empty() && detail.infoHash.empty() && detail.files.empty())
		{
			ShowError(L"Torrent metadata is not available for this task.");
			return;
		}

		::OpenNet::Core::Torrent::TorrentMetadataInfo metadata;
		metadata.infoHash = detail.infoHash;
		metadata.name = detail.name.empty()
			? winrt::to_string(task.Name())
			: detail.name;
		metadata.comment = detail.comment;
		metadata.creator = detail.creator;
		metadata.totalSize = detail.totalSize;
		metadata.creationDate = detail.creationTimestamp;
		metadata.pieceLength = detail.pieceSize;
		metadata.numPieces = detail.piecesNum;
		metadata.isPrivate = detail.isPrivate;

		metadata.files.reserve(detail.files.size());
		for (auto const& source : detail.files)
		{
			::OpenNet::Core::Torrent::TorrentFileInfo file;
			file.path = source.path;
			file.size = source.size;
			file.priority = source.priority;
			file.selected = source.priority > 0;
			file.fileIndex = source.fileIndex;
			metadata.files.push_back(std::move(file));
		}

		metadata.trackers.reserve(detail.trackers.size());
		for (auto const& tracker : detail.trackers)
		{
			if (!tracker.url.empty())
			{
				metadata.trackers.push_back(tracker.url);
			}
		}
		ShowMetadata(metadata);
		m_metadataViewModel.SavePath(winrt::to_hstring(detail.savePath));
		m_metadataViewModel.MetadataStatus(L"Current task metadata");
		NavigateToGeneralPage();
	}

	void TorrentCheckModalWindow::StartParseMetadata()
	{
		if (m_torrentLink.empty())
		{
			return;
		}

		// Fire and forget - the async operation manages its own lifetime
		ParseTorrentMetadataAsync().Completed([](auto const&, auto const&)
		{});
	}

	IAsyncAction TorrentCheckModalWindow::ParseTorrentMetadataAsync()
	{
		auto lifetime = get_strong();
		auto dispatcherQueue = DispatcherQueue();

		try
		{
			auto& p2pManager = ::OpenNet::Core::P2PManager::Instance();
			co_await p2pManager.EnsureTorrentCoreInitializedAsync();
			if (auto core = p2pManager.TorrentCore())
			{
				m_metadataFetcher->UseSharedSession(core->NativeSession());
			}

			auto& trackerManager =
				::OpenNet::Core::Torrent::TrackerManager::Instance();
			co_await trackerManager.InitializeAsync();
			auto enabled = trackerManager.GetEnabledTrackers();
			dispatcherQueue.TryEnqueue([this, enabled = std::move(enabled)]()
			{
				std::vector<std::string> urls;
				urls.reserve(enabled.size());
				for (auto const& tracker : enabled)
				{
					urls.push_back(winrt::to_string(
						winrt::hstring{ tracker.url }));
				}
				MergeTrackerList(urls);
			});
		}
		catch (...)
		{
			// Custom trackers improve discovery but are not required for a
			// magnet or .torrent source to remain addable.
		}

		// Update UI to loading state
		dispatcherQueue.TryEnqueue([this]()
		{
			UpdateLoadingState(true, L"Initializing...", 5);
		});

		std::string torrentSource = winrt::to_string(m_torrentLink);

		// Validate the source
		if (!::OpenNet::Core::Torrent::TorrentMetadataFetcher::IsValidTorrentSource(torrentSource))
		{
			dispatcherQueue.TryEnqueue([this]()
			{
				ShowError(L"Invalid torrent link or file path");
			});
			co_return;
		}

		// Set progress callback
		m_metadataFetcher->SetProgressCallback([this, dispatcherQueue](const std::string& status, int progress)
		{
			dispatcherQueue.TryEnqueue([this, status = winrt::to_hstring(status), progress]()
			{
				UpdateLoadingState(true, status, progress);
			});
		});

		dispatcherQueue.TryEnqueue([this]()
		{
			UpdateLoadingState(true, L"Fetching torrent metadata...", 10);
		});

		try
		{
			// Wait without a deadline. The user may add the magnet task at any
			// time; otherwise metadata discovery continues until success or the
			// window is closed.
			co_await m_metadataFetcher->FetchMetadataAsync(
				torrentSource,
				// On success callback
				[this, dispatcherQueue](::OpenNet::Core::Torrent::TorrentMetadataInfo const& metadata)
			{
				dispatcherQueue.TryEnqueue([this, metadata]()
				{
					ShowMetadata(metadata);
				});
			},
				// On error callback
				[this, dispatcherQueue](std::string const& errorMsg)
			{
				dispatcherQueue.TryEnqueue([this, msg = winrt::to_hstring(errorMsg)]()
				{
					if (!m_closing)
					{
						ShowError(msg);
					}
				});
			},
				0
			);

			if (!m_metadataReady && !m_closing && m_metadataFetcher)
			{
				co_await winrt::resume_after(std::chrono::seconds(2));
				if (!m_closing)
				{
					StartParseMetadata();
				}
			}
		}
		catch (winrt::hresult_error const& ex)
		{
			dispatcherQueue.TryEnqueue([this, msg = ex.message()]()
			{
				ShowError(L"Metadata fetch error: " + msg);
			});
		}
		catch (std::exception const& ex)
		{
			dispatcherQueue.TryEnqueue([this, msg = winrt::to_hstring(ex.what())]()
			{
				ShowError(L"Metadata fetch error: " + msg);
			});
		}
		catch (...)
		{
			dispatcherQueue.TryEnqueue([this]()
			{
				ShowError(L"Unknown error during metadata fetch");
			});
		}
	}

	void TorrentCheckModalWindow::UpdateLoadingState(bool isLoading, winrt::hstring const& status, int progress)
	{
		m_isLoading = isLoading;
		m_loadingStatus = status;
		m_loadingProgress = progress;
		m_hasError = false;
		if (m_metadataViewModel && !m_metadataReady)
		{
			m_metadataViewModel.MetadataState(L"Loading");
			m_metadataViewModel.MetadataStatus(status);
		}

		if (auto contentPanel = ContentPanel())
		{
			contentPanel.Visibility(Visibility::Visible);
		}
	}

	void TorrentCheckModalWindow::ShowError(winrt::hstring const& message)
	{
		m_isLoading = false;
		m_hasError = false;
		m_errorMessage = message;
		if (m_metadataViewModel && !m_metadataReady)
		{
			m_metadataViewModel.MetadataState(L"Loading");
			m_metadataViewModel.MetadataStatus(message);
		}
		if (auto contentPanel = ContentPanel())
		{
			contentPanel.Visibility(Visibility::Visible);
		}

		OutputDebugStringW((L"TorrentCheckModalWindow Error: " + message + L"\n").c_str());
	}

	void TorrentCheckModalWindow::ShowMetadata(::OpenNet::Core::Torrent::TorrentMetadataInfo const& metadata)
	{
		m_isLoading = false;
		m_hasError = false;
		m_metadataReady = true;

		auto const previousSavePath = m_metadataViewModel
			? m_metadataViewModel.SavePath() : winrt::hstring{};
		auto const previousCreateSubfolder = m_metadataViewModel
			? m_metadataViewModel.CreateSubfolder() : true;

		m_metadataViewModel = winrt::make<
			winrt::OpenNet::ViewModels::implementation::TorrentMetadataViewModel>(
				metadata);
		m_metadataViewModel.CreateSubfolder(previousCreateSubfolder);

		// Set default save path
		if (!previousSavePath.empty())
		{
			m_metadataViewModel.SavePath(previousSavePath);
		}
		else
		{
			const std::wstring_view& defaultPath =
				winrt::OpenNet::Core::IO::FileSystem::GetDownloadsPathW()
				.GetResults();
			if (!defaultPath.empty())
			{
				m_metadataViewModel.SavePath(defaultPath);
			}
		}
		MergeTrackerList(metadata.trackers);

		// Update UI
		if (auto contentPanel = ContentPanel())
		{
			contentPanel.Visibility(Visibility::Visible);
		}

		// Navigate to the General page with the metadata
		NavigateToGeneralPage();

		OutputDebugStringW((L"Metadata ready: " + winrt::to_hstring(metadata.name) + L"\n").c_str());
	}

	void TorrentCheckModalWindow::NavigateToGeneralPage()
	{
		if (auto frame = TorrentCheckFrame())
		{
			// Navigate and pass the ViewModel as parameter
			frame.Navigate(
				winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TorrentCheckGeneralPage>(),
				m_metadataViewModel);
		}
	}

	void TorrentCheckModalWindow::MergeTrackerList(
		std::vector<std::string> const& trackers)
	{
		if (!TrackerListTextBox())
		{
			return;
		}

		auto merged = GetTaskTrackers();
		std::unordered_set<std::string> seen(merged.begin(), merged.end());
		for (auto const& tracker : trackers)
		{
			if (!tracker.empty() && seen.insert(tracker).second)
			{
				merged.push_back(tracker);
			}
		}

		std::wstring text;
		for (auto const& tracker : merged)
		{
			if (!text.empty())
			{
				text += L"\r\n";
			}
			text += winrt::to_hstring(tracker);
		}
		TrackerListTextBox().Text(text);
	}

	std::vector<std::string> TorrentCheckModalWindow::GetTaskTrackers()
	{
		std::vector<std::string> trackers;
		if (!TrackerListTextBox())
		{
			return trackers;
		}

		std::wistringstream lines{ std::wstring{
			TrackerListTextBox().Text().c_str() } };
		for (std::wstring line; std::getline(lines, line);)
		{
			auto const first = line.find_first_not_of(L" \t\r");
			if (first == std::wstring::npos)
			{
				continue;
			}
			auto const last = line.find_last_not_of(L" \t\r");
			line = line.substr(first, last - first + 1);
			auto url = winrt::to_string(winrt::hstring{ line });
			if (!url.empty())
			{
				trackers.push_back(std::move(url));
			}
		}
		return trackers;
	}

	IAsyncAction TorrentCheckModalWindow::StartDownloadAsync()
	{
		auto lifetime = get_strong();

		if (!m_metadataViewModel || m_downloadStarting)
		{
			co_return;
		}
		m_downloadStarting = true;
		StartDownloadButton().IsEnabled(false);

		try
		{
			auto& p2pManager = ::OpenNet::Core::P2PManager::Instance();
			co_await p2pManager.EnsureTorrentCoreInitializedAsync();

			std::string torrentSource = winrt::to_string(m_torrentLink);
			std::string savePath = winrt::to_string(m_metadataViewModel.SavePath());

			auto files = m_metadataViewModel.Files();
			std::vector<int> filePriorities;
			filePriorities.reserve(files.Size());
			for (uint32_t i = 0; i < files.Size(); ++i)
			{
				auto file = files.GetAt(i);
				int priority = static_cast<int>(file.Priority());
				if (priority < 1) priority = 1;
				if (priority > 7) priority = 7;
				filePriorities.push_back(file.IsSelected() ? priority : 0);
			}

			bool success = false;
			auto taskTrackers = GetTaskTrackers();
			auto const startImmediately =
				StartImmediatelyCheckBox().IsChecked().Value();
			auto const saveTorrentCopy =
				SaveTorrentCopyCheckBox().IsChecked().Value();
			auto& settingsDb = ::OpenNet::Core::AppSettingsDatabase::Instance();
			settingsDb.Initialize();
			settingsDb.SetBool(
				::OpenNet::Core::AppSettingsDatabase::CAT_TORRENT,
				"saveTorrentCopyToDownloadDirectory",
				saveTorrentCopy);

			// Determine if it's a magnet link or a torrent file
			if (::OpenNet::Core::Torrent::TorrentMetadataFetcher::IsMagnetLink(torrentSource))
			{
				// It's a magnet link
				success = co_await p2pManager.AddMagnetAsync(
					torrentSource,
					savePath,
					filePriorities,
					taskTrackers,
					startImmediately);
			}
			else if (::OpenNet::Core::Torrent::TorrentMetadataFetcher::IsTorrentFile(torrentSource))
			{
				// It's a torrent file
				success = co_await p2pManager.AddTorrentFileAsync(
					torrentSource,
					savePath,
					filePriorities,
					taskTrackers,
					startImmediately);
			}
			else
			{
				DispatcherQueue().TryEnqueue([this]()
				{
					m_downloadStarting = false;
					StartDownloadButton().IsEnabled(true);
					ShowError(L"Invalid torrent source: not a magnet link or torrent file");
				});
				co_return;
			}

			if (success)
			{
				OutputDebugStringA("Download started successfully\n");
				this->Close();
			}
			else
			{
				DispatcherQueue().TryEnqueue([this]()
				{
					m_downloadStarting = false;
					StartDownloadButton().IsEnabled(true);
					ShowError(L"Failed to start download");
				});
			}
		}
		catch (std::exception const& ex)
		{
			DispatcherQueue().TryEnqueue([this, msg = winrt::to_hstring(ex.what())]()
			{
				m_downloadStarting = false;
				StartDownloadButton().IsEnabled(true);
				ShowError(L"Error starting download: " + msg);
			});
		}
	}

	void TorrentCheckModalWindow::StartDownloadButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		StartDownloadAsync().Completed([](auto const&, auto const&)
		{});
	}

	void TorrentCheckModalWindow::CancelButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		m_closing = true;
		// Safely cancel any ongoing operations
		if (m_metadataFetcher)
		{
			m_metadataFetcher->Cancel();
			// Don't access m_metadataFetcher after Cancel() to avoid use-after-free
		}
		this->Close();
	}

	void TorrentCheckModalWindow::SetWindowOwner()
	{
		auto const& ownerWindow = winrt::OpenNet::implementation::App::window;
		if (!ownerWindow)
		{
			return;
		}

		HWND ownerHwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(ownerWindow);
		auto ownedWindowId = AppWindow().Id();
		HWND ownedHwnd = winrt::Microsoft::UI::GetWindowFromWindowId(ownedWindowId);

		if (ownerHwnd && ownedHwnd)
		{
			::SetWindowLongPtrW(ownedHwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(ownerHwnd));
		}
	}

	void TorrentCheckModalWindow::ModalWindow_Closed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::WindowEventArgs const&)
	{
		m_closing = true;
		// Cancel any ongoing operations and clear the fetcher
		if (m_metadataFetcher)
		{
			m_metadataFetcher->Cancel();
			// ParseTorrentMetadataAsync keeps this window alive until the
			// fetch loop observes cancellation. Do not destroy the fetcher
			// while that coroutine is still executing inside it.
		}

		auto const& ownerWindow = winrt::OpenNet::implementation::App::window;
		if (!ownerWindow)
		{
			return;
		}
		ownerWindow.Activate();
	}

	void TorrentCheckModalWindow::TorrentCheckModalWindowSeleterBar_SelectionChanged(
		winrt::Microsoft::UI::Xaml::Controls::SelectorBar const& sender,
		winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const&)
	{
		auto selectedItem = sender.SelectedItem();
		if (!selectedItem)
		{
			return;
		}

		auto items = sender.Items();
		uint32_t selectedIndex = 0;
		for (uint32_t i = 0; i < items.Size(); ++i)
		{
			if (items.GetAt(i) == selectedItem)
			{
				selectedIndex = i;
				break;
			}
		}

		m_selectedTabIndex = selectedIndex;

		if (selectedIndex == 0)
		{
			TrackersPanel().Visibility(Visibility::Collapsed);
			TorrentCheckFrame().Visibility(Visibility::Visible);
			if (auto frame = TorrentCheckFrame())
			{
				frame.Navigate(
					winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::
					TorrentCheckGeneralPage>(),
					m_metadataViewModel);
			}
		}
		else
		{
			TorrentCheckFrame().Visibility(Visibility::Collapsed);
			TrackersPanel().Visibility(Visibility::Visible);
		}
	}

	void TorrentCheckModalWindow::TorrentCreateGrid_Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		NavigateToGeneralPage();
		if (!m_existingTaskMode)
		{
			StartParseMetadata();
		}
	}

	void TorrentCheckModalWindow::RootGridXamlRoot_Changed(winrt::Microsoft::UI::Xaml::XamlRoot, winrt::Microsoft::UI::Xaml::XamlRootChangedEventArgs)
	{
	}
}
