#pragma once

#include "UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.h"
#include <winrt/OpenNet.ViewModels.h>

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TorrentCheckGeneralPage : TorrentCheckGeneralPageT<TorrentCheckGeneralPage>
	{
		TorrentCheckGeneralPage();
		winrt::OpenNet::ViewModels::TorrentMetadataViewModel ViewModel() const { return m_viewModel; }

		// Static helpers for XAML binding to get file/folder icon and font family
		static winrt::hstring GetNodeIcon(bool isFolder);
		static winrt::hstring GetNodeIcon(bool isFolder, winrt::hstring const& fileName);
		static winrt::Microsoft::UI::Xaml::Media::FontFamily GetNodeFontFamily(bool isFolder);
		static winrt::Microsoft::UI::Xaml::Media::FontFamily GetNodeFontFamily(bool isFolder, winrt::hstring const& fileName);

		// Navigation
		void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

		// Event handlers
		winrt::Windows::Foundation::IAsyncAction TorrentCheckGeneralPageFolderPicker_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::Windows::Foundation::IAsyncOperation<hstring> PickFolderAsync();

		// Selection filter handlers
		void SelectAllCheckBox_Checked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void SelectAllCheckBox_Unchecked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void FilterCheckBox_Checked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
		void FilterCheckBox_Unchecked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		// TreeView selection handler
		void TorrentFileTreeView_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::TreeView const& sender, winrt::Microsoft::UI::Xaml::Controls::TreeViewSelectionChangedEventArgs const& args);

		// File checkbox click handler
		void FileCheckBox_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		// Save path changed handler
		void TorrentCheckGeneralPageSavePath_TextChanged(winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& sender, winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& args);

	private:
		winrt::OpenNet::ViewModels::TorrentMetadataViewModel m_viewModel{ nullptr };

		// Cached disk info for progress bar
		uint64_t m_totalDiskBytes{ 0 };
		uint64_t m_freeDiskBytes{ 0 };

		// Helper to apply filters
		void ApplyFilters();
		void UpdateSelectedSizeDisplay();
		template<typename T>
		void UpdateDiskSpaceDisplay(const T& savePath);

		// Known extensions for filtering
		static constexpr std::wstring_view VideoExtensions = L".mp4,.mkv,.avi,.mov,.wmv";
		static constexpr std::wstring_view AudioExtensions = L".mp3,.flac,.wav,.aac,.ogg";
		static constexpr std::wstring_view PictureExtensions = L".jpg,.jpeg,.png,.gif,.bmp,.webp";
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TorrentCheckGeneralPage : TorrentCheckGeneralPageT<TorrentCheckGeneralPage, implementation::TorrentCheckGeneralPage>
	{
	};
}
