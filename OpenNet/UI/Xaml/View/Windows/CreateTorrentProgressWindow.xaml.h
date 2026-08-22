#pragma once

#include "UI/Xaml/View/Windows/CreateTorrentProgressWindow.g.h"

import OpenNet.Core.Torrent.TorrentCreator;

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct CreateTorrentProgressWindow : CreateTorrentProgressWindowT<CreateTorrentProgressWindow>
	{
		CreateTorrentProgressWindow();
		CreateTorrentProgressWindow(::OpenNet::Core::Torrent::TorrentCreationOptions options, std::filesystem::path targetPath, bool startSeeding);
		void RootGrid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void CancelButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void CloseButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
		void OpenFolderButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

	private:
		void InitializeWindow();
		winrt::Windows::Foundation::IAsyncAction StartCreationAsync();
		void UpdateProgress(int completedPieces, int totalPieces);
		void Complete(winrt::hstring const& message, bool success);

		::OpenNet::Core::Torrent::TorrentCreationOptions m_options;
		std::filesystem::path m_targetPath;
		std::stop_source m_stopSource;
		winrt::Windows::Foundation::IAsyncAction m_creationAction{ nullptr };
		bool m_startSeeding{};
		bool m_started{};
		bool m_completed{};
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct CreateTorrentProgressWindow : CreateTorrentProgressWindowT<CreateTorrentProgressWindow, implementation::CreateTorrentProgressWindow>
	{
	};
}
