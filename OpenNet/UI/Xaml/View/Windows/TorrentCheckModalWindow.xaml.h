#pragma once

#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.g.h"
#include <libtorrent/fwd.hpp>
#include <winrt/Windows.Foundation.h>

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct TorrentCheckModalWindow : TorrentCheckModalWindowT<TorrentCheckModalWindow>
	{
		TorrentCheckModalWindow();
		TorrentCheckModalWindow(winrt::hstring const& torrentLink);

		void SetWindowOwner();
		void ModalWindow_Closed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::WindowEventArgs const&);

		void TorrentCheckModalWindowSeleterBar_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::SelectorBar const&, winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const&);
		void TorrentCreateGrid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void RootGridXamlRoot_Changed(winrt::Microsoft::UI::Xaml::XamlRoot sender, winrt::Microsoft::UI::Xaml::XamlRootChangedEventArgs args);

	private:
		void InitializeWindow();
		void StartParseMetadata();
		winrt::Windows::Foundation::IAsyncAction ParseTorrentMetadataAsync();

		// Callback methods for metadata parsing progress
		void OnMetadataParsingProgress(const std::string& status);
		void OnMetadataParsingCompleted();
		void OnMetadataParsingFailed(const std::string& errorMessage);

		// Get valid temporary directory
		static std::string GetTempDirectory();

		// State and data members
		uint32_t m_selectedTabIndex{};
		winrt::hstring m_torrentLink{};
		std::string m_magnetUri{};
		bool m_metadataReady{ false };
		bool m_isParsingMetadata{ false };
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct TorrentCheckModalWindow : TorrentCheckModalWindowT<TorrentCheckModalWindow, implementation::TorrentCheckModalWindow>
	{
	};
}
