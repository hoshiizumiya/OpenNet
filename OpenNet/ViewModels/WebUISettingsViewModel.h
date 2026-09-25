#pragma once

#include "ViewModels/WebUISettingsViewModel.g.h"

import OpenNet.ViewModels.ObservableMixin;

namespace winrt::OpenNet::ViewModels::implementation
{
    struct WebUISettingsViewModel :
        WebUISettingsViewModelT<WebUISettingsViewModel>,
        ::OpenNet::ViewModels::ObservableMixin<WebUISettingsViewModel>
    {
        WebUISettingsViewModel() = default;

        bool Enabled() const noexcept
        {
            return m_enabled;
        }

        void Enabled(bool value)
        {
            SetProperty(m_enabled, value, L"Enabled");
        }

        winrt::hstring ApiKey() const
        {
            return m_apiKey;
        }

        void ApiKey(winrt::hstring const& value)
        {
            if (SetProperty(m_apiKey, value, L"ApiKey"))
                RaisePropertyChanged(L"HasApiKey");
        }

        bool HasApiKey() const noexcept
        {
            return !m_apiKey.empty();
        }

        bool IsRunning() const noexcept
        {
            return m_isRunning;
        }

        void Load();
        void RefreshRuntimeState();

    private:
        using ::OpenNet::ViewModels::ObservableMixin<WebUISettingsViewModel>::SetProperty;
        using ::OpenNet::ViewModels::ObservableMixin<WebUISettingsViewModel>::RaisePropertyChanged;

        bool m_enabled{ true };
        bool m_isRunning{};
        winrt::hstring m_apiKey;
    };
}

namespace winrt::OpenNet::ViewModels::factory_implementation
{
    struct WebUISettingsViewModel :
        WebUISettingsViewModelT<WebUISettingsViewModel, implementation::WebUISettingsViewModel>
    {
    };
}
