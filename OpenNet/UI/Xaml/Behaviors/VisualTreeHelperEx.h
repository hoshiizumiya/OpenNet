#pragma once

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::OpenNet::UI::Xaml::Behaviors
{
    /// <summary>
    /// Helper methods for working with the visual tree.
    /// </summary>
    struct VisualTreeHelperEx
    {
        /// <summary>
        /// Find an ancestor of a given type in the visual tree.
        /// </summary>
        template<typename T>
        static T FindAscendant(Microsoft::UI::Xaml::UIElement const& element)
        {
            auto parent = Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(element);
            while (parent)
            {
                if (auto target = parent.try_as<T>())
                {
                    return target;
                }
                auto uiElement = parent.try_as<Microsoft::UI::Xaml::UIElement>();
                if (!uiElement)
                {
                    break;
                }
                parent = Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(uiElement);
            }
            return nullptr;
        }
    };
}
