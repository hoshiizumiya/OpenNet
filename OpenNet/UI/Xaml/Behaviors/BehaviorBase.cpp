#include "pch.h"
#include "BehaviorBase.h"
#if __has_include("UI/Xaml/Behaviors/BehaviorBase.g.cpp")
#include "UI/Xaml/Behaviors/BehaviorBase.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Windows::Foundation;

namespace winrt::OpenNet::UI::Xaml::Behaviors::implementation
{
    void BehaviorBase::Attach(DependencyObject const& associatedObject)
    {
        if (associatedObject == m_associatedObject)
        {
            return;
        }

        if (m_associatedObject)
        {
            throw hresult_invalid_argument(L"Behavior cannot be attached multiple times");
        }

        m_isAttaching = true;

        try
        {
            m_associatedObject = associatedObject;

            // Initialize the behavior
            if (Initialize())
            {
                m_isAttached = true;
                OnAttached();
            }
            else
            {
                m_associatedObject = nullptr;
            }
        }
        catch (...)
        {
            m_isAttaching = false;
            m_associatedObject = nullptr;
            throw;
        }

        m_isAttaching = false;
    }

    void BehaviorBase::Detach()
    {
        if (!m_isAttached)
        {
            return;
        }

        OnDetaching();

        if (Uninitialize())
        {
            m_isAttached = false;
        }

        m_associatedObject = nullptr;
    }

    void BehaviorBase::OnAssociatedObjectLoadedHandler(IInspectable const& sender, RoutedEventArgs const& e)
    {
        if (m_isAttached)
        {
            OnAssociatedObjectLoaded();
        }
    }

    void BehaviorBase::OnAssociatedObjectUnloadedHandler(IInspectable const& sender, RoutedEventArgs const& e)
    {
        if (m_isAttached)
        {
            OnAssociatedObjectUnloaded();
        }
    }
}
