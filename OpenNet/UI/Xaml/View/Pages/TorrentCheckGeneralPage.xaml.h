#pragma once

#include "UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.h"
#include <winrt/OpenNet.ViewModels.h>

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TorrentCheckGeneralPage : TorrentCheckGeneralPageT<TorrentCheckGeneralPage>
	{
		TorrentCheckGeneralPage();

		// ViewModel property
		winrt::OpenNet::ViewModels::TorrentMetadataViewModel ViewModel() const { return m_viewModel; }

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

		// Helper to apply filters
		void ApplyFilters();
		void UpdateSelectedSizeDisplay();
		void UpdateDiskSpaceDisplay(const hstring& savePath);

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
