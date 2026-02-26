#pragma once

#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.g.h"
#include "Core/torrentCore/TorrentMetadataFetcher.h"
#include "ViewModels/TorrentMetadataViewModel.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/OpenNet.ViewModels.h>
#include <memory>

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct TorrentCheckModalWindow : TorrentCheckModalWindowT<TorrentCheckModalWindow>
	{
		TorrentCheckModalWindow();
		TorrentCheckModalWindow(winrt::hstring const& torrentLink);

		// Properties for XAML binding
		winrt::OpenNet::ViewModels::TorrentMetadataViewModel MetadataViewModel() const { return m_metadataViewModel; }
		bool IsLoading() const { return m_isLoading; }
		winrt::hstring LoadingStatus() const { return m_loadingStatus; }
		int LoadingProgress() const { return m_loadingProgress; }
		bool HasError() const { return m_hasError; }
		winrt::hstring ErrorMessage() const { return m_errorMessage; }
		bool MetadataReady() const { return m_metadataReady; }

		// Event handlers
		void SetWindowOwner();
		void ModalWindow_Closed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::WindowEventArgs const&);
		void TorrentCheckModalWindowSeleterBar_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::SelectorBar const&, winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const&);
		void TorrentCreateGrid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void RootGridXamlRoot_Changed(winrt::Microsoft::UI::Xaml::XamlRoot sender, winrt::Microsoft::UI::Xaml::XamlRootChangedEventArgs args);

		// Button handlers
		void StartDownloadButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void CancelButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void RetryButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		void InitializeWindow();
		void StartParseMetadata();
		winrt::Windows::Foundation::IAsyncAction ParseTorrentMetadataAsync();

		// UI state update helpers (must be called on UI thread)
		void UpdateLoadingState(bool isLoading, winrt::hstring const& status, int progress);
		void ShowError(winrt::hstring const& message);
		void ShowMetadata(::OpenNet::Core::Torrent::TorrentMetadataInfo const& metadata);
		void NavigateToGeneralPage();

		// Start the actual download with current settings
		winrt::Windows::Foundation::IAsyncAction StartDownloadAsync();

		// State and data members
		uint32_t m_selectedTabIndex{};
		winrt::hstring m_torrentLink{};

		// UI state
		bool m_isLoading{ false };
		winrt::hstring m_loadingStatus{};
		int m_loadingProgress{};
		bool m_hasError{ false };
		winrt::hstring m_errorMessage{};
		bool m_metadataReady{ false };

		// ViewModel
		winrt::OpenNet::ViewModels::TorrentMetadataViewModel m_metadataViewModel{ nullptr };

		// Metadata fetcher
		std::unique_ptr<::OpenNet::Core::Torrent::TorrentMetadataFetcher> m_metadataFetcher;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct TorrentCheckModalWindow : TorrentCheckModalWindowT<TorrentCheckModalWindow, implementation::TorrentCheckModalWindow>
	{
	};
}
