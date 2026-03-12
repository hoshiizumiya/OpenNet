#include "pch.h"
#include "TorrentCheckModalWindow.xaml.h"
#if __has_include("UI/Xaml/View/Windows/TorrentCheckModalWindow.g.cpp")
#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <windowsx.h>
#include <winuser.h>
#include "Core/IO/FileSystem.h"
#include "Helpers/WindowHelper.h"
#include "Helpers/ThemeHelper.h"
#include "App.xaml.h"
#include "../Pages/TorrentCheckGeneralPage.xaml.h"
#include "Core/P2PManager.h"
#include "Core/Utils/Misc.h"
#include "Core/torrentCore/TorrentMetadataFetcher.h"
#include "ViewModels/TorrentMetadataViewModel.h"
#include <algorithm>

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

	void TorrentCheckModalWindow::InitializeWindow()
	{
		// AppWindow().Resize(winrt::Windows::Graphics::SizeInt32(1500, 1800));
		::OpenNet::Helpers::WinUIWindowHelper::PlacementRestoration::Enable(*this);

		SetTitleBar(TorrentCheckModalWindowTitleBar());
		ExtendsContentIntoTitleBar(true);
		AppWindow().TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);
		SetWindowOwner();

		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(*this);

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
	}

	void TorrentCheckModalWindow::StartParseMetadata()
	{
		if (m_torrentLink.empty())
		{
			return;
		}

		// Fire and forget - the async operation manages its own lifetime
		ParseTorrentMetadataAsync().Completed([](auto const&, auto const&) {});
	}

	IAsyncAction TorrentCheckModalWindow::ParseTorrentMetadataAsync()
	{
		auto lifetime = get_strong();
		auto dispatcherQueue = DispatcherQueue();

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
			// Fetch metadata using callback-based API (60 second timeout)
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
					ShowError(msg);
				});
			},
				60  // timeout seconds
			);
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

		// Update UI elements
		if (auto loadingPanel = LoadingPanel())
		{
			loadingPanel.Visibility(isLoading ? Visibility::Visible : Visibility::Collapsed);
		}
		if (auto loadingText = LoadingStatusText())
		{
			loadingText.Text(status);
		}
		if (auto progressRing = LoadingProgressRing())
		{
			progressRing.IsIndeterminate(progress < 0);
			if (progress >= 0)
			{
				progressRing.Value(static_cast<double>(progress));
			}
		}
		if (auto errorPanel = ErrorPanel())
		{
			errorPanel.Visibility(Visibility::Collapsed);
		}
		if (auto contentPanel = ContentPanel())
		{
			contentPanel.Visibility(isLoading ? Visibility::Collapsed : Visibility::Visible);
		}
	}

	void TorrentCheckModalWindow::ShowError(winrt::hstring const& message)
	{
		m_isLoading = false;
		m_hasError = true;
		m_errorMessage = message;
		m_metadataReady = false;

		if (auto loadingPanel = LoadingPanel())
		{
			loadingPanel.Visibility(Visibility::Collapsed);
		}
		if (auto errorPanel = ErrorPanel())
		{
			errorPanel.Visibility(Visibility::Visible);
		}
		if (auto errorText = ErrorMessageText())
		{
			errorText.Text(message);
		}
		if (auto contentPanel = ContentPanel())
		{
			contentPanel.Visibility(Visibility::Collapsed);
		}

		OutputDebugStringW((L"TorrentCheckModalWindow Error: " + message + L"\n").c_str());
	}

	void TorrentCheckModalWindow::ShowMetadata(::OpenNet::Core::Torrent::TorrentMetadataInfo const& metadata)
	{
		m_isLoading = false;
		m_hasError = false;
		m_metadataReady = true;

		// Create ViewModel from metadata
		m_metadataViewModel = winrt::make<winrt::OpenNet::ViewModels::implementation::TorrentMetadataViewModel>(metadata);

		// Set default save path
		auto defaultPath = winrt::OpenNet::Core::IO::FileSystem::GetDownloadsPathW();
		if (!defaultPath.empty())
		{
			m_metadataViewModel.SavePath(winrt::hstring(defaultPath));
		}

		// Update UI
		if (auto loadingPanel = LoadingPanel())
		{
			loadingPanel.Visibility(Visibility::Collapsed);
		}
		if (auto errorPanel = ErrorPanel())
		{
			errorPanel.Visibility(Visibility::Collapsed);
		}
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

	IAsyncAction TorrentCheckModalWindow::StartDownloadAsync()
	{
		auto lifetime = get_strong();

		if (!m_metadataViewModel)
		{
			co_return;
		}

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

			// Determine if it's a magnet link or a torrent file
			if (::OpenNet::Core::Torrent::TorrentMetadataFetcher::IsMagnetLink(torrentSource))
			{
				// It's a magnet link
				success = co_await p2pManager.AddMagnetAsync(torrentSource, savePath, filePriorities);
			}
			else if (::OpenNet::Core::Torrent::TorrentMetadataFetcher::IsTorrentFile(torrentSource))
			{
				// It's a torrent file
				success = co_await p2pManager.AddTorrentFileAsync(torrentSource, savePath, filePriorities);
			}
			else
			{
				DispatcherQueue().TryEnqueue([this]()
				{
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
					ShowError(L"Failed to start download");
				});
			}
		}
		catch (std::exception const& ex)
		{
			DispatcherQueue().TryEnqueue([this, msg = winrt::to_hstring(ex.what())]()
			{
				ShowError(L"Error starting download: " + msg);
			});
		}
	}

	void TorrentCheckModalWindow::StartDownloadButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		StartDownloadAsync().Completed([](auto const&, auto const&) {});
	}

	void TorrentCheckModalWindow::CancelButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		// Safely cancel any ongoing operations
		if (m_metadataFetcher)
		{
			m_metadataFetcher->Cancel();
			// Don't access m_metadataFetcher after Cancel() to avoid use-after-free
		}
		this->Close();
	}

	void TorrentCheckModalWindow::RetryButton_Click(
		winrt::Windows::Foundation::IInspectable const&,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		StartParseMetadata();
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
		// Cancel any ongoing operations and clear the fetcher
		if (m_metadataFetcher)
		{
			m_metadataFetcher->Cancel();
			// Clear the unique_ptr to ensure proper cleanup
			m_metadataFetcher.reset();
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

		// Navigate to the appropriate page based on selected tab
		// 0: General, 1: Snapshots, 2: Advanced, 3: Publisher, 4: Download Order
		if (auto frame = TorrentCheckFrame())
		{
			switch (selectedIndex)
			{
				case 0:
					frame.Navigate(
						winrt::xaml_typename<winrt::OpenNet::UI::Xaml::View::Pages::TorrentCheckGeneralPage>(),
						m_metadataViewModel);
					break;
					// TODO: Add other pages when implemented
				default:
					break;
			}
		}
	}

	void TorrentCheckModalWindow::TorrentCreateGrid_Loaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
	{
		StartParseMetadata();
	}

	void TorrentCheckModalWindow::RootGridXamlRoot_Changed(winrt::Microsoft::UI::Xaml::XamlRoot, winrt::Microsoft::UI::Xaml::XamlRootChangedEventArgs)
	{
	}
}
