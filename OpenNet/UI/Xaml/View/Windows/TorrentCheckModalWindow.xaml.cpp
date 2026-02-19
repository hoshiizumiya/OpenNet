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

		// Start parsing metadata when window is initialized with a torrent link
		StartParseMetadata();
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

		// Make the window modal and show it.
		if (auto presenter = AppWindow().Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
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
		ParseTorrentMetadataAsync().Completed([](auto const&, auto const&) {});
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
				// Give the core a moment to start by using std::this_thread::sleep_for
				// Note: In production, use proper async delay mechanisms
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}

			OnMetadataParsingProgress("Downloading torrent metadata...");

			// Use a default save path for metadata-only operation
			std::string tempSavePath = R"(C:\Temp\TorrentCheck)";

			// Add the torrent with flags to only fetch metadata without downloading content
			// This will:
			// 1. Download the metadata
			// 2. Pause immediately after metadata is ready
			bool success = torrentCore->AddMagnet(torrentLink, tempSavePath);

			if (!success)
			{
				OnMetadataParsingFailed("Failed to initiate metadata download");
				m_isParsingMetadata = false;
				co_return;
			}

			// Wait for metadata to be ready (polling with timeout)
			// In production, this should be event-driven via callbacks
			int maxAttempts = 120; // 60 seconds with 500ms sleep
			int attempts = 0;

			while (attempts < maxAttempts && !m_metadataReady)
			{
				// Sleep for 500ms in background thread
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
		catch (std::exception const& ex)
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
		// TODO: Update UI with status message
		// This can be connected to a TextBlock or ProgressRing in XAML
		// For now, just output to debug
		OutputDebugStringW(winrt::to_hstring(status).c_str());
	}

	void TorrentCheckModalWindow::OnMetadataParsingCompleted()
	{
		m_metadataReady = true;

		// TODO: Update UI to show file list for user selection
		// Example:
		// - Hide loading indicator
		// - Show file tree view
		// - Enable "Start Download" button
		// - Navigate to the general page with metadata

		OnMetadataParsingProgress("Metadata ready! Select files to download.");
	}

	void TorrentCheckModalWindow::OnMetadataParsingFailed(const std::string& errorMessage)
	{
		// TODO: Display error to user in UI
		// Example:
		// - Show error dialog
		// - Update status text
		// - Close window or reset state

		std::string fullError = "Metadata parsing failed: " + errorMessage;
		OutputDebugStringW(winrt::to_hstring(fullError).c_str());
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


	void TorrentCheckModalWindow::TorrentCheckModalWindowSeleterBar_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::SelectorBar const& sender, winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const& /*args*/)
	{
		auto item = sender.SelectedItem();
		uint32_t currentSelectedIndex;
		sender.Items().IndexOf(item, currentSelectedIndex);
		SlideNavigationTransitionInfo slideInfo{};


		switch (currentSelectedIndex)
		{
		case 0:
			TorrentCheckFrame().Navigate(xaml_typename<TorrentCheckGeneralPage>(), nullptr, slideInfo);
			break;
		case 1:
			//TorrentCheckFrame().Navigate(xaml_typename<>(), nullptr, slideInfo);
			break;
		default:
			break;
		}

		// This is also working to determine slide direction
		if (currentSelectedIndex > m_selected_index)
		{
			slideInfo.Effect(SlideNavigationTransitionEffect::FromRight);
		}
		else if (currentSelectedIndex < m_selected_index)
		{
			slideInfo.Effect(SlideNavigationTransitionEffect::FromLeft);
		}
		m_selected_index = currentSelectedIndex;

	}

	void TorrentCheckModalWindow::TorrentCreateGrid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& /*e*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);

		if (auto rootGrid = sender.try_as<FrameworkElement>())
		{
			if (auto xamlRoot = rootGrid.XamlRoot())
			{
				xamlRoot.Changed(
					{
						this, & TorrentCheckModalWindow::RootGridXamlRoot_Changed
					}
				);
			}
		}

	}

	void TorrentCheckModalWindow::RootGridXamlRoot_Changed(XamlRoot /*sender*/, XamlRootChangedEventArgs /*args*/)
	{
		::OpenNet::Helpers::WinUIWindowHelper::WindowHelper::SetWindowMinSize(*this, 640, 500);
	}

}
