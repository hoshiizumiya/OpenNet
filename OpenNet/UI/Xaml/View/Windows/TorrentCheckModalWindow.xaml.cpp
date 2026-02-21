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
#include "Helpers/WindowHelper.h"
#include "Helpers/ThemeHelper.h"
#include "App.xaml.h"
#include "../Pages/TorrentCheckGeneralPage.xaml.h"
#include "Core/P2PManager.h"
#include "Core/Utils/Misc.h"

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Media::Animation;
using namespace winrt::Microsoft::UI::Windowing;
using namespace winrt::OpenNet::UI::Xaml::View::Pages;
using namespace winrt::Windows::Foundation;

namespace lt = libtorrent;

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

		// Don't start parsing here - wait for window to load and display
		// This is handled in TorrentCreateGrid_Loaded event handler
	}

	void TorrentCheckModalWindow::InitializeWindow()
	{
		AppWindow().Resize(winrt::Windows::Graphics::SizeInt32(1500, 1800));

		SetTitleBar(TorrentCheckModalWindowTitleBar());
		ExtendsContentIntoTitleBar(true);
		AppWindow().TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);
		// Set this modal window's owner (the main application window).
		SetWindowOwner();

		// Apply saved/custom theme to this window as well.
		::OpenNet::Helpers::ThemeHelper::UpdateThemeForWindow(*this);

		//// Make the window modal and show it. Other way: AppWindow().Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>()
		if (auto presenter = winrt::Microsoft::UI::Windowing::OverlappedPresenter::CreateForDialog())
		{
			presenter.IsModal(true);
			AppWindow().SetPresenter(presenter);
		}
		AppWindow().Show();

		Closed({ this, &TorrentCheckModalWindow::ModalWindow_Closed });
	}

	void TorrentCheckModalWindow::StartParseMetadata()
	{
		// Don't parse if already parsing or no link provided
		if (m_isParsingMetadata || m_torrentLink.empty())
		{
			return;
		}

		// Start async operation without waiting (fire and forget pattern)
		// The async operation will manage its own lifetime via the window
		ParseTorrentMetadataAsync().Completed([](auto const&, auto const&)
		{});
	}

	IAsyncAction TorrentCheckModalWindow::ParseTorrentMetadataAsync()
	{
		m_isParsingMetadata = true;

		OnMetadataParsingProgress("Initializing torrent session...");

		try
		{
			// Get the P2P Manager instance and ensure torrent core is initialized
			auto& p2pManager = ::OpenNet::Core::P2PManager::Instance();
			co_await p2pManager.EnsureTorrentCoreInitializedAsync();

			// Convert hstring to std::string for libtorrent
			std::string torrentLink = winrt::to_string(m_torrentLink);

			// Validate the link
			if (!::Core::Utils::Misc::isTorrentLink(m_torrentLink))
			{
				OnMetadataParsingFailed("Invalid torrent link format");
				m_isParsingMetadata = false;
				co_return;
			}

			OnMetadataParsingProgress("Parsing magnet URI or torrent file...");

			auto torrentCore = p2pManager.TorrentCore();
			if (!torrentCore)
			{
				OnMetadataParsingFailed("Torrent core not available");
				m_isParsingMetadata = false;
				co_return;
			}

			// Ensure the core is running
			if (!torrentCore->IsRunning())
			{
				torrentCore->Start();
				// Give the core a moment to start
				co_await winrt::resume_after(std::chrono::milliseconds(500));
			}

			OnMetadataParsingProgress("Downloading torrent metadata...");

			// Use proper temporary directory
			std::string tempSavePath = GetTempDirectory();
			if (tempSavePath.empty())
			{
				OnMetadataParsingFailed("Failed to get temporary directory");
				m_isParsingMetadata = false;
				co_return;
			}

			// Add the torrent with flags to only fetch metadata without downloading content
			bool success = torrentCore->AddMagnet(torrentLink, tempSavePath);

			if (!success)
			{
				OnMetadataParsingFailed("Failed to initiate metadata download");
				m_isParsingMetadata = false;
				co_return;
			}

			// Wait for metadata to be ready with timeout
			int maxAttempts = 120; // 60 seconds with 500ms sleep
			int attempts = 0;

			while (attempts < maxAttempts && !m_metadataReady)
			{
				// Use async delay instead of blocking sleep
				co_await winrt::resume_after(std::chrono::milliseconds(500));
				attempts++;
			}

			if (m_metadataReady)
			{
				OnMetadataParsingCompleted();
			}
			else
			{
				OnMetadataParsingFailed("Metadata download timeout");
			}
		}
		catch (const std::exception& ex)
		{
			OnMetadataParsingFailed(std::string("Error during metadata parsing: ") + ex.what());
		}
		catch (...)
		{
			OnMetadataParsingFailed("Unknown error during metadata parsing");
		}

		m_isParsingMetadata = false;
	}

	void TorrentCheckModalWindow::OnMetadataParsingProgress(const std::string& status)
	{
		// Update UI with status message in the main UI thread
		// TODO: Update ProgressRing and status TextBlock in XAML
		// Example: StatusTextBlock().Text(winrt::to_hstring(status));
		OutputDebugStringW(winrt::to_hstring(status).c_str());
	}

	std::string TorrentCheckModalWindow::GetTempDirectory()
	{
		try
		{
			// Use Windows API to get temp directory
			wchar_t tempPath[MAX_PATH];
			DWORD result = GetTempPathW(MAX_PATH, tempPath);
			if (result == 0 || result > MAX_PATH)
			{
				return {};
			}

			std::wstring tempDir(tempPath);
			// Append a subdirectory for torrent metadata
			tempDir += L"OpenNet\\TorrentMetadata";

			// Create directory if it doesn't exist
			if (!CreateDirectoryW(tempDir.c_str(), nullptr))
			{
				DWORD error = GetLastError();
				// ERROR_ALREADY_EXISTS is fine
				if (error != ERROR_ALREADY_EXISTS)
				{
					return {};
				}
			}

			// Convert wide string to regular string
			int size = WideCharToMultiByte(CP_UTF8, 0, tempDir.c_str(), -1, nullptr, 0, nullptr, nullptr);
			if (size <= 0)
			{
				return {};
			}

			std::string result_str(size - 1, 0);
			WideCharToMultiByte(CP_UTF8, 0, tempDir.c_str(), -1, &result_str[0], size, nullptr, nullptr);
			return result_str;
		}
		catch (...)
		{
			return {};
		}
	}

	void TorrentCheckModalWindow::OnMetadataParsingCompleted()
	{
		m_metadataReady = true;

		// TODO: Update UI to show file list for user selection:
		// 1. Hide loading indicator (ProgressRing)
		// 2. Show file tree view with all files from the torrent
		// 3. Enable checkboxes for each file for selective download
		// 4. Display total size and selected size
		// 5. Enable "Start Download" button
		// 6. Navigate to TorrentCheckGeneralPage with metadata

		OutputDebugStringW(L"Metadata ready! Ready to display file list for selection.");
	}

	void TorrentCheckModalWindow::OnMetadataParsingFailed(const std::string& errorMessage)
	{
		// TODO: Display error to user in UI:
		// 1. Hide loading indicator
		// 2. Show error message in an InfoBar or MessageDialog
		// 3. Provide option to retry or close the window

		std::string fullError = "Metadata parsing failed: " + errorMessage;
		OutputDebugStringW(winrt::to_hstring(fullError).c_str());

		// Optionally close the window after a delay
		// this->Close();
	}

	// Sets the owner window of the modal window to the main app window.
	void TorrentCheckModalWindow::SetWindowOwner()
	{
		// Owner: main window exposed by App.xaml.h
		auto const& ownerWindow = winrt::OpenNet::implementation::App::window;
		if (!ownerWindow)
		{
			return; // No owner available yet.
		}

		// Get HWND of the owner (main window).
		HWND ownerHwnd = ::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::GetWindowHandleFromWindow(ownerWindow);

		// Get HWND of this modal window's AppWindow.
		auto ownedWindowId = AppWindow().Id();
		HWND ownedHwnd = winrt::Microsoft::UI::GetWindowFromWindowId(ownedWindowId);

		if (ownerHwnd && ownedHwnd)
		{
			// Set the owner relationship so this window becomes modal to the owner.
			::SetWindowLongPtrW(ownedHwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(ownerHwnd));
		}
	}

	void TorrentCheckModalWindow::ModalWindow_Closed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::WindowEventArgs const&)
	{
		// Reactivate the main application window when the modal window closes.
		auto const& ownerWindow = winrt::OpenNet::implementation::App::window;
		if (!ownerWindow)
		{
			return; // No owner available yet.
		}
		ownerWindow.Activate();
	}

	void TorrentCheckModalWindow::TorrentCheckModalWindowSeleterBar_SelectionChanged(
		winrt::Microsoft::UI::Xaml::Controls::SelectorBar const& sender,
		winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const& /*args*/)
	{
		// Get the selected tab index
		auto selectedItem = sender.SelectedItem();
		if (!selectedItem)
		{
			return;
		}

		// Find the index of the selected item in the Items collection
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
		// TODO: Implement page navigation for each tab
		// 0: General - TorrentCheckGeneralPage
		// 1: Snapshots
		// 2: Advanced
		// 3: Publisher
		// 4: Download Order

		if (auto frame = TorrentCheckFrame())
		{
			// TODO: Navigate to the appropriate page with metadata
			// Example:
			// frame.Navigate(winrt::xaml_typename<TorrentCheckGeneralPage>(), m_torrentLink);
		}
	}

	void TorrentCheckModalWindow::TorrentCreateGrid_Loaded(
		winrt::Windows::Foundation::IInspectable const& /*sender*/,
		winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*args*/)
	{
		// Window has been loaded and displayed, NOW safe to start parsing metadata
		// This prevents UI freezing - window displays first, then starts download
		StartParseMetadata();
	}

	void TorrentCheckModalWindow::RootGridXamlRoot_Changed(
		winrt::Microsoft::UI::Xaml::XamlRoot /*sender*/,
		winrt::Microsoft::UI::Xaml::XamlRootChangedEventArgs /*args*/)
	{
		// Handle XamlRoot changes if needed
		// This can be used to adjust window size or content layout
	}
}
