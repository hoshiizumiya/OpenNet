#include "pch.h"
#include "StickyHeaderBehavior.h"
#if __has_include("UI/Xaml/Behaviors/Headers/StickyHeaderBehavior.g.cpp")
#include "UI/Xaml/Behaviors/Headers/StickyHeaderBehavior.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Composition.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Hosting;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Composition;

namespace winrt::OpenNet::UI::Xaml::Behaviors::implementation
{
    void StickyHeaderBehavior::Attach(DependencyObject const& associatedObject)
    {
        if (!associatedObject)
        {
            return;
        }

        m_associatedObject = associatedObject;

        auto element = associatedObject.try_as<FrameworkElement>();
        if (!element)
        {
            return;
        }

        // Find the ScrollViewer ancestor
        DependencyObject current = VisualTreeHelper::GetParent(element);
        while (current)
        {
            if (auto sv = current.try_as<ScrollViewer>())
            {
                m_scrollViewer = sv;
                break;
            }
            current = VisualTreeHelper::GetParent(current);
        }

        if (!m_scrollViewer)
        {
            return;
        }

        // Subscribe to SizeChanged on the header element
        m_sizeChangedToken = element.SizeChanged([this](Windows::Foundation::IInspectable const&, SizeChangedEventArgs const&)
        {
            AssignAnimation();
        });

        // Subscribe to GotFocus on ScrollViewer
        m_gotFocusToken = m_scrollViewer.GotFocus([this](Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&)
        {
            auto scroller = sender.try_as<ScrollViewer>();
            if (!scroller) return;

            auto element = m_associatedObject.try_as<FrameworkElement>();
            if (!element) return;

            auto xamlRoot = scroller.XamlRoot();
            if (!xamlRoot) return;

            auto focusedObj = Input::FocusManager::GetFocusedElement(xamlRoot);
            auto focusedElement = focusedObj.try_as<UIElement>();
            if (focusedElement && VisualTreeHelper::GetParent(focusedElement))
            {
                auto transform = focusedElement.TransformToVisual(scroller);
                auto point = transform.TransformPoint(Windows::Foundation::Point(0, 0));
                if (point.Y < element.ActualHeight())
                {
                    scroller.ChangeView(nullptr, scroller.VerticalOffset() - (element.ActualHeight() - point.Y), nullptr, false);
                }
            }
        });

        // Push items panel behind header
        auto itemsControl = m_scrollViewer.try_as<ItemsControl>();
        if (!itemsControl)
        {
            // Walk up from scrollviewer to find ItemsControl
            DependencyObject parent = VisualTreeHelper::GetParent(m_scrollViewer);
            while (parent)
            {
                if (auto ic = parent.try_as<ItemsControl>())
                {
                    itemsControl = ic;
                    break;
                }
                parent = VisualTreeHelper::GetParent(parent);
            }
        }

        if (itemsControl && itemsControl.ItemsPanelRoot())
        {
            Canvas::SetZIndex(itemsControl.ItemsPanelRoot(), -1);
        }
        else
        {
            Canvas::SetZIndex(element, 1'000'000);
        }

        AssignAnimation();
    }

    void StickyHeaderBehavior::Detach()
    {
        RemoveAnimation();

        if (auto element = m_associatedObject.try_as<FrameworkElement>())
        {
            element.SizeChanged(m_sizeChangedToken);
        }

        if (m_scrollViewer)
        {
            m_scrollViewer.GotFocus(m_gotFocusToken);
        }

        m_associatedObject = nullptr;
        m_scrollViewer = nullptr;
        m_scrollProperties = nullptr;
        m_animationProperties = nullptr;
        m_headerVisual = nullptr;
    }

    void StickyHeaderBehavior::Show()
    {
        if (m_animationProperties)
        {
            m_animationProperties.InsertScalar(L"OffsetY", 0.0f);
        }
    }

    bool StickyHeaderBehavior::AssignAnimation()
    {
        StopAnimation();

        auto element = m_associatedObject.try_as<FrameworkElement>();
        if (!element || element.RenderSize().Height == 0)
        {
            return false;
        }

        if (!m_scrollViewer)
        {
            return false;
        }

        // Get the ScrollViewer manipulation property set (Translation.Y tracks scroll offset)
        m_scrollProperties = ElementCompositionPreview::GetScrollViewerManipulationPropertySet(m_scrollViewer);
        if (!m_scrollProperties)
        {
            return false;
        }

        // Get the header element's composition visual
        m_headerVisual = ElementCompositionPreview::GetElementVisual(element);
        if (!m_headerVisual)
        {
            return false;
        }

        auto compositor = m_scrollProperties.Compositor();

        // Create animation property set with OffsetY scalar
        m_animationProperties = compositor.CreatePropertySet();
        m_animationProperties.InsertScalar(L"OffsetY", 0.0f);

        // Build expression: Max(propSet.OffsetY - scroll.Translation.Y, 0)
        // This keeps the header pinned at the top as content scrolls underneath
        auto expressionAnimation = compositor.CreateExpressionAnimation(
            L"Max(propSet.OffsetY - scroll.Translation.Y, 0)"
        );
        expressionAnimation.SetReferenceParameter(L"propSet", m_animationProperties);
        expressionAnimation.SetReferenceParameter(L"scroll", m_scrollProperties);

        // Start the animation on the header visual's Offset.Y
        m_headerVisual.StartAnimation(L"Offset.Y", expressionAnimation);

        return true;
    }

    void StickyHeaderBehavior::StopAnimation()
    {
        if (m_animationProperties)
        {
            m_animationProperties.InsertScalar(L"OffsetY", 0.0f);
        }

        if (m_headerVisual)
        {
            m_headerVisual.StopAnimation(L"Offset.Y");

            auto offset = m_headerVisual.Offset();
            offset.y = 0.0f;
            m_headerVisual.Offset(offset);
        }
    }

    void StickyHeaderBehavior::RemoveAnimation()
    {
        StopAnimation();
    }
}