#pragma once

#include "UI/Xaml/Behaviors/HeaderBehaviorBase.g.h"
#include "../BehaviorBase.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Composition.h>

namespace winrt::OpenNet::UI::Xaml::Behaviors::implementation
{
    struct HeaderBehaviorBase : HeaderBehaviorBaseT<HeaderBehaviorBase>
    {
        HeaderBehaviorBase() = default;

    protected:
        // From Doc: https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.controls.canvas.zindex
        static constexpr int32_t CanvasZIndexMax = 1'000'000;

        /// <summary>
        /// The ScrollViewer associated with the ListViewBase control.
        /// </summary>
        Microsoft::UI::Xaml::Controls::ScrollViewer m_scrollViewer{ nullptr };

        /// <summary>
        /// The CompositionPropertySet associated with the ScrollViewer.
        /// </summary>
        Microsoft::UI::Composition::CompositionPropertySet m_scrollProperties{ nullptr };

        /// <summary>
        /// The CompositionPropertySet associated with the animation.
        /// </summary>
        Microsoft::UI::Composition::CompositionPropertySet m_animationProperties{ nullptr };

        /// <summary>
        /// The Visual associated with the header element.
        /// </summary>
        Microsoft::UI::Composition::Visual m_headerVisual{ nullptr };

        /// <summary>
        /// Initializes the behavior to the associated object.
        /// </summary>
        /// <returns>true if the initialization succeeded; otherwise false.</returns>
        virtual bool Initialize();

        /// <summary>
        /// Uses Composition API to get the UIElement and sets an ExpressionAnimation.
        /// </summary>
        /// <returns>true if the assignment was successful; otherwise false.</returns>
        virtual bool AssignAnimation();

        /// <summary>
        /// Stop the animation of the UIElement.
        /// </summary>
        virtual void StopAnimation();

        /// <summary>
        /// Remove the animation from the UIElement.
        /// </summary>
        virtual void RemoveAnimation();

    private:
        winrt::event_token m_sizeChangedToken{};
        winrt::event_token m_gotFocusToken{};

        void ScrollHeader_SizeChanged(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::SizeChangedEventArgs const& e);
        void ScrollViewer_GotFocus(winrt::Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
    };
}

namespace winrt::OpenNet::UI::Xaml::Behaviors::factory_implementation
{
    struct HeaderBehaviorBase : HeaderBehaviorBaseT<HeaderBehaviorBase, implementation::HeaderBehaviorBase>
    {
    };
}
