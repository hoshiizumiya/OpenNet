#pragma once

#include "UI/Xaml/View/Windows/TorrentCheckModalWindow.g.h"

namespace winrt::OpenNet::UI::Xaml::View::Windows::implementation
{
	struct TorrentCheckModalWindow : TorrentCheckModalWindowT<TorrentCheckModalWindow>
	{
		TorrentCheckModalWindow();

		void SetWindowOwner();
		void ModalWindow_Closed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::WindowEventArgs const&);

		void TorrentCheckModalWindowSeleterBar_SelectionChanged(winrt::Microsoft::UI::Xaml::Controls::SelectorBar const&, winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const&);
		void TorrentCreateGrid_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void RootGridXamlRoot_Changed(winrt::Microsoft::UI::Xaml::XamlRoot sender, winrt::Microsoft::UI::Xaml::XamlRootChangedEventArgs args);


	private:
		uint32_t m_selected_index{};
	};
}

namespace winrt::OpenNet::UI::Xaml::View::Windows::factory_implementation
{
	struct TorrentCheckModalWindow : TorrentCheckModalWindowT<TorrentCheckModalWindow, implementation::TorrentCheckModalWindow>
	{
	};
}
