#pragma once

#include "UI/Xaml/Behaviors/BehaviorBase.g.h"
#include <winrt/Microsoft.UI.Xaml.h>

namespace winrt::OpenNet::UI::Xaml::Behaviors::implementation
{
    struct BehaviorBase : BehaviorBaseT<BehaviorBase>
    {
        BehaviorBase() = default;

        /// <summary>
        /// Gets the DependencyObject to which the behavior is attached.
        /// </summary>
        winrt::Microsoft::UI::Xaml::DependencyObject AssociatedObject() const { return m_associatedObject; }

        /// <summary>
        /// Attaches the behavior to the specified DependencyObject.
        /// </summary>
        void Attach(winrt::Microsoft::UI::Xaml::DependencyObject const& associatedObject);

        /// <summary>
        /// Detaches the behavior from its associated object.
        /// </summary>
        void Detach();

    protected:
        /// <summary>
        /// Gets a value indicating whether this behavior is attached.
        /// </summary>
        bool IsAttached() const { return m_isAttached; }

        /// <summary>
        /// Called after the behavior is attached to the associated object.
        /// Override this to hook up functionality to the associated object.
        /// </summary>
        virtual void OnAttached() {}

        /// <summary>
        /// Called when the behavior is being detached from its associated object.
        /// Override this to unhook functionality from the associated object.
        /// </summary>
        virtual void OnDetaching() {}

        /// <summary>
        /// Called when the associated object has been loaded.
        /// </summary>
        virtual void OnAssociatedObjectLoaded() {}

        /// <summary>
        /// Called when the associated object has been unloaded.
        /// </summary>
        virtual void OnAssociatedObjectUnloaded() {}

        /// <summary>
        /// Initializes the behavior to the associated object.
        /// </summary>
        /// <returns>true if the initialization succeeded; otherwise false.</returns>
        virtual bool Initialize() { return true; }

        /// <summary>
        /// Uninitializes the behavior from the associated object.
        /// </summary>
        /// <returns>true if uninitialization succeeded; otherwise false.</returns>
        virtual bool Uninitialize() { return true; }

    private:
        winrt::Microsoft::UI::Xaml::DependencyObject m_associatedObject{ nullptr };
        bool m_isAttaching = false;
        bool m_isAttached = false;

        void OnAssociatedObjectLoadedHandler(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnAssociatedObjectUnloadedHandler(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
    };
}

namespace winrt::OpenNet::UI::Xaml::Behaviors::factory_implementation
{
    struct BehaviorBase : BehaviorBaseT<BehaviorBase, implementation::BehaviorBase>
    {
    };
}
