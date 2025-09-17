#pragma once
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/base.h>

namespace OpenNet::ViewModels
{
    // Lightweight mixin for INotifyPropertyChanged without deriving from winrt::implements
    // CRTP: D should be the derived type (e.g., SettingsViewModel)
    template <typename D>
    struct ObservableMixin
    {
    public:
        // INotifyPropertyChanged implementation surface expected by C++/WinRT projection
        winrt::event_token PropertyChanged(winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }

        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

    protected:
        template <typename T>
        bool SetProperty(T& storage, T const& value, winrt::hstring const& propertyName)
        {
            if (storage == value)
            {
                return false;
            }
            storage = value;
            RaisePropertyChanged(propertyName);
            return true;
        }

        void RaisePropertyChanged(winrt::hstring const& propertyName)
        {
            // Sender is the derived instance
            m_propertyChanged(*static_cast<D*>(this),
                winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ propertyName });
        }

    private:
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}
