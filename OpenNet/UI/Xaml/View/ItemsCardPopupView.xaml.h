#pragma once

#include "UI/Xaml/View/ItemsCardPopupView.g.h"

namespace winrt::OpenNet::UI::Xaml::View::implementation
{
	struct ItemsCardPopupView : ItemsCardPopupViewT<ItemsCardPopupView>
	{
		ItemsCardPopupView();
		void InitializeComponent();

		void OnLoaded(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::fire_and_forget OnOpenWebUIClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnCopyUrlClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnRuntimeStatusClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnNatDetectionClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnGuideClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
	private:
		void RefreshStatus();
		winrt::hstring WebUIUrl() const;
	};
}

namespace winrt::OpenNet::UI::Xaml::View::factory_implementation
{
	struct ItemsCardPopupView : ItemsCardPopupViewT<ItemsCardPopupView, implementation::ItemsCardPopupView>
	{
	};
}
