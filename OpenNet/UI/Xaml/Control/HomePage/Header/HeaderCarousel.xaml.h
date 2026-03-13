#pragma once

#include "UI/Xaml/Control/HomePage/Header/HeaderCarousel.g.h"

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::implementation
{
    struct HeaderCarousel : HeaderCarouselT<HeaderCarousel>
    {
        HeaderCarousel();

        void UserControl_Loaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void UserControl_Unloaded(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        void SubscribeToEvents();
        void UnsubscribeToEvents();

        void SelectionTimer_Tick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& e);
        void DeselectionTimer_Tick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& e);
        void SelectNextTile();
        void SetTileVisuals();
        void AnimateTitleGradient(winrt::Microsoft::UI::Xaml::Media::LinearGradientBrush const& brush);

        void Tile_PointerEntered(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void Tile_PointerExited(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void Tile_GotFocus(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void Tile_LostFocus(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void Tile_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

        void SelectTile();
        void ResetAndShuffle();
        int GetNextUniqueRandom();

        winrt::Microsoft::UI::Xaml::DispatcherTimer m_selectionTimer;
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_deselectionTimer;
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_scrollDelayTimer;
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_selectDelayTimer;
        winrt::OpenNet::UI::Xaml::Control::HomePage::Header::HeaderTile m_selectedTile{ nullptr };

        std::mt19937 m_rng{ std::random_device{}() };
        std::vector<int> m_numbers;
        int m_currentIndex{ 0 };

        // Event tokens for tile subscriptions
        struct TileTokens
        {
            winrt::event_token pointerEntered;
            winrt::event_token pointerExited;
            winrt::event_token gotFocus;
            winrt::event_token lostFocus;
            winrt::event_token click;
        };
        std::vector<TileTokens> m_tileTokens;
        winrt::event_token m_selectionTimerToken{};
        winrt::event_token m_deselectionTimerToken{};
        winrt::event_token m_scrollDelayTimerToken{};
        winrt::event_token m_selectDelayTimerToken{};

        bool m_isLoaded{ false };
        bool m_eventsSubscribed{ false };
        bool m_isNavigatingFromTileClick{ false };
    };
}

namespace winrt::OpenNet::UI::Xaml::Control::HomePage::Header::factory_implementation
{
    struct HeaderCarousel : HeaderCarouselT<HeaderCarousel, implementation::HeaderCarousel>
    {
    };
}
