#pragma once

#include "UI/Xaml/View/Pages/TorrentCheckGeneralPage.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Pages::implementation
{
	struct TorrentCheckGeneralPage : TorrentCheckGeneralPageT<TorrentCheckGeneralPage>
	{
		TorrentCheckGeneralPage();

		winrt::Windows::Foundation::IAsyncAction TorrentCheckGeneralPageFolderPicker_Click(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

		winrt::Windows::Foundation::IAsyncOperation<hstring> PickFolderAsync();
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Pages::factory_implementation
{
	struct TorrentCheckGeneralPage : TorrentCheckGeneralPageT<TorrentCheckGeneralPage, implementation::TorrentCheckGeneralPage>
	{
	};
}
