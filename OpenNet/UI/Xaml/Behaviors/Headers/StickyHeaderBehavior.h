#pragma once

#include "UI/Xaml/Behaviors/StickyHeaderBehavior.g.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Composition.h>

namespace winrt::OpenNet::UI::Xaml::Behaviors::implementation
{
    struct StickyHeaderBehavior : StickyHeaderBehaviorT<StickyHeaderBehavior>
    {
        StickyHeaderBehavior() = default;

        void Attach(winrt::Microsoft::UI::Xaml::DependencyObject const& associatedObject);
        void Detach();
        winrt::Microsoft::UI::Xaml::DependencyObject AssociatedObject() const { return m_associatedObject; }
        void Show();

    private:
        bool AssignAnimation();
        void StopAnimation();
        void RemoveAnimation();

        winrt::Microsoft::UI::Xaml::DependencyObject m_associatedObject{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewer m_scrollViewer{ nullptr };
        Microsoft::UI::Composition::CompositionPropertySet m_scrollProperties{ nullptr };
        Microsoft::UI::Composition::CompositionPropertySet m_animationProperties{ nullptr };
        Microsoft::UI::Composition::Visual m_headerVisual{ nullptr };

        winrt::event_token m_sizeChangedToken{};
        winrt::event_token m_gotFocusToken{};
    };
}

namespace winrt::OpenNet::UI::Xaml::Behaviors::factory_implementation
{
    struct StickyHeaderBehavior : StickyHeaderBehaviorT<StickyHeaderBehavior, implementation::StickyHeaderBehavior>
    {
    };
}
